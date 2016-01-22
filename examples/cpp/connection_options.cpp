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
#include "proton/handler.hpp"
#include "proton/event.hpp"
#include "proton/url.hpp"
#include "proton/transport.hpp"

#include <iostream>

using proton::connection_options;

class handler_2 : public proton::handler {
    void on_connection_open(proton::event &e) {
        std::cout << "connection events going to handler_2" << std::endl;
        std::cout << "connection max_frame_size: " << e.connection().transport().max_frame_size() <<
            ", idle timeout: " << e.connection().transport().idle_timeout() << std::endl;
        e.connection().close();
    }
};

class main_handler : public proton::handler {
  private:
    proton::url url;
    handler_2 conn_handler;

  public:
    main_handler(const proton::url& u) : url(u) {}

    void on_start(proton::event &e) {
        // Connection options for this connection.  Merged with and overriding the container's
        // client_connection_options() settings.
        e.container().connect(url, connection_options().handler(&conn_handler).max_frame_size(2468));
    }

    void on_connection_open(proton::event &e) {
        std::cout << "unexpected connection event on main handler" << std::endl;
        e.connection().close();
    }
};

int main(int argc, char **argv) {
    try {
        std::string url = argc > 1 ? argv[1] : "127.0.0.1:5672/examples";
        main_handler handler(url);
        proton::container container(handler);
        // Global connection options for future connections on container.
        container.client_connection_options(connection_options().max_frame_size(12345).idle_timeout(proton::duration(15000)));
        container.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
