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
#include "fetcher.hpp"
#include "proton/event.hpp"

namespace proton {

fetcher::fetcher(blocking_connection &c, int prefetch) :
    messaging_handler(prefetch, false), // no auto_accept
    connection_(c), refcount_(0), pn_link_(0) {
}

void fetcher::incref() { refcount_++; }
void fetcher::decref() {
    refcount_--;
    if (!refcount_) {
        // fetcher needs to outlive its blocking_receiver unless disconnected from reactor
        if (pn_link_) {
            pn_record_set_handler(pn_link_attachments(pn_link_), 0);
            pn_decref(pn_link_);
        }
        delete this;
        return;
    }
}

void fetcher::on_link_init(event &e) {
    pn_link_ = e.link().pn_link();
    pn_incref(pn_link_);
}

void fetcher::on_message(event &e) {
    messages_.push_back(e.message());
    deliveries_.push_back(e.delivery());
    // Wake up enclosing blocking_connection.wait()
    e.container().yield();
}

void fetcher::on_link_error(event &e) {
    link lnk = e.link();
    if (pn_link_state(lnk.pn_link()) & PN_LOCAL_ACTIVE) {
        lnk.close();
        throw error(MSG("Link detached: " << lnk.name()));
    }
}

void fetcher::on_connection_error(event &e) {
    throw error(MSG("Connection closed"));
}

bool fetcher::has_message() {
    return !messages_.empty();
}

message fetcher::pop() {
    if (messages_.empty())
        throw error(MSG("blocking_receiver has no messages"));
    delivery &dlv(deliveries_.front());
    if (!dlv.settled())
        unsettled_.push_back(dlv);
    message m = messages_.front();
    messages_.pop_front();
    deliveries_.pop_front();
    return m;
}

void fetcher::settle(delivery::state state) {
    delivery &dlv = unsettled_.front();
    if (state)
        dlv.update(state);
    dlv.settle();
}

} // namespace
