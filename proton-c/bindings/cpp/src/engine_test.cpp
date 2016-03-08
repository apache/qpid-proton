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
#include <proton/connection_engine.hpp>
#include <proton/handler.hpp>
#include <proton/event.hpp>
#include <proton/types_fwd.hpp>
#include <proton/link.hpp>
#include <deque>
#include <algorithm>

using namespace proton;
using namespace test;

// One end of an in-memory connection
struct mem_pipe {
    mem_pipe(std::deque<char>& r, std::deque<char>& w) : read(r), write(w) {}
    std::deque<char>  &read, &write;
};

struct mem_queues : public std::pair<std::deque<char>, std::deque<char> > {
    mem_pipe a() { return mem_pipe(first, second); }
    mem_pipe b() { return mem_pipe(second, first); }
};

// In memory connection_engine
struct mem_engine : public connection_engine {
    mem_pipe socket;
    std::string read_error;
    std::string write_error;

    mem_engine(mem_pipe s, handler &h, const connection_options &opts)
        : connection_engine(h, opts), socket(s) {}

    std::pair<size_t, bool> io_read(char* buf, size_t size) {
        if (!read_error.empty()) throw io_error(read_error);
        size = std::min(socket.read.size(), size);
        copy(socket.read.begin(), socket.read.begin()+size, buf);
        socket.read.erase(socket.read.begin(), socket.read.begin()+size);
        return std::make_pair(size, true);
    }

    size_t io_write(const char* buf, size_t size) {
        if (!write_error.empty()) throw io_error(write_error);
        socket.write.insert(socket.write.begin(), buf, buf+size);
        return size;
    }

    void io_close() {
        read_error = write_error = "closed";
    }
};

struct debug_handler : handler {
    void on_unhandled(event& e) {
        std::cout << e.name() << std::endl;
    }
};

struct record_handler : handler {
    std::deque<std::string> events;
    void on_unhandled(event& e) {
        events.push_back(e.name());
    }
};

template <class HA=record_handler, class HB=record_handler> struct engine_pair {
    connection_engine::container cont;
    mem_queues queues;
    HA ha;
    HB hb;
    mem_engine a, b;
    engine_pair() : a(queues.a(), ha, cont.make_options()), b(queues.b(), hb, cont.make_options()) {}
    engine_pair(const std::string& id)
        : cont(id), a(queues.a(), ha, cont.make_options()), b(queues.b(), hb, cont.make_options())
    {}
    engine_pair(const connection_options &aopts, connection_options &bopts)
        : a(queues.a(), ha, aopts), b(queues.b(), hb, bopts)
    {}

    void process() { a.process(); b.process(); }
};

void test_process_amqp() {
    engine_pair<> e;

    e.a.process(connection_engine::READ); // Don't write unlesss writable
    ASSERT(e.a.socket.write.empty());
    e.a.process(connection_engine::WRITE);

    std::string wrote(e.a.socket.write.begin(), e.a.socket.write.end());
    e.a.process(connection_engine::WRITE);
    ASSERT_EQUAL(8, wrote.size());
    ASSERT_EQUAL("AMQP", wrote.substr(0,4));

    e.b.process();              // Read and write AMQP
    ASSERT_EQUAL("AMQP", std::string(e.b.socket.write.begin(), e.b.socket.write.begin()+4));
    ASSERT(e.b.socket.read.empty());
    ASSERT(e.a.socket.write.empty());
    ASSERT_EQUAL(many<std::string>() + "START", e.ha.events);
}


struct link_handler : public record_handler {
    std::deque<proton::link> links;
    void on_link_open(event& e) {
        links.push_back(e.link());
    }

    proton::link pop() {
        proton::link l;
        if (!links.empty()) {
            l = links.front();
            links.pop_front();
        }
        return l;
    }
};

void test_engine_prefix() {
    // Set container ID and prefix explicitly
    engine_pair<link_handler, link_handler> e(
        connection_options().container_id("a").link_prefix("x/"),
        connection_options().container_id("b").link_prefix("y/"));
    e.a.connection().open();
    ASSERT_EQUAL("a", e.a.connection().container_id());
    e.b.connection().open();
    ASSERT_EQUAL("b", e.b.connection().container_id());

    e.a.connection().open_sender("");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("x/1", e.ha.pop().name());
    ASSERT_EQUAL("x/1", e.hb.pop().name());

    e.a.connection().open_receiver("");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("x/2", e.ha.pop().name());
    ASSERT_EQUAL("x/2", e.hb.pop().name());

    e.b.connection().open_receiver("");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("y/1", e.ha.pop().name());
    ASSERT_EQUAL("y/1", e.hb.pop().name());
}

void test_container_prefix() {
    /// Let the container set the options.
    engine_pair<link_handler, link_handler> e;
    e.a.connection().open();

    e.a.connection().open_sender("x");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("1/1", e.ha.pop().name());
    ASSERT_EQUAL("1/1", e.hb.pop().name());

    e.a.connection().open_receiver("y");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("1/2", e.ha.pop().name());
    ASSERT_EQUAL("1/2", e.hb.pop().name());

    e.b.connection().open_receiver("z");
    while (e.ha.links.size() + e.hb.links.size() < 2) e.process();
    ASSERT_EQUAL("2/1", e.ha.pop().name());
    ASSERT_EQUAL("2/1", e.hb.pop().name());

    // TODO aconway 2016-01-22: check we respect name set in linkn-options.
};

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_process_amqp());
    RUN_TEST(failed, test_engine_prefix());
    RUN_TEST(failed, test_container_prefix());
    return failed;
}
