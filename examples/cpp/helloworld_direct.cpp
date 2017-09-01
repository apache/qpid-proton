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
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/sender.hpp>
#include <proton/tracker.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

class hello_world_direct : public proton::messaging_handler {
  private:
    std::string url;
    proton::listener listener;

  public:
    hello_world_direct(const std::string& u) : url(u) {}

    void on_container_start(proton::container &c) OVERRIDE {
        listener = c.listen(url);
        c.open_sender(url);
    }

    // Send one message and close sender
    void on_sendable(proton::sender &s) OVERRIDE {
        proton::message m("Hello World!");
        s.send(m);
        s.close();
    }

    // Receive one message and stop listener
    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << m.body() << std::endl;
        listener.stop();
    }

    // After receiving acknowledgement close connection
    void on_tracker_accept(proton::tracker &t) OVERRIDE {
        t.connection().close();
    }
};

int main(int argc, char **argv) {
    try {
        // Pick an "unusual" port since we are going to be talking to
        // ourselves, not a broker.
        std::string url = argc > 1 ? argv[1] : "127.0.0.1:8888/examples";

        hello_world_direct hwd(url);
        proton::container(hwd).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
