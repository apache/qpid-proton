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

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/delivery.hpp"
#include "proton/handler.hpp"
#include "proton/link.hpp"
#include "proton/message_id.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <map>

#include "fake_cpp11.hpp"

class simple_recv : public proton::handler {
  private:
    proton::url url;
    proton::receiver receiver;
    uint64_t expected;
    uint64_t received;

  public:
    simple_recv(const std::string &s, int c) : url(s), expected(c), received(0) {}

    void on_container_start(proton::container &c) override {
        receiver = c.open_receiver(url);
        std::cout << "simple_recv listening on " << url << std::endl;
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        if (msg.id().get<uint64_t>() < received) {
            return; // Ignore duplicate
        }

        if (expected == 0 || received < expected) {
            std::cout << msg.body() << std::endl;
            received++;

            if (received == expected) {
                d.link().close();
                d.connection().close();
            }
        }
    }
};

int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");

    int message_count = 100;
    options opts(argc, argv);

    opts.add_value(address, 'a', "address", "connect to and receive from URL", "URL");
    opts.add_value(message_count, 'm', "messages", "receive COUNT messages", "COUNT");

    try {
        opts.parse();

        simple_recv recv(address, message_count);
        proton::container(recv).run();

        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
