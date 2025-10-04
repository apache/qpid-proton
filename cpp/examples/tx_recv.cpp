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

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/source.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/types.hpp>
#include <proton/transaction_handler.hpp>

#include <iostream>
#include <map>
#include <string>

#include <atomic>
#include <chrono>
#include <thread>

class tx_recv : public proton::messaging_handler {
  private:
    proton::sender sender;
    proton::receiver receiver;
    std::string conn_url_;
    std::string addr_;
    int expected;
    int received = 0;
    std::atomic<int> unique_msg_id;

  public:
    tx_recv(const std::string& u, const std::string &a, int c):
        conn_url_(u), addr_(a), expected(c), unique_msg_id(20000) {}

    void on_container_start(proton::container &c) override {
        c.connect(conn_url_);
    }

   void on_connection_open(proton::connection& c) override {
        receiver = c.open_receiver(addr_);
        sender = c.open_sender(addr_);
    }

    void on_session_open(proton::session &s) override {
        std::cout << "New session is open" << std::endl;
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        std::cout<<"# MESSAGE: " << msg.id() <<": "  << msg.body() << std::endl;
        d.accept();
        proton::message reply_message;

        reply_message.id(std::atomic_fetch_add(&unique_msg_id, 1));
        reply_message.body(msg.body());
        reply_message.reply_to(receiver.source().address());

        sender.send(reply_message);

        received += 1;
        if (received == expected) {
            receiver.connection().close();
        }
    }
};

int main(int argc, char **argv) {
    std::string conn_url = argc > 1 ? argv[1] : "//127.0.0.1:5672";
    std::string addr = argc > 2 ? argv[2] : "examples";
    int message_count = 6;
    example::options opts(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connect and send to URL", "URL");
    opts.add_value(addr, 'a', "address", "connect and send to address", "URL");
    opts.add_value(message_count, 'm', "messages", "number of messages to send", "COUNT");

    try {
        opts.parse();

        tx_recv recv(conn_url, addr, message_count);
        proton::container(recv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
