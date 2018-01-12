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
#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/work_queue.hpp"

#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdio>
#include <sstream>

namespace {

std::string make_url(std::string host, int port) {
    std::ostringstream url;
    url << "//" << host << ":" << port;
    return url.str();
}

struct test_listen_handler : public proton::listen_handler {
    bool on_open_, on_accept_, on_close_;
    std::string on_error_;
    std::string host_;
    proton::connection_options opts_;

    test_listen_handler(const std::string& host=std::string(),
                        const proton::connection_options& opts=proton::connection_options()
    ) : on_open_(false), on_accept_(false), on_close_(false), host_(host), opts_(opts) {}

    proton::connection_options on_accept(proton::listener&) PN_CPP_OVERRIDE {
        on_accept_ = true;
        return proton::connection_options();
    }
    void on_open(proton::listener& l) PN_CPP_OVERRIDE {
        on_open_ = true;
        ASSERT(!on_accept_);
        ASSERT(on_error_.empty());
        ASSERT(!on_close_);
        l.container().connect(make_url(host_, l.port()), opts_);
    }

    void on_close(proton::listener&) PN_CPP_OVERRIDE {
        on_close_ = true;
        ASSERT(on_open_ || on_error_.size());
    }

    void on_error(proton::listener&, const std::string& e) PN_CPP_OVERRIDE {
        on_error_ = e;
        ASSERT(!on_close_);
    }
};

class test_handler : public proton::messaging_handler {
  public:
    bool closing;
    bool done;

    std::string peer_vhost;
    std::string peer_container_id;
    proton::listener listener;
    test_listen_handler listen_handler;

    test_handler(const std::string h, const proton::connection_options& c_opts)
        : closing(false), done(false), listen_handler(h, c_opts)
    {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        listener = c.listen("//:0", listen_handler);
    }

    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
        ASSERT(listen_handler.on_open_);
        ASSERT(!listen_handler.on_close_);
        ASSERT(listen_handler.on_error_.empty());
        if (peer_vhost.empty() && !c.virtual_host().empty())
            peer_vhost = c.virtual_host();
        if (peer_container_id.empty() && !c.container_id().empty())
            peer_container_id = c.container_id();
        if (!closing) c.close();
        closing = true;
    }

    void on_connection_close(proton::connection &) PN_CPP_OVERRIDE {
        if (!done) listener.stop();
        done = true;
    }
};

int test_container_default_container_id() {
    proton::connection_options opts;
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT(!th.peer_container_id.empty());
    ASSERT(th.listen_handler.on_error_.empty());
    ASSERT(th.listen_handler.on_close_);
    return 0;
}

int test_container_vhost() {
    proton::connection_options opts;
    opts.virtual_host("a.b.c");
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "a.b.c");
    return 0;
}

int test_container_default_vhost() {
    proton::connection_options opts;
    test_handler th("127.0.0.1", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "127.0.0.1");
    return 0;
}

int test_container_no_vhost() {
    // explicitly setting an empty virtual-host will cause the Open
    // performative to be sent without a hostname field present.
    // Sadly whether or not a 'hostname' field was received cannot be
    // determined from here, so just exercise the code
    proton::connection_options opts;
    opts.virtual_host("");
    test_handler th("127.0.0.1", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "");
    return 0;
}

int test_container_bad_address() {
    // Listen on a bad address, check for leaks
    // Regression test for https://issues.apache.org/jira/browse/PROTON-1217

    proton::container c;
    // Default fixed-option listener. Valgrind for leaks.
    try { c.listen("999.666.999.666:0"); } catch (const proton::error&) {}
    c.run();
    // Dummy listener.
    test_listen_handler l;
    test_handler h2("999.999.999.666", proton::connection_options());
    try { c.listen("999.666.999.666:0", l); } catch (const proton::error&) {}
    c.run();
    ASSERT(!l.on_open_);
    ASSERT(!l.on_accept_);
    ASSERT(l.on_close_);
    ASSERT(!l.on_error_.empty());
    return 0;
}

class stop_tester : public proton::messaging_handler {
    proton::listener listener;
    test_listen_handler listen_handler;

    // Set up a listener which would block forever
    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        ASSERT(state==0);
        listener = c.listen("//:0", listen_handler);
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

    void on_transport_error(proton::transport & t) PN_CPP_OVERRIDE {
        // Do nothing - ignore transport errors - we're going to get one when
        // the container stops.
    }

public:
    stop_tester(): state(0) {}

    int state;
};

int test_container_stop() {
    stop_tester t;
    proton::container(t).run();
    ASSERT(t.state==5);
    return 0;
}


struct hang_tester : public proton::messaging_handler {
    proton::listener listener;
    bool done;

    hang_tester() : done(false) {}

    void connect(proton::container* c) {
        c->connect(make_url("", listener.port()));
    }

    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        listener = c.listen("//:0");
        c.schedule(proton::duration(250), make_work(&hang_tester::connect, this, &c));
    }

    void on_connection_open(proton::connection& c) PN_CPP_OVERRIDE {
        c.close();
    }

    void on_connection_close(proton::connection& c) PN_CPP_OVERRIDE {
        if (!done) {
            done = true;
            listener.stop();
        }
    }
};

int test_container_schedule_nohang() {
    hang_tester t;
    proton::container(t).run();
    return 0;
}

class immediate_stop_tester : public proton::messaging_handler {
public:
    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        c.stop();
    }
};

int test_container_immediate_stop() {
    immediate_stop_tester t;
    proton::container(t).run();  // will hang
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_container_default_container_id());
    RUN_ARGV_TEST(failed, test_container_vhost());
    RUN_ARGV_TEST(failed, test_container_default_vhost());
    RUN_ARGV_TEST(failed, test_container_no_vhost());
    RUN_ARGV_TEST(failed, test_container_bad_address());
    RUN_ARGV_TEST(failed, test_container_stop());
    RUN_ARGV_TEST(failed, test_container_schedule_nohang());
    RUN_ARGV_TEST(failed, test_container_immediate_stop());
    return failed;
}

