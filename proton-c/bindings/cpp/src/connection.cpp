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

#include "proton_bits.hpp"

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/error.hpp"
#include "proton/event_loop.hpp"
#include "proton/session.hpp"
#include "proton/transport.hpp"

#include "connector.hpp"
#include "contexts.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <proton/reactor.h>
#include <proton/object.h>

namespace proton {

transport connection::transport() const {
    return make_wrapper(pn_connection_transport(pn_object()));
}

void connection::open() {
    open(connection_options());
}

void connection::open(const connection_options &opts) {
    connector *connector = dynamic_cast<class connector*>(
        connection_context::get(pn_object()).handler.get());
    if (connector)
        // connector has an internal copy of opts
        connector->apply_options();
    else
        opts.apply(*this);
    pn_connection_open(pn_object());
}

void connection::close() { pn_connection_close(pn_object()); }

std::string connection::virtual_host() const {
    return str(pn_connection_remote_hostname(pn_object()));
}

std::string connection::container_id() const {
    return str(pn_connection_get_container(pn_object()));
}

container& connection::container() const {
    class container* c = connection_context::get(pn_object()).container;
    if (!c) {
        pn_reactor_t *r = pn_object_reactor(pn_object());
        if (r)
            c = &container_context::get(r);
    }
    if (!c)
        throw proton::error("connection does not have a container");
    return *c;
}

session_range connection::sessions() const {
    return session_range(session_iterator(make_wrapper(pn_session_head(pn_object(), 0))));
}

receiver_range connection::receivers() const {
  pn_link_t *lnk = pn_link_head(pn_object(), 0);
  while (lnk) {
    if (pn_link_is_receiver(lnk))
      break;
    lnk = pn_link_next(lnk, 0);
  }
  return receiver_range(receiver_iterator(make_wrapper<receiver>(lnk)));
}

sender_range connection::senders() const {
  pn_link_t *lnk = pn_link_head(pn_object(), 0);
  while (lnk) {
    if (pn_link_is_sender(lnk))
      break;
    lnk = pn_link_next(lnk, 0);
  }
  return sender_range(sender_iterator(make_wrapper<sender>(lnk)));
}

session connection::open_session() {
    return open_session(session_options());
}

session connection::open_session(const session_options &opts) {
    session s(make_wrapper<session>(pn_session(pn_object())));
    // TODO: error check, too many sessions, no mem...
    if (!!s) s.open(opts);
    return s;
}

session connection::default_session() {
    connection_context& ctx = connection_context::get(pn_object());
    if (!ctx.default_session) {
        // Note we can't use a proton::session here because we don't want to own
        // a session reference. The connection owns the session, owning it here as well
        // would create a circular ownership.
        ctx.default_session = pn_session(pn_object());
        pn_session_open(ctx.default_session);
    }
    return make_wrapper(ctx.default_session);
}

sender connection::open_sender(const std::string &addr) {
    return open_sender(addr, sender_options());
}

sender connection::open_sender(const std::string &addr, const sender_options &opts) {
    return default_session().open_sender(addr, opts);
}

receiver connection::open_receiver(const std::string &addr) {
    return open_receiver(addr, receiver_options());
}

receiver connection::open_receiver(const std::string &addr, const receiver_options &opts)
{
    return default_session().open_receiver(addr, opts);
}

error_condition connection::error() const {
    return make_wrapper(pn_connection_remote_condition(pn_object()));
}

uint32_t connection::max_frame_size() const {
    return pn_transport_get_remote_max_frame(pn_connection_transport(pn_object()));
}

uint16_t connection::max_sessions() const {
    return pn_transport_remote_channel_max(pn_connection_transport(pn_object()));
}

uint32_t connection::idle_timeout() const {
    return pn_transport_get_remote_idle_timeout(pn_connection_transport(pn_object()));
}

void connection::user(const std::string &name) { pn_connection_set_user(pn_object(), name.c_str()); }

void connection::password(const std::string &pass) { pn_connection_set_password(pn_object(), pass.c_str()); }

}
