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
#include "proton/io/socket.hpp"
#include "proton/url.hpp"
#include "proton/handler.hpp"
#include "proton/connection.hpp"

#include <iostream>
#include <vector>

#include "../fake_cpp11.hpp"

class client : public proton::handler {
  private:
    proton::url url;
    std::vector<std::string> requests;
    proton::sender sender;
    proton::receiver receiver;

  public:
    client(const proton::url &u, const std::vector<std::string>& r) : url(u), requests(r) {}

    void on_connection_open(proton::connection &c) override {
        sender = c.open_sender(url.path());
        receiver = c.open_receiver("", proton::link_options().dynamic_address(true));
    }

    void send_request() {
        proton::message req;
        req.body(requests.front());
        req.reply_to(receiver.remote_source().address());
        sender.send(req);
    }

    void on_receiver_open(proton::receiver &) override {
        send_request();
    }

    void on_message(proton::delivery &d, proton::message &response) override {
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
    // Command line options
    std::string address("127.0.0.1:5672/examples");
    options opts(argc, argv);
    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");

    try {
        opts.parse();

        std::vector<std::string> requests;
        requests.push_back("Twas brillig, and the slithy toves");
        requests.push_back("Did gire and gymble in the wabe.");
        requests.push_back("All mimsy were the borogroves,");
        requests.push_back("And the mome raths outgrabe.");
        client handler(address, requests);
        proton::io::socket::engine(address, handler).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
