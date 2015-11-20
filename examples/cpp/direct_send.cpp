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

#include "proton/acceptor.hpp"
#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <map>

class simple_send : public proton::messaging_handler {
  private:
    proton::url url;
    int sent;
    int confirmed;
    int total;
    proton::acceptor acceptor;
  public:

    simple_send(const std::string &s, int c) : url(s), sent(0), confirmed(0), total(c) {}

    void on_start(proton::event &e) {
        acceptor = e.container().listen(url);
        std::cout << "direct_send listening on " << url << std::endl;
    }

    void on_sendable(proton::event &e) {
        proton::sender sender = e.sender();
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

    void on_accepted(proton::event &e) {
        confirmed++;
        if (confirmed == total) {
            std::cout << "all messages confirmed" << std::endl;
            e.connection().close();
            acceptor.close();
        }
    }

    void on_disconnected(proton::event &e) {
        sent = confirmed;
    }
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("127.0.0.1:5672/examples");
    int message_count = 100;
    options opts(argc, argv);
    opts.add_value(address, 'a', "address", "listen and send on URL", "URL");
    opts.add_value(message_count, 'm', "messages", "send COUNT messages", "COUNT");
    try {
        opts.parse();
        simple_send send(address, message_count);
        proton::container(send).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
