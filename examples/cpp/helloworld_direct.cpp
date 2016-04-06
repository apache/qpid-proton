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

#include "proton/acceptor.hpp"
#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/delivery.hpp"
#include "proton/handler.hpp"
#include "proton/sender.hpp"

#include <iostream>

#include "fake_cpp11.hpp"

class hello_world_direct : public proton::handler {
  private:
    proton::url url;
    proton::acceptor acceptor;

  public:
    hello_world_direct(const proton::url& u) : url(u) {}

    void on_container_start(proton::container &c) override {
        acceptor = c.listen(url);
        c.open_sender(url);
    }

    void on_sendable(proton::sender &s) override {
        proton::message m("Hello World!");
        s.send(m);
        s.close();
    }

    void on_message(proton::delivery &d, proton::message &m) override {
        std::cout << m.body() << std::endl;
    }

    void on_delivery_accept(proton::delivery &d) override {
        d.connection().close();
    }

    void on_connection_close(proton::connection &) override {
        acceptor.close();
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
