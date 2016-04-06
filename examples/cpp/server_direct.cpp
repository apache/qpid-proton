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

#include "proton/acceptor.hpp"
#include "proton/container.hpp"
#include "proton/handler.hpp"
#include "proton/sender.hpp"
#include "proton/url.hpp"

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <cctype>

#include "fake_cpp11.hpp"

class server : public proton::handler {
  private:
    typedef std::map<std::string, proton::sender> sender_map;
    proton::url url;
    sender_map senders;
    int address_counter;

  public:
    server(const std::string &u) : url(u), address_counter(0) {}

    void on_container_start(proton::container &c) override {
        c.listen(url);
        std::cout << "server listening on " << url << std::endl;
    }

    std::string to_upper(const std::string &s) {
        std::string uc(s);
        size_t l = uc.size();

        for (size_t i=0; i<l; i++) uc[i] = std::toupper(uc[i]);

        return uc;
    }

    std::string generate_address() {
        std::ostringstream addr;
        addr << "server" << address_counter++;

        return addr.str();
    }

    void on_sender_open(proton::sender &sender) override {
        if (sender.remote_source().dynamic()) {
            sender.local_source().address(generate_address());
            senders[sender.local_source().address()] = sender;
        }
    }

    void on_message(proton::delivery &d, proton::message &m) override {
        std::cout << "Received " << m.body() << std::endl;

        std::string reply_to = m.reply_to();
        sender_map::iterator it = senders.find(reply_to);

        if (it == senders.end()) {
            std::cout << "No link for reply_to: " << reply_to << std::endl;
        } else {
            proton::sender sender = it->second;
            proton::message reply;

            reply.address(reply_to);
            reply.body(to_upper(proton::get<std::string>(m.body())));
            reply.correlation_id(m.correlation_id());

            sender.send(reply);
        }
    }
};

int main(int argc, char **argv) {
    std::string address("amqp://127.0.0.1:5672/examples");
    options opts(argc, argv);

    opts.add_value(address, 'a', "address", "listen on URL", "URL");

    try {
        opts.parse();

        server srv(address);
        proton::container(srv).run();

        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
