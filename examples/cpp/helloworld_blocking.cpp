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

#include "proton/messaging_handler.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/blocking_sender.hpp"
#include "proton/blocking_receiver.hpp"
#include "proton/duration.hpp"

#include <iostream>

int main(int argc, char **argv) {
    try {
        proton::url url(argc > 1 ? argv[1] : "127.0.0.1:5672/examples");
        proton::blocking_connection conn(url);
        proton::blocking_receiver receiver(conn, url.path());
        proton::blocking_sender sender(conn, url.path());

        proton::message_value m;
        m.body("Hello World!");
        sender.send(m);

        proton::duration timeout(30000);
        proton::message_value m2 = receiver.receive(timeout);
        std::cout << m2.body() << std::endl;
        receiver.accept();

        conn.close();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
