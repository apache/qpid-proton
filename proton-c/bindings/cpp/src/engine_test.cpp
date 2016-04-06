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
    std::deque<proton::link> links;
    std::deque<proton::session> sessions;
    std::deque<std::string> errors;

    void on_receiver_open(receiver &l) override {
        links.push_back(l);
    }

    void on_sender_open(sender &l) override {
        links.push_back(l);
    }

    void on_session_open(session &s) override {
        sessions.push_back(s);
    }

    void on_unhandled_error(const condition& c) override {
        errors.push_back(c.what());
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
    while (ha.links.empty() || hb.links.empty()) e.process();
    ASSERT_EQUAL("x/1", quick_pop(ha.links).name());
    ASSERT_EQUAL("x/1", quick_pop(hb.links).name());

    e.a.connection().open_receiver("");
    while (ha.links.empty() || hb.links.empty()) e.process();
    ASSERT_EQUAL("x/2", quick_pop(ha.links).name());
    ASSERT_EQUAL("x/2", quick_pop(hb.links).name());

    e.b.connection().open_receiver("");
    while (ha.links.empty() || hb.links.empty()) e.process();
    ASSERT_EQUAL("y/1", quick_pop(ha.links).name());
    ASSERT_EQUAL("y/1", quick_pop(hb.links).name());
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

    while (ha.links.empty() || hb.links.empty()) e.process();

    ASSERT_EQUAL("1/1", quick_pop(ha.links).name());
    ASSERT_EQUAL("1/1", quick_pop(hb.links).name());

    e.a.connection().open_receiver("y");
    while (ha.links.empty() || hb.links.empty()) e.process();
    ASSERT_EQUAL("1/2", quick_pop(ha.links).name());
    ASSERT_EQUAL("1/2", quick_pop(hb.links).name());

    // Open a second connection in each container, make sure links have different IDs.
    record_handler ha2, hb2;
    engine_pair e2(ha2, hb2, ca.make_options(), cb.make_options());

    ASSERT_EQUAL("a", e2.a.connection().container_id());
    ASSERT_EQUAL("b", e2.b.connection().container_id());

    e2.b.connection().open();
    receiver r = e2.b.connection().open_receiver("z");
    ASSERT_EQUAL("2/1", r.name());

    while (ha2.links.empty() || hb2.links.empty()) e2.process();

    ASSERT_EQUAL("2/1", quick_pop(ha2.links).name());
    ASSERT_EQUAL("2/1", quick_pop(hb2.links).name());
};

void test_endpoint_close() {
    // Make sure conditions are sent to the remote end.

    // FIXME aconway 2016-03-22: re-enable these tests when we can set error conditions.

    // record_handler ha, hb;
    // engine_pair e(ha, hb);
    // e.a.connection().open();
    // e.a.connection().open_sender("x");
    // e.a.connection().open_receiver("y");
    // while (ha.links.size() < 2 || hb.links.size() < 2) e.process();
    // link ax = quick_pop(ha.links), ay = quick_pop(ha.links);
    // link bx = quick_pop(hb.links), by = quick_pop(hb.links);

    // // Close a link
    // ax.close(condition("err", "foo bar"));
    // while (!(bx.state() & endpoint::REMOTE_CLOSED)) e.process();
    // condition c = bx.remote_condition();
    // ASSERT_EQUAL("err", c.name());
    // ASSERT_EQUAL("foo bar", c.description());
    // ASSERT_EQUAL("err: foo bar", ax.local_condition().what());

    // // Close a link with an empty condition
    // ay.close(condition());
    // while (!(by.state() & endpoint::REMOTE_CLOSED)) e.process();
    // ASSERT(by.remote_condition().empty());

    // // Close a connection
    // connection ca = e.a.connection(), cb = e.b.connection();
    // ca.close(condition("conn", "bad connection"));
    // while (!cb.closed()) e.process();
    // ASSERT_EQUAL("conn: bad connection", cb.remote_condition().what());
}

void test_transport_close() {
    // Make sure conditions are sent to the remote end.
    record_handler ha, hb;
    engine_pair e(ha, hb);
    e.a.connection().open();
    while (!e.a.connection().state() & endpoint::REMOTE_ACTIVE) e.process();

    e.a.close("oops", "engine failure");
    // Closed but we still have output data to flush so a.dispatch() is true.
    ASSERT(e.a.dispatch());
    while (!e.b.connection().closed()) e.process();
    ASSERT_EQUAL(1, hb.errors.size());
    ASSERT_EQUAL("oops: engine failure", hb.errors.front());
    ASSERT_EQUAL("oops", e.b.connection().remote_condition().name());
    ASSERT_EQUAL("engine failure", e.b.connection().remote_condition().description());
}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_engine_prefix());
    RUN_TEST(failed, test_container_prefix());
    RUN_TEST(failed, test_endpoint_close());
    return failed;
}
