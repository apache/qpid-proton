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
    connection_opening(pn_connection_t *c) : pn_connection(c) {}
    bool operator()() const { return (pn_connection_state(pn_connection) & PN_REMOTE_UNINIT); }
    pn_connection_t *pn_connection;
};

struct connection_closed : public blocking_connection_impl::condition {
    connection_closed(pn_connection_t *c) : pn_connection(c) {}
    bool operator()() const { return !(pn_connection_state(pn_connection) & PN_REMOTE_ACTIVE); }
    pn_connection_t *pn_connection;
};
}

blocking_connection_impl::blocking_connection_impl(const url& url, duration timeout) :
    container_(new container())
{
    container_->reactor().start();
    container_->reactor().timeout(timeout);
    connection_ = container_->connect(url, this).ptr(); // Set this as handler.
    wait(connection_opening(pn_cast(connection_.get())));
}

blocking_connection_impl::~blocking_connection_impl() {}

void blocking_connection_impl::close() {
    connection_->close();
    wait(connection_closed(pn_cast(connection_.get())));
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
    reactor& reactor = container_->reactor();

    if (wait_timeout == duration(-1))
        wait_timeout = container_->reactor().timeout();

    if (wait_timeout == duration::FOREVER) {
        while (!condition()) {
            reactor.process();
        }
    } else {
        save_timeout st(reactor);
        reactor.timeout(wait_timeout);
        pn_timestamp_t deadline = pn_reactor_mark(pn_cast(&reactor)) + wait_timeout.milliseconds;
        while (!condition()) {
            reactor.process();
            if (!condition()) {
                pn_timestamp_t now = pn_reactor_mark(pn_cast(&reactor));
                if (now < deadline)
                    reactor.timeout(duration(deadline - now));
                else
                    throw timeout_error("connection timed out " + msg);
            }
        }
    }
}



}
