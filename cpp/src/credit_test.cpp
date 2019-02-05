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
#include "proton/delivery.hpp"
#include "proton/error_condition.hpp"
#include "proton/listen_handler.hpp"
#include "proton/listener.hpp"
#include "proton/message.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/receiver_options.hpp"
#include "proton/transport.hpp"
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
    proton::sender sender_;
    int expect_;
    bool closing_;
    int available_;
    int acked_;
    listen_handler listen_handler_;
    proton::work_queue *notify_wq_;
    proton::work notify_work_;

    void close (proton::connection &c) {
        if (closing_) return;

        c.close(proton::error_condition("amqp:connection:forced", "Failover testing"));
        closing_ = true;
    }

  public:
    server_connection_handler(proton::container& c, int a, waiter& w)
        : expect_(0), closing_(false), available_(a), acked_(0), listen_handler_(*this, w), notify_wq_(0)
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
        c.open();
    }

    void on_sender_open(proton::sender &s) PN_CPP_OVERRIDE {
        s.open();
        sender_ = s;
    }

    void on_sendable(proton::sender &s) PN_CPP_OVERRIDE {
        send_available_messages(s);
    }

    void send_available_messages(proton::sender &s) {
        bool draining = s.draining();
        while (available_ && s.credit() > 0) {
            s.send(proton::message("hello"));
            available_--;
            expect_++;
        }
        if (draining && !available_ && s.credit()) {
            s.return_credit(); // return the rest
        }
    }

    void notify_idle() {
        notify_wq_->add(notify_work_);
    }

    void on_tracker_accept(proton::tracker& d) PN_CPP_OVERRIDE {
        acked_++;
        if (acked_ == expect_ && (available_ == 0 || d.sender().credit() == 0))
            notify_idle();
    }

    void on_transport_error(proton::transport & ) PN_CPP_OVERRIDE {
        // If we get an error then (try to) stop the listener
        // - this will stop the listener if we didn't already accept a connection
        listener_.stop();
    }

    void notify_on_idle(proton::work_queue &wq, proton::work &w) { notify_wq_ = &wq; notify_work_ = w;}

    void available(int i) {
        available_ = i;
    }
};

class tester : public proton::messaging_handler, public waiter {
  public:
    tester() : waiter(1), container_(*this, "credit_tester"),
               received_(0), initial_credit_(0) {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        srv_.reset(new server_connection_handler(c, 100000, *this));
    }

    // waiter::ready is called when listener can accept connections.
    void ready() PN_CPP_OVERRIDE {
        container_.connect(srv_->url());
    }

    void on_connection_open(proton::connection& c) PN_CPP_OVERRIDE {
        c.open_receiver("messages", proton::receiver_options().credit_window(0));
    }

    void on_receiver_open(proton::receiver &r) PN_CPP_OVERRIDE {
        receiver_ = r;
        next_idle_ = proton::make_work(&tester::first_idle, this);
        proton::work call_on_server_idle(make_work(&tester::on_server_idle, this));
        srv_->notify_on_idle(r.work_queue(), call_on_server_idle);
        r.add_credit(initial_credit_);
    }

    void on_message(proton::delivery &d, proton::message &m) PN_CPP_OVERRIDE {
        received_++;
        d.accept();
    }

    void run() {
        container_.run(); // Single threaded to avoid locks and barriers.
    }

    void server_available(int available) {
        // If multithreaded, locking would be required.
        srv_->available(available);
    }

    void on_server_idle() {
        next_idle_();
    }

    void fail(const std::string &msg, int rcv) {
        // Call from work_queue.  Remember the exception later.
        std::ostringstream os;
        os << msg << rcv;
        fail_msg_ = os.str();
        receiver_.connection().close();
    }

    void on_connection_close(proton::connection& c) PN_CPP_OVERRIDE {
        if (!fail_msg_.empty())
            FAIL(fail_msg_);
    }

    virtual void first_idle() = 0;

