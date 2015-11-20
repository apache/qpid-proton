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
#include "proton/blocking_link.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/connection.hpp"
#include "proton/error.hpp"
#include "proton/link.hpp"

#include "blocking_connection_impl.hpp"
#include "msg.hpp"

#include <proton/link.h>

namespace proton {

namespace {
struct link_opened : public blocking_connection_impl::condition {
    link_opened(link l) : pn_link(l) {}
    bool operator()() const { return !(pn_link.state() & PN_REMOTE_UNINIT); }
    link pn_link;
};

struct link_closed : public blocking_connection_impl::condition {
    link_closed(link l) : pn_link(l) {}
    bool operator()() const { return (pn_link.state() & PN_REMOTE_CLOSED); }
    link pn_link;
};

struct link_not_open : public blocking_connection_impl::condition {
    link_not_open(link l) : pn_link(l) {}
    bool operator()() const { return !(pn_link.state() & PN_REMOTE_ACTIVE); }
    link pn_link;
};

} // namespace

blocking_link::blocking_link(blocking_connection &c) : connection_(c) {}

void blocking_link::open(proton::link l) {
    link_ = l;
    connection_.impl_->wait(link_opened(link_), "opening link " + link_.name());
    check_closed();
}

blocking_link::~blocking_link() {}

void blocking_link::wait_for_closed() {
    link_closed link_closed(link_);
    connection_.impl_->wait(link_closed, "closing link " + link_.name());
    check_closed();
}

void blocking_link::check_closed() {
    if (link_.state() & PN_REMOTE_CLOSED) {
        link_.close();
        throw error(MSG("Link detached: " << link_.name()));
    }
}

void blocking_link::close() {
    link_.close();
    link_not_open link_not_open(link_);
    connection_.impl_->wait(link_not_open, "closing link " + link_.name());
}

}
