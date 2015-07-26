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
#include "proton/blocking_receiver.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/wait_condition.hpp"
#include "proton/error.hpp"
#include "fetcher.hpp"
#include "msg.hpp"


namespace proton {

namespace {

struct fetcher_has_message : public wait_condition {
    fetcher_has_message(fetcher &f) : fetcher_(f) {}
    bool achieved() { return fetcher_.has_message(); }
    fetcher &fetcher_;
};

} // namespace


blocking_receiver::blocking_receiver(blocking_connection &c, receiver &l, fetcher &f, int credit)
    : blocking_link(&c, l.pn_link()), fetcher_(f) {
    std::string sa = link_.source().address();
    std::string rsa = link_.remote_source().address();
    if (sa.empty() || sa.compare(rsa) != 0) {
        wait_for_closed();
        link_.close();
        std::string txt = "Failed to open receiver " + link_.name() + ", source does not match";
        throw error(MSG(txt));
    }
    if (credit)
        pn_link_flow(link_.pn_link(), credit);
    fetcher_.incref();
}

blocking_receiver::blocking_receiver(const blocking_receiver& r) : blocking_link(r), fetcher_(r.fetcher_) {
    fetcher_.incref();
}
blocking_receiver& blocking_receiver::operator=(const blocking_receiver& r) {
    if (this == &r) return *this;
    fetcher_ = r.fetcher_;
    fetcher_.incref();
    return *this;
}
blocking_receiver::~blocking_receiver() { fetcher_.decref(); }



message blocking_receiver::receive(duration timeout) {
    receiver rcv = link_;
    if (!rcv.credit())
        rcv.flow(1);
    std::string txt = "Receiving on receiver " + link_.name();
    fetcher_has_message cond(fetcher_);
    connection_.wait(cond, txt, timeout);
    return fetcher_.pop();
}

message blocking_receiver::receive() {
    // Use default timeout
    return receive(connection_.timeout());
}

void blocking_receiver::accept() {
    settle(delivery::ACCEPTED);
}

void blocking_receiver::reject() {
    settle(delivery::REJECTED);
}

void blocking_receiver::release(bool delivered) {
    if (delivered)
        settle(delivery::MODIFIED);
    else
        settle(delivery::RELEASED);
}

void blocking_receiver::settle(delivery::state state = delivery::NONE) {
    fetcher_.settle(state);
}

} // namespace