  protected:
    proton::internal::pn_unique_ptr<server_connection_handler> srv_;
    proton::container container_;
    proton::receiver receiver_;
    proton::work next_idle_;
    std::string fail_msg_;
    int received_;
    int initial_credit_;
};


class basic_credit_tester : public tester {
  public:
    basic_credit_tester() { initial_credit_ = 3; }

    void first_idle() PN_CPP_OVERRIDE {
        if (received_ != 3) {
            fail(FAIL_MSG("messages received should be 3 not "), received_);
            return;
        }
        next_idle_ = proton::make_work(&basic_credit_tester::second_idle, this);
        server_available(2);
        receiver_.add_credit(3);
    }

    void second_idle() {
        if (received_ != 5) {
            fail(FAIL_MSG("messages received should be 5 not "), received_);
            return;
        }
        next_idle_ = proton::make_work(&basic_credit_tester::third_idle, this);
        server_available(10);
        receiver_.add_credit(1);
    }

    void third_idle() {
        if (received_ != 7) {
            fail(FAIL_MSG("messages received should be 7 not "), received_);
            return;
        }
        // passed
        receiver_.connection().close();
    }
};


int test_basic_credit() {
    basic_credit_tester().run();
    return 0;
}


class drain_credit_tester : public tester {
    int drain_finishes_;

  public:
    drain_credit_tester() : drain_finishes_(0) { initial_credit_ = 10; }

    void on_receiver_drain_finish(proton::receiver &r) PN_CPP_OVERRIDE {
        drain_finishes_++;
    }

    void first_idle() PN_CPP_OVERRIDE {
        if (received_ != 10) {
            fail(FAIL_MSG("messages received should be 10 not "), received_);
            return;
        }
        next_idle_ = proton::make_work(&drain_credit_tester::second_idle, this);
        server_available(10);
        receiver_.add_credit(15);
        receiver_.drain();
    }

    void second_idle() {
        if (received_ != 20) {
            fail(FAIL_MSG("messages received should be 20 not "), received_);
            return;
        }
        if (drain_finishes_ != 1) {
            fail(FAIL_MSG("drain finish callbacks should be 1, not: "), drain_finishes_);
            return;
        }
        if (receiver_.credit() != 0) {
            fail(FAIL_MSG("credit not returned on drain, remaining: "), receiver_.credit());
            return;
        }
        next_idle_ = proton::make_work(&drain_credit_tester::third_idle, this);
        server_available(5);
        receiver_.add_credit(10);
    }

    void third_idle() {
        if (received_ != 25) {
            fail(FAIL_MSG("messages received should be 20 not "), received_);
            return;
        }
        if (receiver_.credit() != 5) {
            fail(FAIL_MSG("incorrect credit after drain, should be 5, not "), receiver_.credit());
            return;
        }
        next_idle_ = proton::make_work(&drain_credit_tester::fourth_idle, this);
        server_available(3);
        receiver_.add_credit(1);
    }

    void fourth_idle() {
        if (received_ != 28) {
            fail(FAIL_MSG("messages received should be 28 not "), received_);
            return;
        }
        if (receiver_.credit() != 3) {
            fail(FAIL_MSG("incorrect credit, should be 3, not "), receiver_.credit());
            return;
        }
        next_idle_ = proton::make_work(&drain_credit_tester::fifth_idle, this);
        server_available(1);
        receiver_.drain();
    }

    void fifth_idle() {
        if (received_ != 29) {
            fail(FAIL_MSG("messages received should be 29 not "), received_);
            return;
        }
        if (drain_finishes_ != 2) {
            fail(FAIL_MSG("drain finish callbacks should be 2, not: "), drain_finishes_);
            return;
        }
        if (receiver_.credit() != 0) {
            fail(FAIL_MSG("second drain credit failed, should be 0, not "), receiver_.credit());
            return;
        }
        // passed
        receiver_.connection().close();
    }
};

int test_drain_credit() {
    drain_credit_tester().run();
    return 0;
}


}  // namespace


int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_basic_credit());
    RUN_ARGV_TEST(failed, test_drain_credit());
    return failed;
}
