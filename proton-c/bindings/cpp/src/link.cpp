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
#include "proton/link.hpp"
#include "proton/error.hpp"
#include "proton/connection.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

void link::open() {
    pn_link_open(pn_cast(this));
}

void link::close() {
    pn_link_close(pn_cast(this));
}

bool link::is_sender() { return pn_link_is_sender(pn_cast(this)); }
bool link::is_receiver() { return pn_link_is_receiver(pn_cast(this)); }

sender& link::sender() {
    if (!is_sender()) throw error("link is not a sender");
    return *reinterpret_cast<class sender*>(this);
}

receiver& link::receiver() {
    if (!is_receiver()) throw error("link is not a receiver");
    return *reinterpret_cast<class receiver*>(this);
}

int link::credit() {
    return pn_link_credit(pn_cast(this));
}

bool link::has_source() { return pn_link_source(pn_cast(this)); }
bool link::has_target() { return pn_link_target(pn_cast(this)); }
bool link::has_remote_source() { return pn_link_remote_source(pn_cast(this)); }
bool link::has_remote_target() { return pn_link_remote_target(pn_cast(this)); }

terminus& link::source() { return *terminus::cast(pn_link_source(pn_cast(this))); }
terminus& link::target() { return *terminus::cast(pn_link_target(pn_cast(this))); }
terminus& link::remote_source() { return *terminus::cast(pn_link_remote_source(pn_cast(this))); }
terminus& link::remote_target() { return *terminus::cast(pn_link_remote_target(pn_cast(this))); }

std::string link::name() { return std::string(pn_link_name(pn_cast(this)));}

class connection &link::connection() {
    return *connection::cast(pn_session_connection(pn_link_session(pn_cast(this))));
}

link* link::next(endpoint::state mask) {
    return link::cast(pn_link_next(pn_cast(this), (pn_state_t) mask));
}

}
