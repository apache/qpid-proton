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

#include "proton/Container.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/Connection.hpp"

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


using namespace proton::reactor;

class Send : public MessagingHandler {
  private:
    std::string url;
    int sent;
    int confirmed;
    int total;
  public:

    Send(const std::string &s, int c) : url(s), sent(0), confirmed(0), total(c) {}

    void onStart(Event &e) {
        e.getContainer().createSender(url);
    }

    void onSendable(Event &e) {
        Sender sender = e.getSender();
        while (sender.getCredit() && sent < total) {
            Message msg;
            msg.setId(sent + 1);
            // TODO: fancy map body content as in Python example.  Simple binary for now.
            const char *bin = "some arbitrary binary data";
            msg.setBody(bin, strlen(bin));
            sender.send(msg);
            sent++;
        }
    }

    void onAccepted(Event &e) {
        confirmed++;
        if (confirmed == total) {
            std::cout << "all messages confirmed" << std::endl;
            e.getConnection().close();
        }
    }

    void onDisconnected(Event &e) {
        sent = confirmed;
    }
};

static void parse_options(int argc, char **argv, int &count, std::string &addr);

int main(int argc, char **argv) {
    try {
        int messageCount = 100;
        std::string address("localhost:5672/examples");
        parse_options(argc, argv, messageCount, address);
        Send send(address, messageCount);
        Container(send).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}


static void usage() {
    std::cout << "Usage: simple_send -m message_count -a address:" << std::endl;
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
