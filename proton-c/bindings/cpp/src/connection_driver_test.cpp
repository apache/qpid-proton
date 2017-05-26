/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "test_bits.hpp"
#include "proton_bits.hpp"

#include "proton/container.hpp"
#include "proton/io/connection_driver.hpp"
#include "proton/io/link_namer.hpp"
#include "proton/link.hpp"
#include "proton/message.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/source_options.hpp"
#include "proton/types_fwd.hpp"
#include "proton/uuid.hpp"

#include <deque>
#include <algorithm>

namespace {

using namespace std;
using namespace proton;

using proton::io::connection_driver;
using proton::io::const_buffer;
using proton::io::mutable_buffer;

typedef std::deque<char> byte_stream;

static const int MAX_SPIN = 1000; // Give up after 1000 event-less dispatches

/// In memory connection_driver that reads and writes from byte_streams
struct in_memory_driver : public connection_driver {

    byte_stream& reads;
    byte_stream& writes;
    int spinning;

    in_memory_driver(byte_stream& rd, byte_stream& wr) :
        reads(rd), writes(wr), spinning(0) {}

    void do_read() {
        mutable_buffer rbuf = read_buffer();
        size_t size = std::min(reads.size(), rbuf.size);
        if (size) {
            copy(reads.begin(), reads.begin()+size, static_cast<char*>(rbuf.data));
            read_done(size);
            reads.erase(reads.begin(), reads.begin()+size);
        }
    }

    void do_write() {
        const_buffer wbuf = write_buffer();
        if (wbuf.size) {
            writes.insert(writes.begin(),
                          static_cast<const char*>(wbuf.data),
                          static_cast<const char*>(wbuf.data) + wbuf.size);
            write_done(wbuf.size);
        }
    }

    void check_idle() {
        spinning = has_events() ? 0 : spinning+1;
        if (spinning > MAX_SPIN)
            throw test::error("no activity, interrupting test");
    }

    void process() {
        check_idle();
        if (!dispatch())
            throw test::error("unexpected close: "+connection().error().what());
        do_read();
        do_write();
        check_idle();
        dispatch();
    }
};

/// A pair of drivers that talk to each other in-memory, simulating a connection.
struct driver_pair {
    byte_stream ab, ba;
    in_memory_driver a, b;

    driver_pair(const connection_options& oa, const connection_options& ob)
        : a(ba, ab), b(ab, ba)
    {
        a.connect(oa);
        b.accept(ob);
    }

    void process() { a.process(); b.process(); }
};

template <class S> typename S::value_type quick_pop(S& s) {
    ASSERT(!s.empty());
    typename S::value_type x = s.front();
    s.pop_front();
    return x;
}

/// A handler that records incoming endpoints, errors etc.
struct record_handler : public messaging_handler {
    std::deque<proton::receiver> receivers;
    std::deque<proton::sender> senders;
    std::deque<proton::session> sessions;
    std::deque<std::string> unhandled_errors, transport_errors, connection_errors;
    std::deque<proton::message> messages;

    void on_receiver_open(receiver &l) PN_CPP_OVERRIDE {
        receivers.push_back(l);
    }

    void on_sender_open(sender &l) PN_CPP_OVERRIDE {
        senders.push_back(l);
    }

    void on_session_open(session &s) PN_CPP_OVERRIDE {
        sessions.push_back(s);
    }

    void on_transport_error(transport& t) PN_CPP_OVERRIDE {
        transport_errors.push_back(t.error().what());
    }

    void on_connection_error(connection& c) PN_CPP_OVERRIDE {
        connection_errors.push_back(c.error().what());
    }

    void on_error(const proton::error_condition& c) PN_CPP_OVERRIDE {
        unhandled_errors.push_back(c.what());
    }

