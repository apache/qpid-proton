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
#include "proton/blocking_connection.hpp"
#include "proton/blocking_receiver.hpp"
#include "proton/receiver.hpp"
#include "proton/connection.hpp"
#include "proton/receiver.hpp"
#include "proton/error.hpp"

#include "blocking_connection_impl.hpp"
#include "blocking_fetcher.hpp"
#include "msg.hpp"


namespace proton {

namespace {

struct fetcher_has_message : public blocking_connection_impl::condition {
    fetcher_has_message(blocking_fetcher &f) : fetcher_(f) {}
    bool operator()() const { return fetcher_.has_message(); }
    blocking_fetcher &fetcher_;
};

} // namespace

blocking_receiver::blocking_receiver(
    class blocking_connection &c, const std::string& addr, int credit, bool dynamic) :
    blocking_link(c), fetcher_(new blocking_fetcher(credit))
{
    open(c.impl_->connection_.open_receiver(addr, dynamic, fetcher_.get()));
    std::string sa = link_.source().address();
    std::string rsa = link_.remote_source().address();
    if (!sa.empty() && sa.compare(rsa) != 0) {
        wait_for_closed();
        link_.close();
        std::string txt = "Failed to open receiver " + link_.name() + ", source does not match";
        throw error(MSG(txt));
    }
    if (credit)
        link_.flow(credit);
}

blocking_receiver::~blocking_receiver() { link_.detach_handler(); }

message blocking_receiver::receive(duration timeout) {
    if (!receiver().credit())
        receiver().flow(1);
    fetcher_has_message cond(*fetcher_);
    connection_.impl_->wait(cond, "receiving on receiver " + link_.name(), timeout);
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
    fetcher_->settle(state);
}

void blocking_receiver::flow(int count) {
    receiver().flow(count);
}

receiver blocking_receiver::receiver() { return link_.receiver(); }

}
