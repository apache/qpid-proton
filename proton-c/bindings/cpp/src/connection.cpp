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
#include "proton/connection.hpp"

#include "proton/container.hpp"
#include "proton/transport.hpp"
#include "proton/session.hpp"
#include "proton/error.hpp"
#include "connector.hpp"

#include "msg.hpp"
#include "contexts.hpp"
#include "container_impl.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/transport.h"
#include "proton/reactor.h"
#include "proton/object.h"

namespace proton {

transport connection::transport() const {
    return pn_connection_transport(pn_object());
}

void connection::open() {
    connector *connector = dynamic_cast<class connector*>(
        connection_context::get(pn_object()).handler.get());
    if (connector)
        connector->apply_options();
    // Inbound connections should already be configured.
    pn_connection_open(pn_object());
}

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

container& connection::container() const {
    pn_reactor_t *r = pn_object_reactor(pn_object());
    if (!r)
        throw error("connection does not have a container");
    return container_context::get(r);
}

link_range connection::links() const {
    return link_range(link_iterator(pn_link_head(pn_object(), 0)));
}

session_range connection::sessions() const {
    return session_range(session_iterator(pn_session_head(pn_object(), 0)));
}

session connection::open_session() { return pn_session(pn_object()); }

session connection::default_session() {
    connection_context& ctx = connection_context::get(pn_object());
    if (!ctx.default_session) {
        // Note we can't use a proton::session here because we don't want to own
        // a session reference. The connection owns the session, owning it here as well
        // would create a circular ownership.
        ctx.default_session = pn_session(pn_object());
        pn_session_open(ctx.default_session);
    }
    return ctx.default_session;
}

sender connection::open_sender(const std::string &addr, const link_options &opts) {
    return default_session().open_sender(addr, opts);
}

receiver connection::open_receiver(const std::string &addr, const link_options &opts)
{
    return default_session().open_receiver(addr, opts);
}

endpoint::state connection::state() const { return pn_connection_state(pn_object()); }

condition connection::local_condition() const {
    return condition(pn_connection_condition(pn_object()));
}

condition connection::remote_condition() const {
    return condition(pn_connection_remote_condition(pn_object()));
}

void connection::user(const std::string &name) { pn_connection_set_user(pn_object(), name.c_str()); }

void connection::password(const std::string &pass) { pn_connection_set_password(pn_object(), pass.c_str()); }

}
