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
#include <proton/uuid.hpp>
#include <proton/io/connection_engine.hpp>
#include <proton/handler.hpp>
#include <proton/types_fwd.hpp>
#include <proton/link.hpp>
#include <deque>
#include <algorithm>

#if __cplusplus < 201103L
#define override
#endif

using namespace proton::io;
using namespace proton;
using namespace test;
using namespace std;

typedef std::deque<char> byte_stream;

/// In memory connection_engine that reads and writes from byte_streams
struct in_memory_engine : public connection_engine {

    byte_stream& reads;
    byte_stream& writes;

    in_memory_engine(byte_stream& rd, byte_stream& wr, handler &h,
                     const connection_options &opts = connection_options()) :
        connection_engine(h, opts), reads(rd), writes(wr) {}

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

    void process() { do_read(); do_write(); dispatch(); }
};

/// A pair of engines that talk to each other in-memory.
struct engine_pair {
    byte_stream ab, ba;
    connection_engine::container cont;

    in_memory_engine a, b;

    engine_pair(handler& ha, handler& hb,
                const connection_options& ca = connection_options(),
                const connection_options& cb = connection_options()) :
        a(ba, ab, ha, ca), b(ab, ba, hb, cb) {}

    void process() { a.process(); b.process(); }
};

template <class S> typename S::value_type quick_pop(S& s) {
    ASSERT(!s.empty());
    typename S::value_type x = s.front();
    s.pop_front();
    return x;
}

/// A handler that records incoming endpoints, errors etc.
struct record_handler : public handler {
    std::deque<proton::receiver> receivers;
    std::deque<proton::sender> senders;
    std::deque<proton::session> sessions;
    std::deque<std::string> unhandled_errors, transport_errors, connection_errors;

    void on_receiver_open(receiver &l) override {
        receivers.push_back(l);
    }

    void on_sender_open(sender &l) override {
        senders.push_back(l);
    }

    void on_session_open(session &s) override {
        sessions.push_back(s);
    }

    void on_transport_error(transport& t) override {
        transport_errors.push_back(t.error().what());
    }

    void on_connection_error(connection& c) override {
        connection_errors.push_back(c.error().what());
    }

    void on_unhandled_error(const proton::error_condition& c) override {
        unhandled_errors.push_back(c.what());
    }
};

void test_engine_prefix() {
    // Set container ID and prefix explicitly
    record_handler ha, hb;
    engine_pair e(ha, hb,
                  connection_options().container_id("a").link_prefix("x/"),
                  connection_options().container_id("b").link_prefix("y/"));
    e.a.connection().open();
    ASSERT_EQUAL("a", e.a.connection().container_id());
    e.b.connection().open();
    ASSERT_EQUAL("b", e.b.connection().container_id());

    e.a.connection().open_sender("x");
    while (ha.senders.empty() || hb.receivers.empty()) e.process();
    ASSERT_EQUAL("x/1", quick_pop(ha.senders).name());
    ASSERT_EQUAL("x/1", quick_pop(hb.receivers).name());

    e.a.connection().open_receiver("");
    while (ha.receivers.empty() || hb.senders.empty()) e.process();
    ASSERT_EQUAL("x/2", quick_pop(ha.receivers).name());
    ASSERT_EQUAL("x/2", quick_pop(hb.senders).name());

    e.b.connection().open_receiver("");
    while (ha.senders.empty() || hb.receivers.empty()) e.process();
    ASSERT_EQUAL("y/1", quick_pop(ha.senders).name());
    ASSERT_EQUAL("y/1", quick_pop(hb.receivers).name());
}

