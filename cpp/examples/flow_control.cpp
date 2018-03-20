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
#include <proton/delivery.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/tracker.hpp>

#include <iostream>
#include <sstream>

#include "fake_cpp11.hpp"

namespace {

bool verbose = true;

void verify(bool success, const std::string &msg) {
    if (!success)
        throw std::runtime_error("example failure:" + msg);
    else {
        std::cout << "success: " << msg << std::endl;
        if (verbose) std::cout << std::endl;
    }
}

}

// flow_sender manages the incoming connection and acts as the message sender.
class flow_sender : public proton::messaging_handler {
  private:
    int available;  // Number of messages the sender may send assuming sufficient credit.
    int sequence;

  public:
    flow_sender() : available(0), sequence(0) {}

    void send_available_messages(proton::sender &s) {
        for (int i = sequence; available && s.credit() > 0; i++) {
            std::ostringstream mbody;
            mbody << "flow_sender message " << sequence++;
            proton::message m(mbody.str());
            s.send(m);
            available--;
        }
    }

    void on_sendable(proton::sender &s) OVERRIDE {
        if (verbose)
            std::cout << "flow_sender in \"on_sendable\" with credit " << s.credit()
                      << " and " << available << " available messages" << std::endl;
        send_available_messages(s);
    }

    void on_sender_drain_start(proton::sender &s) OVERRIDE {
        if (verbose)
            std::cout << "flow_sender in \"on_drain_start\" with credit " << s.credit()
                      << " and " << available << " available messages" << std::endl;
        send_available_messages(s);
        if (s.credit()) {
            s.return_credit(); // return the rest
        }
    }

    void set_available(int n) { available = n; }
};

class flow_receiver : public proton::messaging_handler {
  public:
    int stage;
    int received;
    flow_sender &sender;

    flow_receiver(flow_sender &s) : stage(0), received(0), sender(s) {}

    void example_setup(int n) {
        received = 0;
        sender.set_available(n);
    }

    void run_stage(proton::receiver &r, const std::string &caller) {
        // Serialize the progression of the flow control examples.
        switch (stage) {
        case 0:
            if (verbose) std::cout << "Example 1.  Simple use of credit." << std::endl;
            // TODO: add timeout callbacks, show no messages until credit.
            example_setup(2);
            r.add_credit(2);
            break;
        case 1:
            if (r.credit() > 0) return;
            verify(received == 2, "Example 1: simple credit");

            if (verbose) std::cout << "Example 2.   Use basic drain, sender has 3 \"immediate\" messages." << std::endl;
            example_setup(3);
            r.add_credit(5); // ask for up to 5
            r.drain();       // but only use what's available
            break;
        case 2:
            if (caller == "on_message") return;
            if (caller == "on_receiver_drain_finish") {
                // Note that unused credit of 2 at sender is returned and is now 0.
                verify(received == 3 && r.credit() == 0, "Example 2: basic drain");

                if (verbose) std::cout << "Example 3. Drain use with no credit." << std::endl;
                example_setup(0);
                r.drain();
                break;
            }
            verify(false, "example 2 run_stage");
            return;

        case 3:
            verify(caller == "on_receiver_drain_finish" && received == 0, "Example 3: drain without credit");

            if (verbose) std::cout << "Example 4. Show using high(10)/low(3) watermark for 25 messages." << std::endl;
            example_setup(25);
            r.add_credit(10);
            break;

        case 4:
            if (received < 25) {
                // Top up credit as needed.
                uint32_t credit = r.credit();
                if (credit <= 3) {
                    uint32_t new_credit = 10;
                    uint32_t remaining = 25 - received;
                    if (new_credit > remaining)
                        new_credit = remaining;
                    if (new_credit > credit) {
                        r.add_credit(new_credit - credit);
                        if (verbose)
                            std::cout << "flow_receiver adding credit for " << new_credit - credit
                                      << " messages" << std::endl;
                    }
                }
                return;
            }

            verify(received == 25 && r.credit() == 0, "Example 4: high/low watermark");
            r.connection().close();
            break;

        default:
            throw std::runtime_error("run_stage sequencing error");
        }
        stage++;
    }

    void on_receiver_open(proton::receiver &r) OVERRIDE {
        run_stage(r, "on_receiver_open");
    }

    void on_message(proton::delivery &d, proton::message &m) OVERRIDE {
        if (verbose)
            std::cout << "flow_receiver in \"on_message\" with " << m.body() << std::endl;
        proton::receiver r(d.receiver());
        received++;
        run_stage(r, "on_message");
    }

    void on_receiver_drain_finish(proton::receiver &r) OVERRIDE {
        if (verbose)
            std::cout << "flow_receiver in \"on_receiver_drain_finish\"" << std::endl;
        run_stage(r, "on_receiver_drain_finish");
    }
};

class flow_listener : public proton::listen_handler {
    proton::connection_options opts;
  public:
    flow_listener(flow_sender& sh) {
        opts.handler(sh);
    }

    void on_open(proton::listener& l) OVERRIDE {
        std::ostringstream url;
        url << "//:" << l.port() << "/example"; // Connect to the actual listening port
        l.container().connect(url.str());
    }

    proton::connection_options on_accept(proton::listener&) OVERRIDE { return opts; }
};

class flow_control : public proton::messaging_handler {
  private:
    proton::listener listener;
    flow_sender send_handler;
    flow_receiver receive_handler;
    flow_listener listen_handler;

  public:
    flow_control() : receive_handler(send_handler), listen_handler(send_handler) {}

    void on_container_start(proton::container &c) OVERRIDE {
        // Listen on a dynamic port on the local host.
        listener = c.listen("//:0", listen_handler);
    }

    void on_connection_open(proton::connection &c) OVERRIDE {
        if (c.active()) {
            // outbound connection
            c.open_receiver("flow_example", proton::receiver_options().handler(receive_handler).credit_window(0));
        }
    }

    void on_connection_close(proton::connection &) OVERRIDE {
        listener.stop();
    }
};

int main(int argc, char **argv) {
    // Pick an "unusual" port since we are going to be talking to
    // ourselves, not a broker.
    bool quiet = false;

    example::options opts(argc, argv);
    opts.add_flag(quiet, 'q', "quiet", "suppress additional commentary of credit allocation and consumption");

    try {
        opts.parse();
        if (quiet)
            verbose = false;

        flow_control fc;
        proton::container(fc).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
