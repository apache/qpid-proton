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

transport &connection::transport() {
    return *transport::cast(pn_connection_transport(pn_cast(this)));
}

void connection::open() { pn_connection_open(pn_cast(this)); }

void connection::close() { pn_connection_close(pn_cast(this)); }

std::string connection::hostname() {
    return std::string(pn_connection_get_hostname(pn_cast(this)));
}

container& connection::container() {
    return container_context(pn_object_reactor(pn_cast(this)));
}

link* connection::link_head(endpoint::state mask) {
    return link::cast(pn_link_head(pn_cast(this), mask));
}

session& connection::create_session() { return *session::cast(pn_session(pn_cast(this))); }

session& connection::default_session() {
    struct connection_context& ctx = connection_context::get(pn_cast(this));
    if (!ctx.default_session) {
        ctx.default_session = &create_session(); 
        ctx.default_session->open();
    }
    return *ctx.default_session;
}

}
