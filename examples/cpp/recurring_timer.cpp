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
#include "proton/handler.hpp"
#include "proton/task.hpp"

#include <iostream>
#include <map>

#include "fake_cpp11.hpp"

class ticker : public proton::handler {
    void on_timer(proton::container &) override {
        std::cout << "Tick..." << std::endl;
    }
};

class tocker : public proton::handler {
    void on_timer(proton::container &) override {
        std::cout << "Tock..." << std::endl;
    }
};


class recurring : public proton::handler {
  private:
    int remaining_msecs, tick_ms;
    ticker tick_handler;
    tocker tock_handler;
    proton::task cancel_task;
  public:

    recurring(int msecs, int tickms) : remaining_msecs(msecs), tick_ms(tickms), cancel_task(0) {}

    proton::task ticktock(proton::container &c) {
        // Show timer events in separate handlers.
        c.schedule(tick_ms, &tick_handler);
        return c.schedule(tick_ms * 3, &tock_handler);
    }

    void on_container_start(proton::container &c) override {
        // Demonstrate cancel(), we will cancel the first tock on the first recurring::on_timer_task
        cancel_task = ticktock(c);
        c.schedule(0);
    }

    void on_timer(proton::container &c) override {
        if (!!cancel_task) {
            cancel_task.cancel();
            cancel_task = 0;
            c.schedule(tick_ms * 4);
        } else {
            remaining_msecs -= tick_ms * 4;
            if (remaining_msecs > 0) {
                ticktock(c);
                c.schedule(tick_ms * 4);
            }
        }
    }
};

int main(int argc, char **argv) {
    // Command line options
    double running_time = 5;
    double tick = 0.25;
    options opts(argc, argv);
    opts.add_value(running_time, 't', "running time", "running time in seconds", "RUNTIME");
    opts.add_value(tick, 'k', "tick time", "tick time as fraction of second", "TICK");
    try {
        opts.parse();
        recurring recurring_handler(int(running_time * 1000), int(tick * 1000));
        proton::container(recurring_handler).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
