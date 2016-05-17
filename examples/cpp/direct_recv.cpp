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
#include "proton/default_container.hpp"
#include "proton/delivery.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/link.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <map>

#include <proton/config.hpp>

class direct_recv : public proton::messaging_handler {
  private:
    std::string url;
    proton::listener listener;
    uint64_t expected;
    uint64_t received;

  public:
    direct_recv(const std::string &s, int c) : url(s), expected(c), received(0) {}

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        listener = c.listen(url);
        std::cout << "direct_recv listening on " << url << std::endl;
    }

    void on_message(proton::delivery &d, proton::message &msg) PN_CPP_OVERRIDE {
        if (proton::coerce<uint64_t>(msg.id()) < received) {
            return; // Ignore duplicate
        }

        if (expected == 0 || received < expected) {
            std::cout << msg.body() << std::endl;
            received++;
        }

        if (received == expected) {
            d.receiver().close();
            d.connection().close();
            listener.stop();
        }
    }
};

int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");
    int message_count = 100;
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "listen and receive on URL", "URL");
    opts.add_value(message_count, 'm', "messages", "receive COUNT messages", "COUNT");

    try {
        opts.parse();

        direct_recv recv(address, message_count);
        proton::default_container(recv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
