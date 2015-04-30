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

#include "proton/cpp/MessagingHandler.h"
#include "proton/cpp/Container.h"

//#include "proton/cpp/Acceptor.h"
#include <iostream>


using namespace proton::reactor;


class HelloWorldDirect : public MessagingHandler {
  private:
    std::string url;
    Acceptor acceptor;
  public:

    HelloWorldDirect(const std::string &u) : url(u) {}

    void onStart(Event &e) {
        acceptor = e.getContainer().listen(url);
        e.getContainer().createSender(url);
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
    }

    void onAccepted(Event &e) {
        e.getConnection().close();
    }

    void onConnectionClosed(Event &e) {
        acceptor.close();
    }

};

int main(int argc, char **argv) {
    HelloWorldDirect hwd("localhost:8888/examples");
    Container(hwd).run();
}
