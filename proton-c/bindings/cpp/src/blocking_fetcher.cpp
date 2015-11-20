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
#include "blocking_fetcher.hpp"
#include "proton/container.hpp"
#include "msg.hpp"

#include "proton/event.hpp"

namespace proton {

blocking_fetcher::blocking_fetcher(int prefetch) : messaging_handler(prefetch, false /*auto_accept*/) {}

void blocking_fetcher::on_message(event &e) {
    messages_.push_back(e.message());
    deliveries_.push_back(e.delivery());
    // Wake up enclosing connection.wait()
    e.container().reactor().yield();
}

void blocking_fetcher::on_link_error(event &e) {
    link lnk = e.link();
    if (lnk.state() & PN_LOCAL_ACTIVE) {
        lnk.close();
        throw error(MSG("Link detached: " << lnk.name()));
    }
}

bool blocking_fetcher::has_message() { return !messages_.empty(); }

message blocking_fetcher::pop() {
    if (messages_.empty())
        throw error(MSG("receiver has no messages"));
    delivery dlv(deliveries_.front());
    if (!dlv.settled())
        unsettled_.push_back(dlv);
    message m = messages_.front();
    messages_.pop_front();
    deliveries_.pop_front();
    return m;
}

void blocking_fetcher::settle(delivery::state state) {
    delivery dlv = unsettled_.front();
    if (state)
        dlv.update(state);
    dlv.settle();
}

}
