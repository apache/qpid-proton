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
#include "broker.hpp"

#include "proton/acceptor.hpp"
#include "proton/container.hpp"
#include "proton/value.hpp"

#include <iostream>
#include <deque>
#include <map>
#include <list>
#include <string>

#include "fake_cpp11.hpp"

class broker {
  public:
    broker(const proton::url& url) : handler_(url, queues_) {}

    proton::handler& handler() { return handler_; }

  private:
    class my_handler : public broker_handler {
      public:
        my_handler(const proton::url& u, queues& qs) : broker_handler(qs), url_(u) {}

        void on_container_start(proton::container &c) override {
            c.listen(url_);
            std::cout << "broker listening on " << url_ << std::endl;
        }

      private:
        const proton::url& url_;
    };

  private:
    queues queues_;
    my_handler handler_;
};

int main(int argc, char **argv) {
    proton::url url("0.0.0.0");
    options opts(argc, argv);

    opts.add_value(url, 'a', "address", "listen on URL", "URL");

    try {
        opts.parse();

        broker b(url);
        proton::container(b.handler()).run();

        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
