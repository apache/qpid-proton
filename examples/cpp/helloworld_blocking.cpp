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
#include "proton/blocking_sender.hpp"

#include <iostream>

class hello_world_blocking : public proton::messaging_handler {
  private:
    proton::url url;

  public:

    hello_world_blocking(const proton::url& u) : url(u) {}

    void on_start(proton::event &e) {
        proton::connection conn = e.container().connect(url);
        e.container().create_receiver(conn, url.path());
    }

    void on_message(proton::event &e) {
        std::cout << e.message().body() << std::endl;
        e.connection().close();
    }

};

int main(int argc, char **argv) {
    try {
        proton::url url(argc > 1 ? argv[1] : "127.0.0.1:5672/examples");

        proton::blocking_connection conn(url);
        proton::blocking_sender sender = conn.create_sender(url.path());
        proton::message m;
        m.body("Hello World!");
        sender.send(m);
        conn.close();

        // TODO Temporary hack until blocking receiver available
        hello_world_blocking hw(url);
        proton::container(hw).run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
