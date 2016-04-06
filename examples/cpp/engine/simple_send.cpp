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

#include "proton/io/socket.hpp"
#include "proton/url.hpp"
#include "proton/handler.hpp"
#include "proton/connection.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <map>

#include "../fake_cpp11.hpp"

class simple_send : public proton::handler {
  private:
    proton::url url;
    int sent;
    int confirmed;
    int total;
  public:

    simple_send(const std::string &s, int c) : url(s), sent(0), confirmed(0), total(c) {}

    void on_connection_open(proton::connection &c) override {
        c.open_sender(url.path());
    }

    void on_sendable(proton::sender &sender) override {
        while (sender.credit() && sent < total) {
            proton::message msg;
            msg.id(sent + 1);
            std::map<std::string, int> m;
            m["sequence"] = sent+1;
            msg.body(m);
            sender.send(msg);
            sent++;
        }
    }

    void on_delivery_accept(proton::delivery &d) override {
        confirmed++;
        if (confirmed == total) {
            std::cout << "all messages confirmed" << std::endl;
            d.connection().close();
        }
    }

    void on_transport_close(proton::transport &) override {
        sent = confirmed;
    }
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("127.0.0.1:5672/examples");
    int message_count = 100;
    options opts(argc, argv);
    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(message_count, 'm', "messages", "send COUNT messages", "COUNT");
    try {
        opts.parse();
        simple_send handler(address, message_count);
        proton::io::socket::engine(address, handler).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
