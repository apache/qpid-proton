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

#include "proton/cpp/Container.h"
#include "proton/cpp/MessagingHandler.h"

#include <iostream>


using namespace proton::reactor;

class HelloWorld : public MessagingHandler {
  private:
    std::string server;
    std::string address;
  public:

    HelloWorld(const std::string &s, const std::string &addr) : server(s), address(addr) {}

    void onStart(Event &e) {
        Connection conn = e.getContainer().connect(server);
        e.getContainer().createReceiver(conn, address);
        e.getContainer().createSender(conn, address);
    }

    void onSendable(Event &e) {
        Message m;
        m.setBody("Hello World!");
        e.getSender().send(m);
        e.getSender().close();
    }

    void onMessage(Event &e) {
        std::string body = e.getMessage().getBody();
        std::cout << body << std::endl;
        e.getConnection().close();
    }

};

int main(int argc, char **argv) {
    try {
        std::string server = argc > 1 ? argv[1] : ":5672";
        std::string addr = argc > 2 ? argv[2] : "examples";
        HelloWorld hw(server, addr);
        Container(hw).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
