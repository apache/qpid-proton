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
#include "proton/container.hpp"
#include "proton/event.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/connection.hpp"

#include <iostream>
#include <vector>

class client : public proton::messaging_handler {
  private:
    proton::url url;
    std::vector<std::string> requests;
    proton::sender sender;
    proton::receiver receiver;

  public:
    client(const proton::url &u, const std::vector<std::string>& r) : url(u), requests(r) {}

    void on_start(proton::event &e) {
        sender = e.container().open_sender(url);
        // Create a receiver with a dynamically chosen unique address.
        receiver = sender.connection().open_receiver("", proton::link_options().dynamic_address(true));
    }

    void send_request() {
        proton::message req;
        req.body(requests.front());
        req.reply_to(receiver.remote_source().address());
        sender.send(req);
    }

    void on_link_open(proton::event &e) {
        if (e.link() == receiver)
            send_request();
    }

    void on_message(proton::event &e) {
        if (requests.empty()) return; // Spurious extra message!
        proton::message& response = e.message();
        std::cout << requests.front() << " => " << response.body() << std::endl;
        requests.erase(requests.begin());
        if (!requests.empty()) {
            send_request();
        } else {
            e.connection().close();
        }
    }
};

int main(int argc, char **argv) {
    // Command line options
    proton::url url("127.0.0.1:5672/examples");
    options opts(argc, argv);
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
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
