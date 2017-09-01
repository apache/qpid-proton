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
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/url.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

using proton::source_options;

class browser : public proton::messaging_handler {
  private:
    proton::url url;

  public:
    browser(const std::string& u) : url(u) {}

    void on_container_start(proton::container &c) OVERRIDE {
        proton::connection conn = c.connect(url);
        source_options browsing = source_options().distribution_mode(proton::source::COPY);
        conn.open_receiver(url.path(), proton::receiver_options().source(browsing));
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << m.body() << std::endl;
    }
};

int main(int argc, char **argv) {
    try {
        std::string url = argc > 1 ? argv[1] : "127.0.0.1:5672/examples";

        browser b(url);
        proton::container(b).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
