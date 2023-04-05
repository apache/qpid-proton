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

        std::string data = "listener-user-data";
        // Set user context 'data' on listener.
        listener.user_data(&data);
        ASSERT_EQUAL(&data, listener.user_data());
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        std::string data = "delivery-user-data";
        // Set user context 'data' on delivery.
        d.user_data(&data);
        ASSERT_EQUAL(&data, d.user_data());

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

    void on_connection_open(proton::connection& c) override {
        // Get default session
        proton::session s = c.default_session();

        std::string data_ssn = "session-user-data";
        // Set user context 'data' on default session.
        s.user_data(&data_ssn);
        ASSERT_EQUAL(&data_ssn, s.user_data());

        std::string data_con = "connection-user-data";
        // Set user context 'data' on current connection.
        c.user_data(&data_con);
        ASSERT_EQUAL(&data_con, c.user_data());
    }

    void on_sendable(proton::sender &s) override {
        proton::message msg;
        msg.body("message");
        proton::tracker t = s.send(msg);

        std::string data = "sender-user-data";
        // Set user context 'data' on sender.
        s.user_data(&data);
        ASSERT_EQUAL(&data, s.user_data());

        s.connection().close();
    }

    void on_tracker_accept(proton::tracker &t) override {
        std::string data = "tracker-user-data";
        // Set user context 'data' on tracker.
        t.user_data(&data);
        ASSERT_EQUAL(&data, t.user_data());
    }
};

int test_user_context() {

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

    return 0;
}

int main(int argc, char **argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_user_context());
    return failed;
}
