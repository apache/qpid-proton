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
#include <proton/default_container.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/tracker.hpp>
#include <proton/url.hpp>

#include <iostream>
#include <map>
#include <string>
#include <cctype>

#include "fake_cpp11.hpp"

class server : public proton::messaging_handler {
  private:
    typedef std::map<std::string, proton::sender> sender_map;
    proton::url url;
    proton::connection connection;
    sender_map senders;

  public:
    server(const std::string &u) : url(u) {}

    void on_container_start(proton::container &c) OVERRIDE {
        connection = c.connect(url);
        connection.open_receiver(url.path());

        std::cout << "server connected to " << url << std::endl;
    }

    std::string to_upper(const std::string &s) {
        std::string uc(s);
        size_t l = uc.size();
        for (size_t i=0; i<l; i++)
            uc[i] = static_cast<char>(std::toupper(uc[i]));
        return uc;
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << "Received " << m.body() << std::endl;

        std::string reply_to = m.reply_to();
        proton::message reply;

        reply.to(reply_to);
        reply.body(to_upper(proton::get<std::string>(m.body())));
        reply.correlation_id(m.correlation_id());

        if (!senders[reply_to]) {
            senders[reply_to] = connection.open_sender(reply_to);
        }

        senders[reply_to].send(reply);
    }
};

int main(int argc, char **argv) {
    std::string address("amqp://0.0.0.0:5672/examples");
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "listen on URL", "URL");

    try {
        opts.parse();

        server srv(address);
        proton::default_container(srv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
