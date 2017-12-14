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

static std::string int2string(int n) {
    std::ostringstream strm;
    strm << n;
    return strm.str();
}

class server_connection_handler : public proton::messaging_handler {
    proton::listener listener_;
    std::string url_;
    int messages_;
    int expect_;
    bool closing_;
    bool done_;

    void close (proton::connection &c) {
        if (closing_) return;

        c.close(proton::error_condition("amqp:connection:forced", "Failover testing"));
        closing_ = true;
    }

    void listen_on_random_port(proton::container& c, proton::messaging_handler& h) {
        int p;
        // I'm going to hell for this (on more than one count!):
        static struct once { once() {std::srand((unsigned int)time(0));} } x;
        while (true) {
            p = 20000 + (std::rand() % 30000);
            try {
                listener_ = c.listen("0.0.0.0:" + int2string(p), proton::connection_options().handler(h));
                break;
            } catch (...) {
                // keep trying
            }
        }
        url_ = "127.0.0.1:" + int2string(p);
    }

  public:
    server_connection_handler(proton::container& c, int e)
        : messages_(0), expect_(e), closing_(false), done_(false)
    {
        listen_on_random_port(c, *this);
    }

    std::string url() const { return url_; }

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
};

class tester : public proton::messaging_handler {
  public:
    tester() :
        container_(*this, "reconnect_server")
    {
    }

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        // Server that fails upon connection
        s1.reset(new server_connection_handler(c, 0));
        std::string url1 = s1->url();
        // Server that fails on first message
        s2.reset(new server_connection_handler(c, 1));
        std::string url2 = s2->url();
        // server that doesn't fail in this test
        s3.reset(new server_connection_handler(c, 100));
        std::string url3 = s3->url();

        std::vector<std::string> urls;
        urls.push_back(url2);
        urls.push_back(url3);
        c.connect(url1, proton::connection_options().reconnect(proton::reconnect_options().failover_urls(urls)));
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

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_failover_simple());
    RUN_ARGV_TEST(failed, test_stop_reconnect());
    return failed;
}

