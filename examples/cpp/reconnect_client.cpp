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
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/link.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/reconnect_options.hpp>
#include <proton/value.hpp>
#include <proton/types.hpp>

#include <iostream>
#include <map>
#include <string>

#include "fake_cpp11.hpp"

class reconnect_client : public proton::messaging_handler {
    std::string url;
    std::string address;
    std::vector<std::string> failovers;
    proton::sender sender;
    int sent;
    int expected;
    int received;

  public:
    reconnect_client(const std::string &u, const std::string& a, int c, const std::vector<std::string>& f) :
        url(u), address(a), failovers(f), sent(0), expected(c), received(0) {}

  private:
    void on_container_start(proton::container &c) OVERRIDE {
        proton::connection_options co;
        proton::reconnect_options ro;

        ro.failover_urls(failovers);
        co.reconnect(ro);
        c.connect(url, co);
    }

    void on_connection_open(proton::connection & c) OVERRIDE {
        c.open_receiver(address);
        c.open_sender(address);
        // reconnect we probably lost the last message sent
        sent = received;
        std::cout << "simple_recv listening on " << url << std::endl;
    }

    void on_message(proton::delivery &d, proton::message &msg) OVERRIDE {
        if (proton::coerce<int>(msg.id()) < received) {
            return; // Ignore duplicate
        }

        if (expected == 0 || received < expected) {
            std::cout << msg.body() << std::endl;
            received++;

            if (received == expected) {
                d.receiver().close();
                sender.close();
                d.connection().close();
            } else {
                // See if we can send any messages now
                send(sender);
            }
        }
    }

    void send(proton::sender& s) {
        // Only send with credit and only allow one outstanding message
        while (s.credit() && sent < received+1) {
            std::map<std::string, int> m;
            m["sequence"] = sent + 1;

            proton::message msg;
            msg.id(sent + 1);
            msg.body(m);

            std::cout << "Sending: " << sent+1 << std::endl;
            s.send(msg);
            sent++;
        }
    }

    void on_sender_open(proton::sender & s) OVERRIDE {
        sender = s;
    }

    void on_sendable(proton::sender &s) OVERRIDE {
        send(s);
    }
};

int main(int argc, const char** argv) {
    try {
        if (argc < 4) {
            std ::cerr <<
                "Usage: " << argv[0] << " CONNECTION-URL AMQP-ADDRESS MESSAGE-COUNT FAILOVER-URL...\n"
                "CONNECTION-URL: connection address, e.g.'amqp://127.0.0.1'\n"
                "AMQP-ADDRESS: AMQP node address, e.g. 'examples'\n"
                "MESSAGE-COUNT: number of messages to receive\n"
                "FAILOVER_URL...: zero or more failover urls\n";
            return 1;
        }
        const char *url = argv[1];
        const char *address = argv[2];
        int message_count = atoi(argv[3]);
        std::vector<std::string> failovers(&argv[4], &argv[argc]);

        reconnect_client client(url, address, message_count, failovers);
        proton::container(client).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
