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
#include "proton/connection.hpp"
#include "proton/transport.hpp"
#include "proton/handler.hpp"
#include "proton/session.hpp"
#include "proton/error.hpp"

#include "msg.hpp"
#include "contexts.hpp"
#include "container_impl.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/transport.h"
#include "proton/reactor.h"
#include "proton/object.h"

namespace proton {

connection_context& connection::context() const { return connection_context::get(pn_object()); }

void connection::open() { pn_connection_open(pn_object()); }

void connection::close() { pn_connection_close(pn_object()); }

void connection::release() { pn_connection_release(pn_object()); }

std::string connection::host() const {
    return std::string(pn_connection_get_hostname(pn_object()));
}

void connection::host(const std::string& h) {
    pn_connection_set_hostname(pn_object(), h.c_str());
}

std::string connection::container_id() const {
    const char* id = pn_connection_get_container(pn_object());
    return id ? std::string(id) : std::string();
}

void connection::container_id(const std::string& id) {
    pn_connection_set_container(pn_object(), id.c_str());
}

container& connection::container() const {
    return container_context(pn_object_reactor(pn_object()));
}

link_range connection::find_links(endpoint::state mask) const {
    return link_range(link_iterator(pn_link_head(pn_object(), mask)));
}

session_range connection::find_sessions(endpoint::state mask) const {
    return session_range(session_iterator(pn_session_head(pn_object(), mask)));
}

session connection::open_session() { return pn_session(pn_object()); }

session connection::default_session() {
    struct connection_context& ctx = connection_context::get(pn_object());
    if (!ctx.default_session) {
        ctx.default_session = open_session();
        ctx.default_session.open();
    }
    return ctx.default_session;
}

sender connection::open_sender(const std::string &addr, handler *h) {
    return default_session().open_sender(addr, h);
}

receiver connection::open_receiver(const std::string &addr, bool dynamic, handler *h)
{
    return default_session().open_receiver(addr, dynamic, h);
}

endpoint::state connection::state() const { return pn_connection_state(pn_object()); }

}
