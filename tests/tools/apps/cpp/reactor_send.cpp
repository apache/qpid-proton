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

#include "proton/binary.hpp"
#include "proton/connection.hpp"
#include "proton/default_container.hpp"
#include "proton/codec/decoder.hpp"
#include "proton/delivery.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/tracker.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

class reactor_send : public proton::messaging_handler {
  private:
    std::string url_;
    proton::message message_;
    std::string reply_to_;
    int sent_;
    int confirmed_;
    int total_;
    int received_;
    size_t received_bytes_;
    proton::binary received_content_;
    bool replying_;
    proton::message_id id_value_;
  public:

    reactor_send(const std::string &url, int c, int size, bool replying)
        : url_(url), sent_(0), confirmed_(0), total_(c),
          received_(0), received_bytes_(0), replying_(replying) {
        if (replying_)
            message_.reply_to("localhost/test");
        proton::binary content;
        content.assign((size_t) size, 'X');
        message_.body(content);
    }

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        c.receiver_options(proton::receiver_options().credit_window(1024));
        c.open_sender(url_);
    }

    void on_sendable(proton::sender &sender) PN_CPP_OVERRIDE {
        while (sender.credit() && sent_ < total_) {
            id_value_ = sent_ + 1;
            message_.correlation_id(id_value_);
            message_.creation_time(proton::timestamp::now());
            sender.send(message_);
            sent_++;
        }
    }

    void on_tracker_accept(proton::tracker &t) PN_CPP_OVERRIDE {
        confirmed_++;
        t.settle();
        if (confirmed_ == total_) {
            std::cout << "all messages confirmed" << std::endl;
            if (!replying_)
                t.connection().close();
        }
    }

    void on_message(proton::delivery &d, proton::message &msg) PN_CPP_OVERRIDE {
        received_content_ = proton::get<proton::binary>(msg.body());
        received_bytes_ += received_content_.size();
        if (received_ < total_) {
            received_++;
        }
        d.settle();
        if (received_ == total_) {
            d.receiver().close();
            d.connection().close();
        }
    }

    void on_transport_close(proton::transport &) PN_CPP_OVERRIDE {
        sent_ = confirmed_;
    }
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("127.0.0.1:5672/cpp_tests");
    int message_count = 10;
    int message_size = 100;
    bool replying = false;
    example::options opts(argc, argv);
    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(message_count, 'c', "messages", "send COUNT messages", "COUNT");
    opts.add_value(message_size, 'b', "bytes", "send binary messages BYTES long", "BYTES");
    opts.add_value(replying, 'R', "replying", "process reply messages", "REPLYING");
    try {
        opts.parse();
        reactor_send send(address, message_count, message_size, replying);
        proton::default_container(send).run();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
