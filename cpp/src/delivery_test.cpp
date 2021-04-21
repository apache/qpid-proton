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
#include "proton/internal/pn_unique_ptr.hpp"
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

class test_recv : public proton::messaging_handler {
  private:
    class listener_ready_handler : public proton::listen_handler {
        void on_open(proton::listener &l) PN_CPP_OVERRIDE {
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
    test_recv(const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        listener = c.listen(url, listen_handler);
    }

    void on_message(proton::delivery &d, proton::message &msg) PN_CPP_OVERRIDE {
        proton::binary test_tag_recv("TESTTAG");
        ASSERT_EQUAL(test_tag_recv, d.tag());
        d.receiver().close();
        d.connection().close();
        listener.stop();
    }
};

class test_send : public proton::messaging_handler {
  private:
    std::string url;
    proton::sender sender;

  public:
    test_send(const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        proton::connection_options co;
        sender = c.open_sender(url, co);
    }

    void on_sendable(proton::sender &s) PN_CPP_OVERRIDE {
        proton::message msg;
        msg.body("message");
        proton::binary test_tag_send("TESTTAG");
        s.send(msg, test_tag_send);
    }
};

int test_delivery_tag() {
    std::string recv_address("127.0.0.1:0/test");

    test_recv recv(recv_address);
    proton::container c(recv);
    std::thread thread_recv([&c]() -> void { c.run(); });

    // wait until listener is ready
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return listener_ready; });

    std::string send_address =
        "127.0.0.1:" + std::to_string(listener_port) + "/test";
    test_send send(send_address);
    proton::container(send).run();
    thread_recv.join();

    return 0;
}

int main(int argc, char **argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_delivery_tag());
    return failed;
}
