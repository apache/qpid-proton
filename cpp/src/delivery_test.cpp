/*
 *
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
 *
 */

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.h>
#include <proton/delivery.hpp>
#include <proton/link.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/tracker.hpp>
#include <proton/types.h>
#include <proton/types.hpp>
#include <proton/value.hpp>

#include "proton/error_condition.hpp"
#include "proton/receiver_options.hpp"
#include "proton/transport.hpp"
#include "proton/work_queue.hpp"
#include "test_bits.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
std::mutex m;
std::condition_variable cv;
bool listener_ready = false;
int listener_port;
int tracker_accept_counter;
int tracker_settle_counter;
proton::binary test_tag("TESTTAG");
} // namespace

class test_server : public proton::messaging_handler {
  private:
    class listener_ready_handler : public proton::listen_handler {
        void on_open(proton::listener &l) override {
            {
                std::lock_guard<std::mutex> lk(m);
                listener_port = l.port();
                listener_ready = true;
            }
            cv.notify_one();
        }
    };

    std::string url;
    proton::listener listener;
    listener_ready_handler listen_handler;

  public:
    test_server (const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) override {
        listener = c.listen(url, listen_handler);
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        ASSERT_EQUAL(test_tag, d.tag());
        d.receiver().close();
        d.connection().close();
        listener.stop();
    }
};

class test_client : public proton::messaging_handler {
  private:
    std::string url;
    proton::sender sender;

  public:
    test_client (const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) override {
        proton::connection_options co;
        sender = c.open_sender(url, co);
    }

    void on_sendable(proton::sender &s) override {
        proton::message msg;
        msg.body("message");
        proton::tracker t = s.send(msg, test_tag);
        ASSERT_EQUAL(test_tag, t.tag());
        s.connection().close();
    }

    void on_tracker_accept(proton::tracker &t) override {
        ASSERT_EQUAL(test_tag, t.tag());
        tracker_accept_counter++;
    }

    void on_tracker_settle(proton::tracker &t) override {
        ASSERT_EQUAL(test_tag, t.tag());
        tracker_settle_counter++;
    }
};

int test_delivery_tag() {
    tracker_accept_counter = 0;
    tracker_settle_counter = 0;

    std::string recv_address("127.0.0.1:0/test");
    test_server recv(recv_address);
    proton::container c(recv);
    std::thread thread_recv([&c]() -> void { c.run(); });

    // wait until listener is ready
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return listener_ready; });

    std::string send_address =
        "127.0.0.1:" + std::to_string(listener_port) + "/test";
    test_client send(send_address);
    proton::container(send).run();
    thread_recv.join();

    ASSERT_EQUAL(1 ,tracker_accept_counter);
    ASSERT_EQUAL(1 ,tracker_settle_counter);

    return 0;
}

int main(int argc, char **argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_delivery_tag());
    return failed;
}
