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
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/default_container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"

#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdio>
#include <sstream>

namespace {

static std::string int2string(int n) {
    std::ostringstream strm;
    strm << n;
    return strm.str();
}

int listen_on_random_port(proton::container& c, proton::listener& l) {
    int port;
    // I'm going to hell for this:
    srand((unsigned int)time(0));
    while (true) {
        port = 20000 + (rand() % 30000);
        try {
            l = c.listen("0.0.0.0:" + int2string(port));
            break;
        } catch (...) {
            // keep trying
        }
    }
    return port;
}

class test_handler : public proton::messaging_handler {
  public:
    const std::string host;
    proton::connection_options opts;
    bool closing;
    bool done;

    std::string peer_vhost;
    proton::listener listener;

    test_handler(const std::string h, const proton::connection_options& c_opts)
        : host(h), opts(c_opts), closing(false), done(false)
    {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        int port = listen_on_random_port(c, listener);
        proton::connection conn = c.connect(host + ":" + int2string(port), opts);
    }

    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
        if (peer_vhost.empty() && !c.virtual_host().empty())
            peer_vhost = c.virtual_host();
        if (!closing) c.close();
        closing = true;
    }

    void on_connection_close(proton::connection &) PN_CPP_OVERRIDE {
        if (!done) listener.stop();
        done = true;
    }
};

int test_container_vhost() {
    proton::connection_options opts;
    opts.virtual_host(std::string("a.b.c"));
    test_handler th(std::string("127.0.0.1"), opts);
    proton::default_container(th).run();
    ASSERT_EQUAL(th.peer_vhost, std::string("a.b.c"));
    return 0;
}

int test_container_default_vhost() {
    proton::connection_options opts;
    test_handler th(std::string("127.0.0.1"), opts);
    proton::default_container(th).run();
    ASSERT_EQUAL(th.peer_vhost, std::string("127.0.0.1"));
    return 0;
}

int test_container_no_vhost() {
    // explicitly setting an empty virtual-host will cause the Open
    // performative to be sent without a hostname field present.
    // Sadly whether or not a 'hostname' field was received cannot be
    // determined from here, so just exercise the code
    proton::connection_options opts;
    opts.virtual_host(std::string(""));
    test_handler th(std::string("127.0.0.1"), opts);
    proton::default_container(th).run();
    ASSERT_EQUAL(th.peer_vhost, std::string(""));
    return 0;
}

struct test_listener : public proton::listen_handler {
    bool on_accept_, on_close_;
    std::string on_error_;
    test_listener() : on_accept_(false), on_close_(false) {}
    proton::connection_options on_accept() PN_CPP_OVERRIDE {
        on_accept_ = true;
        return proton::connection_options();
    }
    void on_close() PN_CPP_OVERRIDE { on_close_ = true; }
    void on_error(const std::string& e) PN_CPP_OVERRIDE { on_error_ = e; }
};

int test_container_bad_address() {
    // Listen on a bad address, check for leaks
    // Regression test for https://issues.apache.org/jira/browse/PROTON-1217

    proton::default_container c;
    // Default fixed-option listener. Valgrind for leaks.
    try { c.listen("999.666.999.666:0"); } catch (const proton::error&) {}
    // Dummy listener.
    test_listener l;
    test_handler h2(std::string("999.999.999.666"), proton::connection_options());
    try { c.listen("999.666.999.666:0", l); } catch (const proton::error&) {}
    ASSERT(!l.on_accept_);
    ASSERT(l.on_close_);
    ASSERT(!l.on_error_.empty());
    return 0;
}

class stop_tester : public proton::messaging_handler {
    proton::listener listener;

    // Set up a listener which would block forever
    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        ASSERT(state==0);
        int port = listen_on_random_port(c, listener);
        c.connect("127.0.0.1:" + int2string(port));
        c.auto_stop(false);
        state = 1;
    }

    // Get here twice - once for listener, once for connector
    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
        c.close();
        state++;
    }

    void on_connection_close(proton::connection &c) PN_CPP_OVERRIDE {
        ASSERT(state==3);
        c.container().stop();
        state = 4;
    }
    void on_container_stop(proton::container & ) PN_CPP_OVERRIDE {
        ASSERT(state==4);
        state = 5;
    }

public:
    stop_tester(): state(0) {}

    int state;
};

int test_container_stop() {
    stop_tester t;
    proton::default_container(t).run();
    ASSERT(t.state==5);
    return 0;
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_container_vhost());
    RUN_TEST(failed, test_container_default_vhost());
    RUN_TEST(failed, test_container_no_vhost());
    RUN_TEST(failed, test_container_bad_address());
    RUN_TEST(failed, test_container_stop());
    return failed;
}

