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

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/handler.hpp"
#include "proton/url.hpp"
#include "proton/link_options.hpp"

#include <iostream>

#include "fake_cpp11.hpp"

class selected_recv : public proton::handler {
  private:
    proton::url url;

  public:
    selected_recv(const proton::url& u) : url(u) {}

    void on_container_start(proton::container &c) override {
        proton::connection conn = c.connect(url);
        conn.open_receiver(url.path(), proton::link_options().selector("colour = 'green'"));
    }

    void on_message(proton::delivery &d, proton::message &m) override {
        std::cout << m.body() << std::endl;
    }
};

int main(int argc, char **argv) {
    try {
        std::string url = argc > 1 ? argv[1] : "127.0.0.1:5672/examples";

        selected_recv recv(url);
        proton::container(recv).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
