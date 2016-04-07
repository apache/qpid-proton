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

#include "proton_event.hpp"

#include "proton/container.hpp"
#include "proton/delivery.hpp"
#include "proton/error.hpp"
#include "proton/receiver.hpp"
#include "proton/sender.hpp"
#include "proton/transport.hpp"

#include "msg.hpp"
#include "contexts.hpp"
#include "proton_handler.hpp"

#include "proton/reactor.h"
#include "proton/link.h"

namespace proton {

proton_event::proton_event(pn_event_t *ce, pn_event_type_t t, class container *c) :
    pn_event_(ce),
    type_(event_type(t)),
    container_(c)
{}

proton_event::event_type proton_event::type() const { return type_; }

std::string proton_event::name() const { return pn_event_type_name(pn_event_type_t(type_)); }

pn_event_t *proton_event::pn_event() const { return pn_event_; }

container* proton_event::container() const {
    return container_;
}

transport proton_event::transport() const {
    pn_transport_t *t = pn_event_transport(pn_event());
    if (!t)
        throw error(MSG("No transport context for this event"));
    return t;
}

connection proton_event::connection() const {
    pn_connection_t *conn = pn_event_connection(pn_event());
    if (!conn)
        throw error(MSG("No connection context for this event"));
    return conn;
}

session proton_event::session() const {
    pn_session_t *sess = pn_event_session(pn_event());
    if (!sess)
        throw error(MSG("No session context for this event"));
    return sess;
}

link proton_event::link() const {
    class link lnk = pn_event_link(pn_event());
    if (!lnk) throw error(MSG("No link context for this event"));
    return lnk;
}

sender proton_event::sender() const {
    if (!link().sender()) throw error(MSG("No sender context for this event"));
    return link().sender();
}

receiver proton_event::receiver() const {
    if (!link().receiver()) throw error(MSG("No receiver context for this event"));
    return link().receiver();
}

delivery proton_event::delivery() const {
    pn_delivery_t* dlv = pn_event_delivery(pn_event());
    if (!dlv) throw error(MSG("No delivery context for this event"));
    return dlv;
}

void proton_event::dispatch(proton_handler &handler) {
    switch(type_) {

      case PN_REACTOR_INIT: handler.on_reactor_init(*this); break;
      case PN_REACTOR_QUIESCED: handler.on_reactor_quiesced(*this); break;
      case PN_REACTOR_FINAL: handler.on_reactor_final(*this); break;

      case PN_TIMER_TASK: handler.on_timer_task(*this); break;

      case PN_CONNECTION_INIT: handler.on_connection_init(*this); break;
      case PN_CONNECTION_BOUND: handler.on_connection_bound(*this); break;
      case PN_CONNECTION_UNBOUND: handler.on_connection_unbound(*this); break;
      case PN_CONNECTION_LOCAL_OPEN: handler.on_connection_local_open(*this); break;
      case PN_CONNECTION_LOCAL_CLOSE: handler.on_connection_local_close(*this); break;
      case PN_CONNECTION_REMOTE_OPEN: handler.on_connection_remote_open(*this); break;
      case PN_CONNECTION_REMOTE_CLOSE: handler.on_connection_remote_close(*this); break;
      case PN_CONNECTION_FINAL: handler.on_connection_final(*this); break;

      case PN_SESSION_INIT: handler.on_session_init(*this); break;
      case PN_SESSION_LOCAL_OPEN: handler.on_session_local_open(*this); break;
      case PN_SESSION_LOCAL_CLOSE: handler.on_session_local_close(*this); break;
      case PN_SESSION_REMOTE_OPEN: handler.on_session_remote_open(*this); break;
      case PN_SESSION_REMOTE_CLOSE: handler.on_session_remote_close(*this); break;
      case PN_SESSION_FINAL: handler.on_session_final(*this); break;

      case PN_LINK_INIT: handler.on_link_init(*this); break;
      case PN_LINK_LOCAL_OPEN: handler.on_link_local_open(*this); break;
      case PN_LINK_LOCAL_CLOSE: handler.on_link_local_close(*this); break;
      case PN_LINK_LOCAL_DETACH: handler.on_link_local_detach(*this); break;
      case PN_LINK_REMOTE_OPEN: handler.on_link_remote_open(*this); break;
      case PN_LINK_REMOTE_CLOSE: handler.on_link_remote_close(*this); break;
      case PN_LINK_REMOTE_DETACH: handler.on_link_remote_detach(*this); break;
      case PN_LINK_FLOW: handler.on_link_flow(*this); break;
      case PN_LINK_FINAL: handler.on_link_final(*this); break;

      case PN_DELIVERY: handler.on_delivery(*this); break;

      case PN_TRANSPORT: handler.on_transport(*this); break;
      case PN_TRANSPORT_ERROR: handler.on_transport_error(*this); break;
      case PN_TRANSPORT_HEAD_CLOSED: handler.on_transport_head_closed(*this); break;
      case PN_TRANSPORT_TAIL_CLOSED: handler.on_transport_tail_closed(*this); break;
      case PN_TRANSPORT_CLOSED: handler.on_transport_closed(*this); break;

      case PN_SELECTABLE_INIT: handler.on_selectable_init(*this); break;
      case PN_SELECTABLE_UPDATED: handler.on_selectable_updated(*this); break;
      case PN_SELECTABLE_READABLE: handler.on_selectable_readable(*this); break;
      case PN_SELECTABLE_WRITABLE: handler.on_selectable_writable(*this); break;
      case PN_SELECTABLE_EXPIRED: handler.on_selectable_expired(*this); break;
      case PN_SELECTABLE_ERROR: handler.on_selectable_error(*this); break;
      case PN_SELECTABLE_FINAL: handler.on_selectable_final(*this); break;
      default:
        throw error(MSG("Invalid Proton event type " << type_));
    }
}

}
