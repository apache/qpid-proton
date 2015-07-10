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
#include "proton/link.hpp"
#include "proton/url.hpp"

#include <iostream>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



class direct_recv : public proton::messaging_handler {
  private:
    proton::url url;
    int expected;
    int received;
    proton::acceptor acceptor;

  public:
    direct_recv(const std::string &s, int c) : url(s), expected(c), received(0) {}

    void on_start(proton::event &e) {
        acceptor = e.container().listen(url);
        std::cout << "direct_recv listening on " << url << std::endl;
    }

    void on_message(proton::event &e) {
        proton::message msg = e.message();
        proton::value id = msg.id();
        if (id.type() == proton::ULONG) {
            if (id.get<int>() < received)
                return; // ignore duplicate
        }
        if (expected == 0 || received < expected) {
            std::cout << msg.body() << std::endl;
            received++;
        }
        if (received == expected) {
            e.receiver().close();
            e.connection().close();
            if (acceptor) acceptor.close();
        }
    }
};

static void parse_options(int argc, char **argv, int &count, std::string &addr);

int main(int argc, char **argv) {
    try {
        int message_count = 100;
        std::string address("127.0.0.1:5672/examples");
        parse_options(argc, argv, message_count, address);
        direct_recv recv(address, message_count);
        proton::container(recv).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}


static void usage() {
    std::cout << "Usage: direct_recv -m message_count -a address:" << std::endl;
    exit (1);
}


static void parse_options(int argc, char **argv, int &count, std::string &addr) {
    int c, i;
    for (i = 1; i < argc; i++) {
        if (strlen(argv[i]) == 2 && argv[i][0] == '-') {
            c = argv[i][1];
            const char *nextarg = i < argc ? argv[i+1] : NULL;

            switch (c) {
            case 'a':
                if (!nextarg) usage();
                addr = nextarg;
                i++;
                break;
            case 'm':
                if (!nextarg) usage();
                unsigned newc;
                if (sscanf( nextarg, "%d", &newc) != 1) usage();
                count = newc;
                i++;
                break;
            default:
                usage();
            }
        }
        else usage();
    }
}
