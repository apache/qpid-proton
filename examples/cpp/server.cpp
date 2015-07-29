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
#include "proton/messaging_handler.hpp"
#include "proton/url.hpp"

#include <iostream>
#include <map>
#include <string>

class server : public proton::messaging_handler {
  private:
    typedef std::map<std::string, proton::sender> sender_map;
    proton::url url;
    proton::connection connection;
    sender_map senders;

  public:

    server(const proton::url &u) : url(u) {}

    void on_start(proton::event &e) {
        connection = e.container().connect(url);
        e.container().create_receiver(connection, url.path());
        std::cout << "server listening on " << url << std::endl;
    }

    std::string to_upper(const std::string &s) {
        std::string uc(s);
        size_t l = uc.size();
        for (size_t i=0; i<l; i++) uc[i] = std::toupper(uc[i]);
        return uc;
    }

    // TODO: on_connection_opened() and ANONYMOUS-RELAY

    void on_message(proton::event &e) {
        proton::sender sender;
        proton::message msg = e.message();
        std::cout << "Received " << msg.body() << std::endl;
        std::string sender_id = msg.reply_to();
        sender_map::iterator it = senders.find(sender_id);
        if (it == senders.end()) {
            sender = e.container().create_sender(connection, sender_id);
            senders[sender_id] = sender;
        }
        else {
            sender = it->second;
        }
        proton::message reply;
        reply.body(to_upper(msg.body().get<std::string>()));
        reply.correlation_id(msg.correlation_id());
        reply.address(sender_id);
        sender.send(reply);
    }
};

int main(int argc, char **argv) {
    // Command line options
    proton::url url("amqp://127.0.0.1:5672/examples");
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        server server(url);
        proton::container(server).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
