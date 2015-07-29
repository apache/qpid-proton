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
#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/duration.hpp"
#include "proton/error.hpp"
#include "proton/wait_condition.hpp"
#include "blocking_connection_impl.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"

namespace proton {

wait_condition::~wait_condition() {}


void blocking_connection_impl::incref(blocking_connection_impl *impl_) {
    impl_->refcount_++;
}

void blocking_connection_impl::decref(blocking_connection_impl *impl_) {
    impl_->refcount_--;
    if (impl_->refcount_ == 0)
        delete impl_;
}

namespace {
struct connection_opening : public wait_condition {
    connection_opening(pn_connection_t *c) : pn_connection(c) {}
    bool achieved() { return (pn_connection_state(pn_connection) & PN_REMOTE_UNINIT); }
    pn_connection_t *pn_connection;
};

struct connection_closed : public wait_condition {
    connection_closed(pn_connection_t *c) : pn_connection(c) {}
    bool achieved() { return !(pn_connection_state(pn_connection) & PN_REMOTE_ACTIVE); }
    pn_connection_t *pn_connection;
};

}


blocking_connection_impl::blocking_connection_impl(const url &u, duration timeout0, ssl_domain *ssld, container *c)
    : url_(u), timeout_(timeout0), refcount_(0)
{
    if (c)
        container_ = *c;
    container_.start();
    container_.timeout(timeout_);
    // Create connection and send the connection events here
    connection_ = container_.connect(url_, static_cast<handler *>(this));
    connection_opening cond(connection_.pn_connection());
    wait(cond);
}

blocking_connection_impl::~blocking_connection_impl() {
    container_ = container();
}

void blocking_connection_impl::close() {
    connection_.close();
    connection_closed cond(connection_.pn_connection());
    wait(cond);
}

void blocking_connection_impl::wait(wait_condition &condition) {
    std::string empty;
    wait(condition, empty, timeout_);
}

void blocking_connection_impl::wait(wait_condition &condition, std::string &msg) {
    wait(condition, msg, timeout_);
}

void blocking_connection_impl::wait(wait_condition &condition, std::string &msg, duration wait_timeout) {
    if (wait_timeout == duration::FOREVER) {
        while (!condition.achieved()) {
            container_.process();
        }
    }

    pn_reactor_t *reactor = container_.reactor();
    pn_millis_t orig_timeout = pn_reactor_get_timeout(reactor);
    pn_reactor_set_timeout(reactor, wait_timeout.milliseconds);
    try {
        pn_timestamp_t now = pn_reactor_mark(reactor);
        pn_timestamp_t deadline = now + wait_timeout.milliseconds;
        while (!condition.achieved()) {
            container_.process();
            if (deadline < pn_reactor_mark(reactor)) {
                std::string txt = "connection timed out";
                if (!msg.empty())
                    txt += ": " + msg;
                throw timeout_error(txt);
            }
        }
    } catch (...) {
        pn_reactor_set_timeout(reactor, orig_timeout);
        throw;
    }
    pn_reactor_set_timeout(reactor, orig_timeout);
}



}
