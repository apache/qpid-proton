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
#include "proton/messaging_handler.hpp"
#include "proton/error.hpp"
#include "msg.hpp"


namespace proton {

namespace {
struct link_opened {
    link_opened(pn_link_t *l) : pn_link(l) {}
    bool operator()() { return !(pn_link_state(pn_link) & PN_REMOTE_UNINIT); }
    pn_link_t *pn_link;
};

struct link_closed {
    link_closed(pn_link_t *l) : pn_link(l) {}
    bool operator()() { return (pn_link_state(pn_link) & PN_REMOTE_CLOSED); }
    pn_link_t *pn_link;
};

struct link_not_open {
    link_not_open(pn_link_t *l) : pn_link(l) {}
    bool operator()() { return !(pn_link_state(pn_link) & PN_REMOTE_ACTIVE); }
    pn_link_t *pn_link;
};


} // namespace


blocking_link::blocking_link(blocking_connection *c, pn_link_t *pnl) : connection_(*c), link_(pnl) {
    std::string msg = "Opening link " + link_.name();
    link_opened link_opened(link_.get());
    connection_.wait(link_opened, msg);
    check_closed();
}

blocking_link::~blocking_link() {}

void blocking_link::wait_for_closed(duration timeout) {
    std::string msg = "Closing link " + link_.name();
    link_closed link_closed(link_.get());
    connection_.wait(link_closed, msg);
    check_closed();
}

void blocking_link::check_closed() {
    pn_link_t * pn_link = link_.get();
    if (pn_link_state(pn_link) & PN_REMOTE_CLOSED) {
        link_.close();
        throw error(MSG("Link detached: " << link_.name()));
    }
}

void blocking_link::close() {
    link_.close();
    std::string msg = "Closing link " + link_.name();
    link_not_open link_not_open(link_.get());
    connection_.wait(link_not_open, msg);
}

}
