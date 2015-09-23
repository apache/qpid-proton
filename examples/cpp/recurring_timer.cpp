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
#include "proton/task.hpp"

#include <iostream>
#include <map>

class ticker : public proton::messaging_handler {
    void on_timer_task(proton::event &e) {
        std::cout << "Tick..." << std::endl;
    }
};

class tocker : public proton::messaging_handler {
    void on_timer_task(proton::event &e) {
        std::cout << "Tock..." << std::endl;
    }
};


class recurring : public proton::messaging_handler {
  private:
    int remaining_secs;
    ticker tick_handler;
    tocker tock_handler;
    proton::task *cancel_task;
  public:

    recurring(int secs) : remaining_secs(secs), cancel_task(0) {}

    proton::task& ticktock(proton::event &e) {
        // Show timer events in separate handlers.
        e.container().schedule(250, &tick_handler);
        return e.container().schedule(750, &tock_handler);
    }

    void on_start(proton::event &e) {
        if (remaining_secs <= 0)
            return;
        proton::task& first_tock = ticktock(e);
        e.container().schedule(1000);
        remaining_secs--;
        // Show a cancel operation.
        cancel_task = &first_tock;
        e.container().schedule(500);
    }

    void on_timer_task(proton::event &e) {
        if (cancel_task) {
            cancel_task->cancel();
            cancel_task = 0;
            return;
        }
        if (remaining_secs) {
            ticktock(e);
            e.container().schedule(1000);
            remaining_secs--;
        }
    }
};

int main(int argc, char **argv) {
    // Command line options
    int running_time_in_secs = 5;
    options opts(argc, argv);
    opts.add_value(running_time_in_secs, 't', "running time", "running time in seconds", "RUNTIME");
    try {
        opts.parse();
        recurring recurring_handler(running_time_in_secs);
        proton::container(recurring_handler).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
