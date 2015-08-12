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
#include "proton/receiver.hpp"
#include "proton/error.hpp"
#include "fetcher.hpp"
#include "msg.hpp"


namespace proton {

namespace {

struct fetcher_has_message {
    fetcher_has_message(fetcher &f) : fetcher_(f) {}
    bool operator()() { return fetcher_.has_message(); }
    fetcher &fetcher_;
};

} // namespace


blocking_receiver::blocking_receiver(blocking_connection &c, receiver &l, fetcher *f, int credit)
    : blocking_link(&c, l.get()), fetcher_(f) {
    std::string sa = link_.source().address();
    std::string rsa = link_.remote_source().address();
    if (!sa.empty() && sa.compare(rsa) != 0) {
        wait_for_closed();
        link_.close();
        std::string txt = "Failed to open receiver " + link_.name() + ", source does not match";
        throw error(MSG(txt));
    }
    if (credit)
        pn_link_flow(link_.get(), credit);
    if (fetcher_)
        fetcher_->incref();
}

blocking_receiver::blocking_receiver(const blocking_receiver& r) : blocking_link(r), fetcher_(r.fetcher_) {
    if (fetcher_)
        fetcher_->incref();
}
blocking_receiver& blocking_receiver::operator=(const blocking_receiver& r) {
    if (this == &r) return *this;
    fetcher_ = r.fetcher_;
    if (fetcher_)
        fetcher_->incref();
    return *this;
}
blocking_receiver::~blocking_receiver() {
    if (fetcher_)
        fetcher_->decref();
}



message blocking_receiver::receive(duration timeout) {
    if (!fetcher_)
        throw error(MSG("Can't call receive on this receiver as a handler was provided"));
    receiver rcv(link_.get());
    if (!rcv.credit())
        rcv.flow(1);
    std::string txt = "Receiving on receiver " + link_.name();
    fetcher_has_message cond(*fetcher_);
    connection_.wait(cond, txt, timeout);
    return fetcher_->pop();
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
    if (!fetcher_)
        throw error(MSG("Can't call accept/reject etc on this receiver as a handler was provided"));
    fetcher_->settle(state);
}

void blocking_receiver::flow(int count) {
    receiver rcv(link_.get());
    rcv.flow(count);
}

int blocking_receiver::credit() {
    return link_.credit();
}

terminus blocking_receiver::source() {
    return link_.source();
}

terminus blocking_receiver::target() {
    return link_.target();
}

terminus blocking_receiver::remote_source() {
    return link_.remote_source();
}

terminus blocking_receiver::remote_target() {
    return link_.remote_target();
}


} // namespace
