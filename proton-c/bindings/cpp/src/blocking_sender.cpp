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
#include "proton/blocking_sender.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/error.hpp"
#include "blocking_connection_impl.hpp"
#include "msg.hpp"

namespace proton {

namespace {
struct delivery_settled : public blocking_connection_impl::condition {
    delivery_settled(pn_delivery_t *d) : pn_delivery(d) {}
    bool operator()() const { return pn_delivery_settled(pn_delivery); }
    pn_delivery_t *pn_delivery;
};
} // namespace

blocking_sender::blocking_sender(blocking_connection &c, const std::string &address) :
    blocking_link(c)
{
    open(c.impl_->connection_->create_sender(address));
    std::string ta = link_->target().address();
    std::string rta = link_->remote_target().address();
    if (ta.empty() || ta.compare(rta) != 0) {
        wait_for_closed();
        link_->close();
        std::string txt = "Failed to open sender " + link_->name() + ", target does not match";
        throw error(MSG(txt));
    }
}

blocking_sender::~blocking_sender() {}

delivery& blocking_sender::send(const message &msg, duration timeout) {
    delivery& dlv = sender().send(msg);
    connection_.impl_->wait(delivery_settled(pn_cast(&dlv)), "sending on sender " + link_->name(), timeout);
    return dlv;
}

delivery& blocking_sender::send(const message &msg) {
    // Use default timeout
    return send(msg, connection_.timeout());
}

sender& blocking_sender::sender() { return link_->sender(); }

}
