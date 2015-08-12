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
#include "connection_impl.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

link::link(pn_link_t* p) : wrapper<pn_link_t>(p) {}

void link::open() {
    pn_link_open(get());
}

void link::close() {
    pn_link_close(get());
}

bool link::is_sender() {
    return pn_link_is_sender(get());
}

bool link::is_receiver() { return !is_sender(); }

int link::credit() {
    return pn_link_credit(get());
}

terminus link::source() {
    return terminus(pn_link_source(get()), get());
}

terminus link::target() {
    return terminus(pn_link_target(get()), get());
}

terminus link::remote_source() {
    return terminus(pn_link_remote_source(get()), get());
}

terminus link::remote_target() {
    return terminus(pn_link_remote_target(get()), get());
}

std::string link::name() {
    return std::string(pn_link_name(get()));
}

class connection &link::connection() {
    pn_session_t *s = pn_link_session(get());
    pn_connection_t *c = pn_session_connection(s);
    return connection_impl::reactor_reference(c);
}

link link::next(endpoint::state mask) {

    return link(pn_link_next(get(), (pn_state_t) mask));
}

}
