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
#include "proton/error_condition.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/container.hpp"
#include "proton/delivery.hpp"
#include "proton/message.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/reconnect_options.hpp"
#include "proton/work_queue.hpp"

#include "proton/internal/pn_unique_ptr.hpp"

#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdio>
#include <sstream>

namespace {

// Wait for N things to be done.
class waiter {
    size_t count;
  public:
    waiter(size_t n) : count(n) {}
    void done() { if (--count == 0) ready(); }
    virtual void ready() = 0;
};

class server_connection_handler : public proton::messaging_handler {

    struct listen_handler : public proton::listen_handler {
        proton::connection_options opts;
        std::string url;
        waiter& listen_waiter;

        listen_handler(proton::messaging_handler& h, waiter& w) : listen_waiter(w) {
            opts.handler(h);
        }

        void on_open(proton::listener& l) PN_CPP_OVERRIDE {
            std::ostringstream o;
            o << "//:" << l.port(); // Connect to the actual listening port
            url = o.str();
            // Schedule rather than call done() direct to ensure serialization
            l.container().schedule(proton::duration::IMMEDIATE,
                                   proton::make_work(&waiter::done, &listen_waiter));
        }

        proton::connection_options on_accept(proton::listener&) PN_CPP_OVERRIDE { return opts; }
    };

    proton::listener listener_;
    int messages_;
    int expect_;
    bool closing_;
    bool done_;
    listen_handler listen_handler_;

    void close (proton::connection &c) {
        if (closing_) return;

        c.close(proton::error_condition("amqp:connection:forced", "Failover testing"));
        closing_ = true;
    }

  public:
    server_connection_handler(proton::container& c, int e, waiter& w)
        : messages_(0), expect_(e), closing_(false), done_(false), listen_handler_(*this, w)
    {
        listener_ = c.listen("//:0", listen_handler_);
    }

    std::string url() const {
        if (listen_handler_.url.empty()) throw std::runtime_error("no url");
        return listen_handler_.url;
    }

    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
        // Only listen for a single connection
        listener_.stop();
        if (messages_==expect_) close(c);
        else c.open();
    }

    void on_message(proton::delivery & d, proton::message & m) PN_CPP_OVERRIDE {
        ++messages_;
        proton::connection c = d.connection();
        if (messages_==expect_) close(c);
    }

    void on_connection_close(proton::connection & c) PN_CPP_OVERRIDE {
        done_ = true;
    }

    void on_transport_error(proton::transport & ) PN_CPP_OVERRIDE {
        // If we get an error then (try to) stop the listener
        // - this will stop the listener if we didn't already accept a connection
        listener_.stop();
    }
};

class tester : public proton::messaging_handler, public waiter {
  public:
    tester() : waiter(3), container_(*this, "reconnect_server") {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        // Server that fails upon connection
        s1.reset(new server_connection_handler(c, 0, *this));
        // Server that fails on first message
        s2.reset(new server_connection_handler(c, 1, *this));
        // server that doesn't fail in this test
        s3.reset(new server_connection_handler(c, 100, *this));
    }

    // waiter::ready is called when all 3 listeners are ready.
    void ready() PN_CPP_OVERRIDE {
        std::vector<std::string> urls;
        urls.push_back(s2->url());
        urls.push_back(s3->url());
        container_.connect(s1->url(), proton::connection_options().reconnect(proton::reconnect_options().failover_urls(urls)));
    }

    void on_connection_open(proton::connection& c) PN_CPP_OVERRIDE {
        c.open_sender("messages");
    }

    void on_sendable(proton::sender& s) PN_CPP_OVERRIDE {
        proton::message m;
        m.body("hello");
        s.send(m);
    }

    void on_tracker_accept(proton::tracker& d) PN_CPP_OVERRIDE {
        d.connection().close();
    }

    void run() {
        container_.run();
    }

  private:
    proton::internal::pn_unique_ptr<server_connection_handler> s1;
    proton::internal::pn_unique_ptr<server_connection_handler> s2;
    proton::internal::pn_unique_ptr<server_connection_handler> s3;
    proton::container container_;
};

int test_failover_simple() {
    tester().run();
    return 0;
}


}

class stop_reconnect_tester : public proton::messaging_handler {
  public:
    stop_reconnect_tester() :
        container_(*this, "reconnect_tester")
    {
    }

    void deferred_stop() {
        container_.stop();
    }

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        proton::reconnect_options reconnect_options;
        c.connect("this-is-not-going-to work.com", proton::connection_options().reconnect(reconnect_options));
        c.schedule(proton::duration::SECOND, proton::make_work(&stop_reconnect_tester::deferred_stop, this));
    }

    void run() {
        container_.run();
    }

  private:
    proton::container container_;
};

int test_stop_reconnect() {
    stop_reconnect_tester().run();
    return 0;
}

class authfail_reconnect_tester : public proton::messaging_handler, public waiter {
  public:
    authfail_reconnect_tester() :
        waiter(1), container_(*this, "authfail_reconnect_tester"), errored_(false)
    {}

    void deferred_stop() {
        container_.stop();
    }

    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        // This server won't fail in this test
        s1.reset(new server_connection_handler(c, 100, *this));
        c.schedule(proton::duration::SECOND, proton::make_work(&authfail_reconnect_tester::deferred_stop, this));
    }

    void on_transport_error(proton::transport& t) PN_CPP_OVERRIDE {
        errored_ = true;
    }

    void ready() PN_CPP_OVERRIDE {
        proton::connection_options co;
        co.sasl_allowed_mechs("PLAIN");
        co.reconnect(proton::reconnect_options());
        container_.connect(s1->url(), co);
    }

    void run() {
        container_.run();
        ASSERT(errored_);
    }

  private:
    proton::container container_;
    proton::internal::pn_unique_ptr<server_connection_handler> s1;
    bool errored_;
};

int test_auth_fail_reconnect() {
    authfail_reconnect_tester().run();
    return 0;
}

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_failover_simple());
    RUN_ARGV_TEST(failed, test_stop_reconnect());
    RUN_ARGV_TEST(failed, test_auth_fail_reconnect());
    return failed;
}

