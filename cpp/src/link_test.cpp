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

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target_options.hpp>
#include <proton/types.hpp>

#include <iostream>

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

using property_map=proton::terminus::dynamic_property_map;

namespace {
std::mutex m;
std::condition_variable cv;
bool listener_ready = false;
int listener_port;
const std::string DYNAMIC_ADDRESS = "test_dynamic_address";
} // namespace

class test_server : public proton::messaging_handler {
  private:
    class listener_ready_handler : public proton::listen_handler {
        void on_open(proton::listener& l) override {
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
    test_server (const std::string& s) : url(s) {}

    void on_container_start(proton::container& c) override {
        listener = c.listen(url, listen_handler);
    }

    void on_sender_open(proton::sender& s) override {
        ASSERT(s.source().dynamic());
        ASSERT(s.source().address().empty());
        property_map p = {
          {proton::symbol("supported-dist-modes"), proton::symbol("copy")}
        };
        ASSERT_EQUAL(s.source().dynamic_properties(), p);

        proton::source_options opts;
        opts.address(DYNAMIC_ADDRESS)
            // This fails due to a bug in the C++ bindings - PROTON-2480
            // .dynamic(true)
            .dynamic_properties({{proton::symbol("supported-dist-modes"), proton::symbol("move")}});
        s.open(proton::sender_options().source(opts));
        listener.stop();
    }
};

class test_client : public proton::messaging_handler {
  private:
    std::string url;

  public:
    test_client (const std::string& s) : url(s) {}

    void on_container_start(proton::container& c) override {
        property_map sp = std::map<proton::symbol, proton::value>{
          {proton::symbol("supported-dist-modes"), proton::symbol("copy")}
        };
        proton::source_options opts;
        opts.dynamic(true)
            .dynamic_properties(sp);
        c.open_receiver(url, proton::receiver_options().source(opts));
    }

    void on_receiver_open(proton::receiver& r) override {
        // This fails due to a bug in the c++ bindings - PROTON-2480
        // ASSERT(r.source().dynamic());
        ASSERT_EQUAL(DYNAMIC_ADDRESS, r.source().address());
        property_map m = std::map<proton::symbol, proton::value>{
          {proton::symbol("supported-dist-modes"), proton::symbol("move")}
        };
        ASSERT_EQUAL(m, r.source().dynamic_properties());

        r.connection().close();
    }
};

int test_dynamic_node_properties() {

    std::string recv_address("127.0.0.1:0");
    test_server server(recv_address);
    proton::container c(server);
    std::thread thread_recv([&c]() -> void { c.run(); });

    // wait until listener is ready
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return listener_ready; });

    std::string send_address = "127.0.0.1:" + std::to_string(listener_port);
    test_client client(send_address);
    proton::container(client).run();
    thread_recv.join();

    return 0;
}

int test_link_name()
{
    proton::container c;

    proton::sender_options so;
    proton::receiver_options ro;

    so.name("qpid-s");
    ro.name("qpid-r");

    proton::sender sender = c.open_sender("", so);
    proton::receiver receiver = c.open_receiver("", ro);

    ASSERT_EQUAL("qpid-s", sender.name());
    ASSERT_EQUAL("qpid-r", receiver.name());

    return 0;
}

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_link_name());
    RUN_ARGV_TEST(failed, test_dynamic_node_properties());
    return failed;
}
