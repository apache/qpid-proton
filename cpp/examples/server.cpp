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

#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>

#include <iostream>
#include <map>
#include <string>
#include <cctype>

#include "fake_cpp11.hpp"

class server : public proton::messaging_handler {
    std::string conn_url_;
    std::string addr_;
    proton::connection conn_;
    std::map<std::string, proton::sender> senders_;

  public:
    server(const std::string& u, const std::string& a) :
        conn_url_(u), addr_(a) {}

    void on_container_start(proton::container& c) OVERRIDE {
        conn_ = c.connect(conn_url_);
        conn_.open_receiver(addr_);

        std::cout << "Server connected to " << conn_url_ << std::endl;
    }

    std::string to_upper(const std::string& s) {
        std::string uc(s);
        size_t l = uc.size();

        for (size_t i=0; i<l; i++) {
            uc[i] = static_cast<char>(std::toupper(uc[i]));
        }

        return uc;
    }

    void on_message(proton::delivery&, proton::message& m) OVERRIDE {
        std::cout << "Received " << m.body() << std::endl;

        std::string reply_to = m.reply_to();
        proton::message reply;

        reply.to(reply_to);
        reply.body(to_upper(proton::get<std::string>(m.body())));
        reply.correlation_id(m.correlation_id());

        if (!senders_[reply_to]) {
            senders_[reply_to] = conn_.open_sender(reply_to);
        }

        senders_[reply_to].send(reply);
    }
};

int main(int argc, char** argv) {
    try {
        std::string conn_url = argc > 1 ? argv[1] : "//127.0.0.1:5672";
        std::string addr = argc > 2 ? argv[2] : "examples";

        server srv(conn_url, addr);
        proton::container(srv).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
