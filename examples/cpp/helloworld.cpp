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

#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"

#include <iostream>

class hello_world : public proton::messaging_handler {
  private:
    std::string server;
    std::string address;
  public:

    hello_world(const std::string &s, const std::string &addr) : server(s), address(addr) {}

    void on_start(proton::event &e) {
        proton::connection conn = e.container().connect(server);
        e.container().create_receiver(conn, address);
        e.container().create_sender(conn, address);
    }

    void on_sendable(proton::event &e) {
        proton::message m;
        m.body("Hello World!");
        e.sender().send(m);
        e.sender().close();
    }

    void on_message(proton::event &e) {
        std::string s;
        proton::value v(e.message().body());
        std::cout << v.get<std::string>() << std::endl;
        e.connection().close();
    }

};

int main(int argc, char **argv) {
    try {
        std::string server = argc > 1 ? argv[1] : ":5672";
        std::string addr = argc > 2 ? argv[2] : "examples";
        hello_world hw(server, addr);
        proton::container(hw).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
