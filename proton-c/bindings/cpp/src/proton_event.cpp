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

#include "proton/reactor.h"
#include "proton/event.h"
#include "proton/link.h"

#include "proton/container.hpp"
#include "proton/engine.hpp"
#include "proton/delivery.hpp"
#include "proton/error.hpp"
#include "proton_event.hpp"
#include "proton/proton_handler.hpp"
#include "proton/receiver.hpp"
#include "proton/sender.hpp"

#include "msg.hpp"
#include "contexts.hpp"

namespace proton {

proton_event::proton_event(pn_event_t *ce, proton_event::event_type t, class event_loop *el) :
    pn_event_(ce),
    type_(t),
    event_loop_(el)
{}

int proton_event::type() const { return type_; }

std::string proton_event::name() const { return pn_event_type_name(pn_event_type_t(type_)); }

pn_event_t *proton_event::pn_event() const { return pn_event_; }

event_loop& proton_event::event_loop() const {
    if (!event_loop_)
        throw error(MSG("No event_loop context for this event"));
    return *event_loop_;
}

container& proton_event::container() const {
    class container *c = dynamic_cast<class container*>(event_loop_);
    if (!c)
        throw error(MSG("No container context for this event"));
    return *c;
}

engine& proton_event::engine() const {
    class engine *e = dynamic_cast<class engine*>(event_loop_);
    if (!e)
        throw error(MSG("No engine context for this event"));
    return *e;
}

connection proton_event::connection() const {
    pn_connection_t *conn = pn_event_connection(pn_event());
    if (!conn)
        throw error(MSG("No connection context for this event"));
    return conn;
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

void proton_event::dispatch(handler &h) {
    proton_handler *handler = dynamic_cast<proton_handler*>(&h);

    if (handler) {
        switch(type_) {

          case PN_REACTOR_INIT: handler->on_reactor_init(*this); break;
          case PN_REACTOR_QUIESCED: handler->on_reactor_quiesced(*this); break;
          case PN_REACTOR_FINAL: handler->on_reactor_final(*this); break;

          case PN_TIMER_TASK: handler->on_timer_task(*this); break;

          case PN_CONNECTION_INIT: handler->on_connection_init(*this); break;
          case PN_CONNECTION_BOUND: handler->on_connection_bound(*this); break;
          case PN_CONNECTION_UNBOUND: handler->on_connection_unbound(*this); break;
          case PN_CONNECTION_LOCAL_OPEN: handler->on_connection_local_open(*this); break;
          case PN_CONNECTION_LOCAL_CLOSE: handler->on_connection_local_close(*this); break;
          case PN_CONNECTION_REMOTE_OPEN: handler->on_connection_remote_open(*this); break;
          case PN_CONNECTION_REMOTE_CLOSE: handler->on_connection_remote_close(*this); break;
          case PN_CONNECTION_FINAL: handler->on_connection_final(*this); break;

          case PN_SESSION_INIT: handler->on_session_init(*this); break;
          case PN_SESSION_LOCAL_OPEN: handler->on_session_local_open(*this); break;
          case PN_SESSION_LOCAL_CLOSE: handler->on_session_local_close(*this); break;
          case PN_SESSION_REMOTE_OPEN: handler->on_session_remote_open(*this); break;
          case PN_SESSION_REMOTE_CLOSE: handler->on_session_remote_close(*this); break;
          case PN_SESSION_FINAL: handler->on_session_final(*this); break;

          case PN_LINK_INIT: handler->on_link_init(*this); break;
          case PN_LINK_LOCAL_OPEN: handler->on_link_local_open(*this); break;
          case PN_LINK_LOCAL_CLOSE: handler->on_link_local_close(*this); break;
          case PN_LINK_LOCAL_DETACH: handler->on_link_local_detach(*this); break;
          case PN_LINK_REMOTE_OPEN: handler->on_link_remote_open(*this); break;
          case PN_LINK_REMOTE_CLOSE: handler->on_link_remote_close(*this); break;
          case PN_LINK_REMOTE_DETACH: handler->on_link_remote_detach(*this); break;
          case PN_LINK_FLOW: handler->on_link_flow(*this); break;
          case PN_LINK_FINAL: handler->on_link_final(*this); break;

          case PN_DELIVERY: handler->on_delivery(*this); break;

          case PN_TRANSPORT: handler->on_transport(*this); break;
          case PN_TRANSPORT_ERROR: handler->on_transport_error(*this); break;
          case PN_TRANSPORT_HEAD_CLOSED: handler->on_transport_head_closed(*this); break;
          case PN_TRANSPORT_TAIL_CLOSED: handler->on_transport_tail_closed(*this); break;
          case PN_TRANSPORT_CLOSED: handler->on_transport_closed(*this); break;

          case PN_SELECTABLE_INIT: handler->on_selectable_init(*this); break;
          case PN_SELECTABLE_UPDATED: handler->on_selectable_updated(*this); break;
          case PN_SELECTABLE_READABLE: handler->on_selectable_readable(*this); break;
          case PN_SELECTABLE_WRITABLE: handler->on_selectable_writable(*this); break;
          case PN_SELECTABLE_EXPIRED: handler->on_selectable_expired(*this); break;
          case PN_SELECTABLE_ERROR: handler->on_selectable_error(*this); break;
          case PN_SELECTABLE_FINAL: handler->on_selectable_final(*this); break;
          default:
            throw error(MSG("Invalid Proton event type " << type_));
            break;
        }
    } else {
        h.on_unhandled(*this);
    }

    // recurse through children
    for (handler::iterator child = h.begin(); child != h.end(); ++child) {
        dispatch(**child);
    }
}

const proton_event::event_type proton_event::EVENT_NONE=PN_EVENT_NONE;
const proton_event::event_type proton_event::REACTOR_INIT=PN_REACTOR_INIT;
const proton_event::event_type proton_event::REACTOR_QUIESCED=PN_REACTOR_QUIESCED;
const proton_event::event_type proton_event::REACTOR_FINAL=PN_REACTOR_FINAL;
const proton_event::event_type proton_event::TIMER_TASK=PN_TIMER_TASK;
const proton_event::event_type proton_event::CONNECTION_INIT=PN_CONNECTION_INIT;
const proton_event::event_type proton_event::CONNECTION_BOUND=PN_CONNECTION_BOUND;
const proton_event::event_type proton_event::CONNECTION_UNBOUND=PN_CONNECTION_UNBOUND;
const proton_event::event_type proton_event::CONNECTION_LOCAL_OPEN=PN_CONNECTION_LOCAL_OPEN;
const proton_event::event_type proton_event::CONNECTION_REMOTE_OPEN=PN_CONNECTION_REMOTE_OPEN;
const proton_event::event_type proton_event::CONNECTION_LOCAL_CLOSE=PN_CONNECTION_LOCAL_CLOSE;
const proton_event::event_type proton_event::CONNECTION_REMOTE_CLOSE=PN_CONNECTION_REMOTE_CLOSE;
const proton_event::event_type proton_event::CONNECTION_FINAL=PN_CONNECTION_FINAL;
const proton_event::event_type proton_event::SESSION_INIT=PN_SESSION_INIT;
const proton_event::event_type proton_event::SESSION_LOCAL_OPEN=PN_SESSION_LOCAL_OPEN;
const proton_event::event_type proton_event::SESSION_REMOTE_OPEN=PN_SESSION_REMOTE_OPEN;
const proton_event::event_type proton_event::SESSION_LOCAL_CLOSE=PN_SESSION_LOCAL_CLOSE;
const proton_event::event_type proton_event::SESSION_REMOTE_CLOSE=PN_SESSION_REMOTE_CLOSE;
const proton_event::event_type proton_event::SESSION_FINAL=PN_SESSION_FINAL;
const proton_event::event_type proton_event::LINK_INIT=PN_LINK_INIT;
const proton_event::event_type proton_event::LINK_LOCAL_OPEN=PN_LINK_LOCAL_OPEN;
const proton_event::event_type proton_event::LINK_REMOTE_OPEN=PN_LINK_REMOTE_OPEN;
const proton_event::event_type proton_event::LINK_LOCAL_CLOSE=PN_LINK_LOCAL_CLOSE;
const proton_event::event_type proton_event::LINK_REMOTE_CLOSE=PN_LINK_REMOTE_CLOSE;
const proton_event::event_type proton_event::LINK_LOCAL_DETACH=PN_LINK_LOCAL_DETACH;
const proton_event::event_type proton_event::LINK_REMOTE_DETACH=PN_LINK_REMOTE_DETACH;
const proton_event::event_type proton_event::LINK_FLOW=PN_LINK_FLOW;
const proton_event::event_type proton_event::LINK_FINAL=PN_LINK_FINAL;
const proton_event::event_type proton_event::DELIVERY=PN_DELIVERY;
const proton_event::event_type proton_event::TRANSPORT=PN_TRANSPORT;
const proton_event::event_type proton_event::TRANSPORT_AUTHENTICATED=PN_TRANSPORT_AUTHENTICATED;
const proton_event::event_type proton_event::TRANSPORT_ERROR=PN_TRANSPORT_ERROR;
const proton_event::event_type proton_event::TRANSPORT_HEAD_CLOSED=PN_TRANSPORT_HEAD_CLOSED;
const proton_event::event_type proton_event::TRANSPORT_TAIL_CLOSED=PN_TRANSPORT_TAIL_CLOSED;
const proton_event::event_type proton_event::TRANSPORT_CLOSED=PN_TRANSPORT_CLOSED;
const proton_event::event_type proton_event::SELECTABLE_INIT=PN_SELECTABLE_INIT;
const proton_event::event_type proton_event::SELECTABLE_UPDATED=PN_SELECTABLE_UPDATED;
const proton_event::event_type proton_event::SELECTABLE_READABLE=PN_SELECTABLE_READABLE;
const proton_event::event_type proton_event::SELECTABLE_WRITABLE=PN_SELECTABLE_WRITABLE;
const proton_event::event_type proton_event::SELECTABLE_ERROR=PN_SELECTABLE_ERROR;
const proton_event::event_type proton_event::SELECTABLE_EXPIRED=PN_SELECTABLE_EXPIRED;
const proton_event::event_type proton_event::SELECTABLE_FINAL=PN_SELECTABLE_FINAL;
}