void test_container_prefix() {
    /// Let the container set the options.
    record_handler ha, hb;
    connection_engine::container ca("a"), cb("b");
    engine_pair e(ha, hb, ca.make_options(), cb.make_options());

    ASSERT_EQUAL("a", e.a.connection().container_id());
    ASSERT_EQUAL("b", e.b.connection().container_id());

    e.a.connection().open();
    sender s = e.a.connection().open_sender("x");
    ASSERT_EQUAL("1/1", s.name());

    while (ha.senders.empty() || hb.receivers.empty()) e.process();

    ASSERT_EQUAL("1/1", quick_pop(ha.senders).name());
    ASSERT_EQUAL("1/1", quick_pop(hb.receivers).name());

    e.a.connection().open_receiver("y");
    while (ha.receivers.empty() || hb.senders.empty()) e.process();
    ASSERT_EQUAL("1/2", quick_pop(ha.receivers).name());
    ASSERT_EQUAL("1/2", quick_pop(hb.senders).name());

    // Open a second connection in each container, make sure links have different IDs.
    record_handler ha2, hb2;
    engine_pair e2(ha2, hb2, ca.make_options(), cb.make_options());

    ASSERT_EQUAL("a", e2.a.connection().container_id());
    ASSERT_EQUAL("b", e2.b.connection().container_id());

    e2.b.connection().open();
    receiver r = e2.b.connection().open_receiver("z");
    ASSERT_EQUAL("2/1", r.name());

    while (ha2.senders.empty() || hb2.receivers.empty()) e2.process();

    ASSERT_EQUAL("2/1", quick_pop(ha2.senders).name());
    ASSERT_EQUAL("2/1", quick_pop(hb2.receivers).name());
};

void test_endpoint_close() {
    // Make sure conditions are sent to the remote end.

    record_handler ha, hb;
    engine_pair e(ha, hb);
    e.a.connection().open();
    e.a.connection().open_sender("x");
    e.a.connection().open_receiver("y");
    while (ha.senders.size()+ha.receivers.size() < 2 ||
           hb.senders.size()+hb.receivers.size() < 2) e.process();
    proton::internal::link ax = quick_pop(ha.senders), ay = quick_pop(ha.receivers);
    proton::internal::link bx = quick_pop(hb.receivers), by = quick_pop(hb.senders);

    // Close a link
    ax.close(proton::error_condition("err", "foo bar"));
    while (!bx.closed()) e.process();
    proton::error_condition c = bx.error();
    ASSERT_EQUAL("err", c.name());
    ASSERT_EQUAL("foo bar", c.description());
    ASSERT_EQUAL("err: foo bar", c.what());

    // Close a link with an empty condition
    ay.close(proton::error_condition());
    while (!by.closed()) e.process();
    ASSERT(by.error().empty());

    // Close a connection
    connection ca = e.a.connection(), cb = e.b.connection();
    ca.close(proton::error_condition("conn", "bad connection"));
    while (!cb.closed()) e.process();
    ASSERT_EQUAL("conn: bad connection", cb.error().what());
    ASSERT_EQUAL(1, hb.connection_errors.size());
    ASSERT_EQUAL("conn: bad connection", hb.connection_errors.front());
}

void test_transport_close() {
    // Make sure an engine close calls the local on_transport_error() and aborts the remote.
    record_handler ha, hb;
    engine_pair e(ha, hb);
    e.a.connection().open();
    while (!e.b.connection().active()) e.process();
    e.a.close(proton::error_condition("oops", "engine failure"));
    ASSERT(!e.a.dispatch());    // Final dispatch on a.
    ASSERT_EQUAL(1, ha.transport_errors.size());
    ASSERT_EQUAL("oops: engine failure", ha.transport_errors.front());
    ASSERT_EQUAL(proton::error_condition("oops", "engine failure"),e.a.transport().error());
    // But connectoin was never protocol closed.
    ASSERT(!e.a.connection().closed());
    ASSERT_EQUAL(0, ha.connection_errors.size());
}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_engine_prefix());
    RUN_TEST(failed, test_container_prefix());
    RUN_TEST(failed, test_endpoint_close());
    RUN_TEST(failed, test_transport_close());
    return failed;
}
