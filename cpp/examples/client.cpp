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
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/tracker.hpp>

#include <iostream>
#include <vector>

#include "fake_cpp11.hpp"

using proton::receiver_options;
using proton::source_options;

class client : public proton::messaging_handler {
  private:
    std::string url;
    std::vector<std::string> requests;
    proton::sender sender;
    proton::receiver receiver;

  public:
    client(const std::string &u, const std::vector<std::string>& r) : url(u), requests(r) {}

    void on_container_start(proton::container &c) OVERRIDE {
        sender = c.open_sender(url);
        // Create a receiver requesting a dynamically created queue
        // for the message source.
        receiver_options opts = receiver_options().source(source_options().dynamic(true));
        receiver = sender.connection().open_receiver("", opts);
    }

    void send_request() {
        proton::message req;
        req.body(requests.front());
        req.reply_to(receiver.source().address());
        sender.send(req);
    }

    void on_receiver_open(proton::receiver &) OVERRIDE {
        send_request();
    }

    void on_message(proton::delivery &d, proton::message &response) OVERRIDE {
        if (requests.empty()) return; // Spurious extra message!

        std::cout << requests.front() << " => " << response.body() << std::endl;
        requests.erase(requests.begin());

        if (!requests.empty()) {
            send_request();
        } else {
            d.connection().close();
        }
    }
};

int main(int argc, char **argv) {
    std::string url("127.0.0.1:5672/examples");
    example::options opts(argc, argv);

    opts.add_value(url, 'a', "address", "connect and send to URL", "URL");

    try {
        opts.parse();

        std::vector<std::string> requests;
        requests.push_back("Twas brillig, and the slithy toves");
        requests.push_back("Did gire and gymble in the wabe.");
        requests.push_back("All mimsy were the borogroves,");
        requests.push_back("And the mome raths outgrabe.");

        client c(url, requests);
        proton::container(c).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
