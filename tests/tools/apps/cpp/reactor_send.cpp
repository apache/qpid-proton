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

#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/connection.hpp"
#include "proton/decoder.hpp"
#include "proton/event.hpp"
#include "proton/reactor.h"
#include "proton/value.hpp"

#include <iostream>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



class reactor_send : public proton::messaging_handler {
  private:
    proton::url url_;
    proton::message message_;
    std::string reply_to_;
    int sent_;
    int confirmed_;
    int total_;
    int received_;
    size_t received_bytes_;
    proton::amqp_binary received_content_;
    bool replying_;
    proton::message_id id_value_;
    proton::reactor reactor_;
  public:

    reactor_send(const std::string &url, int c, int size, bool replying)
        : messaging_handler(1024), // prefetch=1024
          url_(url), sent_(0), confirmed_(0), total_(c),
          received_(0), received_bytes_(0), replying_(replying) {
        if (replying_)
            message_.reply_to("localhost/test");
        proton::amqp_binary content;
        content.assign((size_t) size, 'X');
        message_.body(content);
    }

    void on_start(proton::event &e) {
        e.container().open_sender(url_);
        reactor_ = e.container().reactor();
    }

    void on_sendable(proton::event &e) {
        proton::sender sender = e.sender();

        while (sender.credit() && sent_ < total_) {
            id_value_ = sent_ + 1;
            message_.correlation_id(id_value_);
            proton::amqp_timestamp reactor_now(reactor_.now());
            message_.creation_time(reactor_now);
            sender.send(message_);
            sent_++;
        }
    }

    void on_accepted(proton::event &e) {
        confirmed_++;
        if (confirmed_ == total_) {
            std::cout << "all messages confirmed" << std::endl;
            if (!replying_)
                e.connection().close();
        }
    }

    void on_message(proton::event &e) {
        proton::message &msg = e.message();
        msg.body().decode() >> received_content_;
        received_bytes_ += received_content_.size();
        if (received_ < total_) {
            received_++;
        }
        if (received_ == total_) {
            e.receiver().close();
            e.connection().close();
        }
    }

    void on_disconnected(proton::event &e) {
        sent_ = confirmed_;
    }
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("127.0.0.1:5672/cpp_tests");
    int message_count = 10;
    int message_size = 100;
    bool replying = false;
    options opts(argc, argv);
    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(message_count, 'c', "messages", "send COUNT messages", "COUNT");
    opts.add_value(message_size, 'b', "bytes", "send binary messages BYTES long", "BYTES");
    opts.add_value(replying, 'R', "replying", "process reply messages", "REPLYING");
    try {
        opts.parse();
        reactor_send send(address, message_count, message_size, replying);
        proton::container(send).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
