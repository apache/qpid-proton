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

#include "proton/cpp/Container.h"
#include "proton/cpp/MessagingHandler.h"
#include "proton/cpp/Link.h"

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


using namespace proton::reactor;

class Recv : public MessagingHandler {
  private:
    std::string url;
    int expected;
    int received;
  public:

    Recv(const std::string &s, int c) : url(s), expected(c), received(0) {}

    void onStart(Event &e) {
        e.getContainer().createReceiver(url);
        std::cout << "simple_recv listening on " << url << std::endl;
    }

    void onMessage(Event &e) {
        int64_t id = 0;
        Message msg = e.getMessage();
        if (msg.getIdType() == PN_ULONG) {
            id = msg.getId();
            if (id < received)
                return; // ignore duplicate
        }
        if (expected == 0 || received < expected) {
            std::cout << '[' << id << "]: " << msg.getBody() << std::endl;
            received++;
            if (received == expected) {
                e.getReceiver().close();
                e.getConnection().close();
            }
        }
    }
};

static void parse_options(int argc, char **argv, int &count, std::string &addr);

int main(int argc, char **argv) {
    try {
        int messageCount = 100;
        std::string address("localhost:5672/examples");
        parse_options(argc, argv, messageCount, address);
        Recv recv(address, messageCount);
        Container(recv).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}


static void usage() {
    std::cout << "Usage: simple_recv -m message_count -a address:" << std::endl;
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
