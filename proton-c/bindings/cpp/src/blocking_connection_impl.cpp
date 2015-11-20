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
#include "proton/connection.h"
#include "proton/error.hpp"

#include "blocking_connection_impl.hpp"
#include "msg.hpp"
#include "contexts.hpp"


namespace proton {

namespace {
struct connection_opening : public blocking_connection_impl::condition {
    connection_opening(const connection& c) : connection_(c) {}
    bool operator()() const { return (connection_.state()) & PN_REMOTE_UNINIT; }
    const connection& connection_;
};

struct connection_closed : public blocking_connection_impl::condition {
    connection_closed(const connection& c) : connection_(c) {}
    bool operator()() const { return !(connection_.state() & PN_REMOTE_ACTIVE); }
    const connection& connection_;
};
}

blocking_connection_impl::blocking_connection_impl(const url& url, duration timeout) :
    container_(new container())
{
    container_->reactor().start();
    container_->reactor().timeout(timeout);
    connection_ = container_->connect(url, this); // Set this as handler.
    wait(connection_opening(connection_));
}

blocking_connection_impl::~blocking_connection_impl() {}

void blocking_connection_impl::close() {
    connection_.close();
    wait(connection_closed(connection_));
}

namespace {
struct save_timeout {
    reactor& reactor_;
    duration timeout_;
    save_timeout(reactor& r) : reactor_(r), timeout_(r.timeout()) {}
    ~save_timeout() { reactor_.timeout(timeout_); }
};
}

void blocking_connection_impl::wait(const condition &condition, const std::string &msg, duration wait_timeout)
{
    reactor reactor = container_->reactor();

    if (wait_timeout == duration(-1))
        wait_timeout = reactor.timeout();

    if (wait_timeout == duration::FOREVER) {
        while (!condition()) {
            reactor.process();
        }
    } else {
        save_timeout st(reactor);
        reactor.timeout(wait_timeout);
        pn_timestamp_t deadline = reactor.mark() + wait_timeout.milliseconds;
        while (!condition()) {
            reactor.process();
            if (!condition()) {
                pn_timestamp_t now = reactor.mark();
                if (now < deadline)
                    reactor.timeout(duration(deadline - now));
                else
                    throw timeout_error("connection timed out " + msg);
            }
        }
    }
}



}