    void on_message(proton::delivery&, proton::message& m) PN_CPP_OVERRIDE {
        messages.push_back(m);
    }
};

struct namer : public io::link_namer {
    char name;
    namer(char c) : name(c) {}
    std::string link_name() { return std::string(1, name++); }
};

void test_driver_link_id() {
    record_handler ha, hb;
    driver_pair d(ha, hb);
    d.a.connect(ha);
    d.b.accept(hb);

    namer na('x');
    namer nb('b');
    connection ca = d.a.connection();
    connection cb = d.b.connection();
    set_link_namer(ca, na);
    set_link_namer(cb, nb);

    d.b.connection().open();

    d.a.connection().open_sender("foo");
    while (ha.senders.empty() || hb.receivers.empty()) d.process();
    sender s = quick_pop(ha.senders);
    ASSERT_EQUAL("x", s.name());

    ASSERT_EQUAL("x", quick_pop(hb.receivers).name());

    d.a.connection().open_receiver("bar");
    while (ha.receivers.empty() || hb.senders.empty()) d.process();
    ASSERT_EQUAL("y", quick_pop(ha.receivers).name());
    ASSERT_EQUAL("y", quick_pop(hb.senders).name());

    d.b.connection().open_receiver("");
    while (ha.senders.empty() || hb.receivers.empty()) d.process();
    ASSERT_EQUAL("b", quick_pop(ha.senders).name());
    ASSERT_EQUAL("b", quick_pop(hb.receivers).name());
}

void test_endpoint_close() {
    record_handler ha, hb;
    driver_pair d(ha, hb);
    d.a.connection().open_sender("x");
    d.a.connection().open_receiver("y");
    while (ha.senders.size()+ha.receivers.size() < 2 ||
           hb.senders.size()+hb.receivers.size() < 2) d.process();
    proton::link ax = quick_pop(ha.senders), ay = quick_pop(ha.receivers);
    proton::link bx = quick_pop(hb.receivers), by = quick_pop(hb.senders);

    // Close a link
    ax.close(proton::error_condition("err", "foo bar"));
    while (!bx.closed()) d.process();
    proton::error_condition c = bx.error();
    ASSERT_EQUAL("err", c.name());
    ASSERT_EQUAL("foo bar", c.description());
    ASSERT_EQUAL("err: foo bar", c.what());

    // Close a link with an empty condition
    ay.close(proton::error_condition());
    while (!by.closed()) d.process();
    ASSERT(by.error().empty());

    // Close a connection
    connection ca = d.a.connection(), cb = d.b.connection();
    ca.close(proton::error_condition("conn", "bad connection"));
    while (!cb.closed()) d.process();
    ASSERT_EQUAL("conn: bad connection", cb.error().what());
    ASSERT_EQUAL(1u, hb.connection_errors.size());
    ASSERT_EQUAL("conn: bad connection", hb.connection_errors.front());
}

void test_driver_disconnected() {
    // driver.disconnected() aborts the connection and calls the local on_transport_error()
    record_handler ha, hb;
    driver_pair d(ha, hb);
    d.a.connect(ha);
    d.b.accept(hb);
    while (!d.a.connection().active() || !d.b.connection().active())
        d.process();

    // Close a with an error condition. The AMQP connection is still open.
    d.a.disconnected(proton::error_condition("oops", "driver failure"));
    ASSERT(!d.a.dispatch());
    ASSERT(!d.a.connection().closed());
    ASSERT(d.a.connection().error().empty());
    ASSERT_EQUAL(0u, ha.connection_errors.size());
    ASSERT_EQUAL("oops: driver failure", d.a.transport().error().what());
    ASSERT_EQUAL(1u, ha.transport_errors.size());
    ASSERT_EQUAL("oops: driver failure", ha.transport_errors.front());

    // In a real app the IO code would detect the abort and do this:
    d.b.disconnected(proton::error_condition("broken", "it broke"));
    ASSERT(!d.b.dispatch());
    ASSERT(!d.b.connection().closed());
    ASSERT(d.b.connection().error().empty());
    ASSERT_EQUAL(0u, hb.connection_errors.size());
    // Proton-C adds (connection aborted) if transport closes too early,
    // and provides a default message if there is no user message.
    ASSERT_EQUAL("broken: it broke (connection aborted)", d.b.transport().error().what());
    ASSERT_EQUAL(1u, hb.transport_errors.size());
    ASSERT_EQUAL("broken: it broke (connection aborted)", hb.transport_errors.front());
}

void test_no_container() {
    // An driver with no container should throw, not crash.
    connection_driver d;
    try {
        d.connection().container();
        FAIL("expected error");
    } catch (proton::error) {}
}

void test_spin_interrupt() {
    // Check the test framework interrupts a spinning driver pair with nothing to do.
    record_handler ha, hb;
    driver_pair d(ha, hb);
    try {
        while (true)
            d.process();
        FAIL("expected exception");
    } catch (test::error) {}
}

void test_link_filters() {
    // Propagation of link properties
    record_handler ha, hb;
    driver_pair d(ha, hb);

    source_options opts;
    source::filter_map f;
    f.put("xx", "xxx");
    ASSERT_EQUAL(1U, f.size());
    d.a.connection().open_sender("x", sender_options().source(source_options().filters(f)));

    f.clear();
    f.put("yy", "yyy");
    ASSERT_EQUAL(1U, f.size());
    d.a.connection().open_receiver("y", receiver_options().source(source_options().filters(f)));

    while (ha.senders.size()+ha.receivers.size() < 2 ||
           hb.senders.size()+hb.receivers.size() < 2)
        d.process();

    proton::sender ax = quick_pop(ha.senders);
    proton::receiver ay = quick_pop(ha.receivers);
    proton::receiver bx = quick_pop(hb.receivers);
    proton::sender by = quick_pop(hb.senders);

    // C++ binding only gives remote_source so only look at remote ends of links
    f = by.source().filters();
    ASSERT_EQUAL(1U, f.size());
    ASSERT_EQUAL(value("yyy"), f.get("yy"));

    f = bx.source().filters();
    ASSERT_EQUAL(1U, f.size());
    ASSERT_EQUAL(value("xxx"), bx.source().filters().get("xx"));
}

void test_message() {
    // Verify a message arrives intact
    record_handler ha, hb;
    driver_pair d(ha, hb);

    proton::sender s = d.a.connection().open_sender("x");
    proton::message m("barefoot");
    m.properties().put("x", "y");
    m.message_annotations().put("a", "b");
    s.send(m);

    while (hb.messages.size() == 0)
        d.process();

    proton::message m2 = quick_pop(hb.messages);
    ASSERT_EQUAL(value("barefoot"), m2.body());
    ASSERT_EQUAL(value("y"), m2.properties().get("x"));
    ASSERT_EQUAL(value("b"), m2.message_annotations().get("a"));
}

}

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_driver_link_id());
    RUN_ARGV_TEST(failed, test_endpoint_close());
    RUN_ARGV_TEST(failed, test_driver_disconnected());
    RUN_ARGV_TEST(failed, test_no_container());
    RUN_ARGV_TEST(failed, test_spin_interrupt());
    RUN_ARGV_TEST(failed, test_message());
    RUN_ARGV_TEST(failed, test_link_filters());
    return failed;
}
