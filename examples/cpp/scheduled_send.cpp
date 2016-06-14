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

#include <proton/default_container.hpp>
#include <proton/messaging_handler.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

// Send messages at a constant rate one per interval. cancel after a timeout.
class scheduled_sender : public proton::messaging_handler {
  private:
    std::string url;
    proton::sender sender;
    proton::duration interval, timeout;
    bool ready, canceled;

  public:

    scheduled_sender(const std::string &s, double d, double t) :
        url(s),
        interval(int(d*proton::duration::SECOND.milliseconds())), // Send interval.
        timeout(int(t*proton::duration::SECOND.milliseconds())), // Cancel after timeout.
        ready(true),            // Ready to send.
        canceled(false)         // Canceled.
    {}

    void on_container_start(proton::container &c) OVERRIDE {
        sender = c.open_sender(url);
        // Call this->cancel after timeout.
        c.schedule(timeout, [this]() { this->cancel(); });
         // Start regular ticks every interval.
        c.schedule(interval, [this]() { this->tick(); });
    }

    void cancel() {
        canceled = true;
        sender.connection().close();
    }

    void tick() {
        // Schedule the next tick unless we have been cancelled.
        if (!canceled)
            sender.container().schedule(interval, [this]() { this->tick(); });
        if (sender.credit() > 0) // Only send if we have credit
            send();
        else
            ready = true;  // Set the ready flag, send as soon as we get credit.
    }

    void on_sendable(proton::sender &) OVERRIDE {
        if (ready)              // We have been ticked since the last send.
            send();
    }

    void send() {
        std::cout << "send" << std::endl;
        sender.send(proton::message("ping"));
        ready = false;
    }
};


int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");
    double interval = 1.0;
    double timeout = 5.0;

    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(interval, 'i', "interval", "send a message every INTERVAL seconds", "INTERVAL");
    opts.add_value(timeout, 't', "timeout", "stop after T seconds", "T");

    try {
        opts.parse();
        scheduled_sender h(address, interval, timeout);
        proton::default_container(h).run();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
