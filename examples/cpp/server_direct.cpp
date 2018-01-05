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

#include <proton/container.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/tracker.hpp>

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <cctype>

#include "fake_cpp11.hpp"

class server : public proton::messaging_handler {
  private:
    class listener_ready_handler : public proton::listen_handler {
        void on_open(proton::listener& l) OVERRIDE {
            std::cout << "listening on " << l.port() << std::endl;
        }
    };

    typedef std::map<std::string, proton::sender> sender_map;
    listener_ready_handler listen_handler;
    std::string url;
    sender_map senders;
    int address_counter;

  public:
    server(const std::string &u) : url(u), address_counter(0) {}

    void on_container_start(proton::container &c) OVERRIDE {
        c.listen(url, listen_handler);
    }

    std::string to_upper(const std::string &s) {
        std::string uc(s);
        size_t l = uc.size();

        for (size_t i=0; i<l; i++)
            uc[i] = static_cast<char>(std::toupper(uc[i]));

        return uc;
    }

    std::string generate_address() {
        std::ostringstream addr;
        addr << "server" << address_counter++;

        return addr.str();
    }

    void on_sender_open(proton::sender &sender) OVERRIDE {
        if (sender.source().dynamic()) {
            std::string addr = generate_address();
            sender.open(proton::sender_options().source(proton::source_options().address(addr)));
            senders[addr] = sender;
        }
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << "Received " << m.body() << std::endl;

        std::string reply_to = m.reply_to();
        sender_map::iterator it = senders.find(reply_to);

        if (it == senders.end()) {
            std::cout << "No link for reply_to: " << reply_to << std::endl;
        } else {
            proton::sender sender = it->second;
            proton::message reply;

            reply.to(reply_to);
            reply.body(to_upper(proton::get<std::string>(m.body())));
            reply.correlation_id(m.correlation_id());

            sender.send(reply);
        }
    }
};

int main(int argc, char **argv) {
    std::string address("amqp://127.0.0.1:5672/examples");
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "listen on URL", "URL");

    try {
        opts.parse();

        server srv(address);
        proton::container(srv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
