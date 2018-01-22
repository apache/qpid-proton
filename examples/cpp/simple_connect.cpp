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

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

class simple_connect : public proton::messaging_handler {
  private:
    std::string url;
    std::string user;
    std::string password;
    bool sasl;
    std::string mechs;
    proton::connection connection;

  public:
    simple_connect(const std::string &a, const std::string &u, const std::string &p, bool s, const std::string& ms) :
        url(a), user(u), password(p), sasl(s), mechs(ms) {}

    void on_container_start(proton::container &c) OVERRIDE {
        proton::connection_options co;
        if (!user.empty()) co.user(user);
        if (!password.empty()) co.password(password);
        if (sasl) co.sasl_enabled(true);
        if (!mechs.empty()) co.sasl_allowed_mechs(mechs);
        connection = c.connect(url, co);
    }

    void on_connection_open(proton::connection &c) OVERRIDE {
        c.close();
    }
};

int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");
    std::string user;
    std::string password;
    std::string mechs;
    bool sasl = false;
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(user, 'u', "user", "authenticate as USER", "USER");
    opts.add_value(password, 'p', "password", "authenticate with PASSWORD", "PASSWORD");
    opts.add_flag(sasl,'s', "sasl", "force SASL authentication with no user specified (Use for Kerberos/GSSAPI)");
    opts.add_value(mechs, 'm', "mechs", "allowed SASL mechanisms", "MECHS");

    try {
        opts.parse();

        simple_connect connect(address, user, password, sasl, mechs);
        proton::container(connect).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
