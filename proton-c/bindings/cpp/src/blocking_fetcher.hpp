#ifndef PROTON_CPP_FETCHER_H
#define PROTON_CPP_FETCHER_H

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
#include "proton/messaging_handler.hpp"
#include "proton/message.hpp"
#include <string>
#include <deque>

namespace proton {

class blocking_fetcher : public messaging_handler {
  public:
    blocking_fetcher(int prefetch);
    void on_message(event &e);
    void on_link_error(event &e);
    bool has_message();
    message pop();
    void settle(delivery::state state = delivery::NONE);

  private:
    std::deque<message> messages_;
    std::deque<delivery> deliveries_;
    std::deque<delivery> unsettled_;
};

}

#endif  /*!PROTON_CPP_FETCHER_H*/
