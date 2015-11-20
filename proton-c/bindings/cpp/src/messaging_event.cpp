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

#include "messaging_event.hpp"
#include "proton/message.hpp"
#include "proton/proton_handler.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "contexts.hpp"

/*
 * Performance note:
 * See comments for handler_context::dispatch() in container_impl.cpp.
 */

namespace proton {

messaging_event::messaging_event(pn_event_t *ce, proton_event::event_type t, class event_loop *el) :
    proton_event(ce, t, el), type_(messaging_event::PROTON), parent_event_(0), message_(0)
{}

messaging_event::messaging_event(event_type t, proton_event &p) :
    proton_event(NULL, PN_EVENT_NONE, &p.event_loop()), type_(t), parent_event_(&p), message_(0)
{
    if (type_ == messaging_event::PROTON)
        throw error(MSG("invalid messaging event type"));
}

messaging_event::~messaging_event() {}

messaging_event::event_type messaging_event::type() const { return type_; }

connection messaging_event::connection() const {
    if (type_ == messaging_event::PROTON)
        return proton_event::connection();
    if (parent_event_)
        return parent_event_->connection();
    throw error(MSG("No connection context for event"));
}

sender messaging_event::sender() const {
    if (type_ == messaging_event::PROTON)
        return proton_event::sender();
    if (parent_event_)
        return parent_event_->sender();
    throw error(MSG("No sender context for event"));
}

receiver messaging_event::receiver() const {
    if (type_ == messaging_event::PROTON)
        return proton_event::receiver();
    if (parent_event_)
        return parent_event_->receiver();
    throw error(MSG("No receiver context for event"));
}

link messaging_event::link() const {
    if (type_ == messaging_event::PROTON)
        return proton_event::link();
    if (parent_event_)
        return parent_event_->link();
    throw error(MSG("No link context for event"));
}

delivery messaging_event::delivery() const {
    if (type_ == messaging_event::PROTON)
        return proton_event::delivery();
    if (parent_event_)
        return parent_event_->delivery();
    throw error(MSG("No delivery context for event"));
}

message &messaging_event::message() const {
    if (type_ != messaging_event::MESSAGE || !parent_event_)
        throw error(MSG("event type does not provide message"));
    return *message_;
}

void messaging_event::dispatch(handler &h) {
    if (type_ == messaging_event::PROTON) {
        proton_event::dispatch(h);
        return;
    }

    messaging_handler *handler = dynamic_cast<messaging_handler*>(&h);
    if (handler) {
        switch(type_) {

        case messaging_event::START:       handler->on_start(*this); break;
        case messaging_event::SENDABLE:    handler->on_sendable(*this); break;
        case messaging_event::MESSAGE:     handler->on_message(*this); break;
        case messaging_event::ACCEPTED:    handler->on_accepted(*this); break;
        case messaging_event::REJECTED:    handler->on_rejected(*this); break;
        case messaging_event::RELEASED:    handler->on_released(*this); break;
        case messaging_event::SETTLED:     handler->on_settled(*this); break;

        case messaging_event::CONNECTION_CLOSING:     handler->on_connection_closing(*this); break;
        case messaging_event::CONNECTION_CLOSED:      handler->on_connection_closed(*this); break;
        case messaging_event::CONNECTION_ERROR:       handler->on_connection_error(*this); break;
        case messaging_event::CONNECTION_OPENING:     handler->on_connection_opening(*this); break;
        case messaging_event::CONNECTION_OPENED:      handler->on_connection_opened(*this); break;

        case messaging_event::LINK_CLOSED:            handler->on_link_closed(*this); break;
        case messaging_event::LINK_CLOSING:           handler->on_link_closing(*this); break;
        case messaging_event::LINK_ERROR:             handler->on_link_error(*this); break;
        case messaging_event::LINK_OPENING:           handler->on_link_opening(*this); break;
        case messaging_event::LINK_OPENED:            handler->on_link_opened(*this); break;

        case messaging_event::SESSION_CLOSED:         handler->on_session_closed(*this); break;
        case messaging_event::SESSION_CLOSING:        handler->on_session_closing(*this); break;
        case messaging_event::SESSION_ERROR:          handler->on_session_error(*this); break;
        case messaging_event::SESSION_OPENING:        handler->on_session_opening(*this); break;
        case messaging_event::SESSION_OPENED:         handler->on_session_opened(*this); break;

        case messaging_event::TRANSPORT_CLOSED:       handler->on_transport_closed(*this); break;
        default:
            throw error(MSG("Unkown messaging event type " << type_));
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

std::string messaging_event::name() const {
    switch (type()) {
      case PROTON: return pn_event_type_name(pn_event_type_t(proton_event::type()));
      case ABORT: return "ABORT";
      case ACCEPTED: return "ACCEPTED";
      case COMMIT: return "COMMIT";
      case CONNECTION_CLOSED: return "CONNECTION_CLOSED";
      case CONNECTION_CLOSING: return "CONNECTION_CLOSING";
      case CONNECTION_ERROR: return "CONNECTION_ERROR";
      case CONNECTION_OPENED: return "CONNECTION_OPENED";
      case CONNECTION_OPENING: return "CONNECTION_OPENING";
      case DISCONNECTED: return "DISCONNECTED";
      case FETCH: return "FETCH";
      case ID_LOADED: return "ID_LOADED";
      case LINK_CLOSED: return "LINK_CLOSED";
      case LINK_CLOSING: return "LINK_CLOSING";
      case LINK_OPENED: return "LINK_OPENED";
      case LINK_OPENING: return "LINK_OPENING";
      case LINK_ERROR: return "LINK_ERROR";
      case MESSAGE: return "MESSAGE";
      case QUIT: return "QUIT";
      case RECORD_INSERTED: return "RECORD_INSERTED";
      case RECORDS_LOADED: return "RECORDS_LOADED";
      case REJECTED: return "REJECTED";
      case RELEASED: return "RELEASED";
      case REQUEST: return "REQUEST";
      case RESPONSE: return "RESPONSE";
      case SENDABLE: return "SENDABLE";
      case SESSION_CLOSED: return "SESSION_CLOSED";
      case SESSION_CLOSING: return "SESSION_CLOSING";
      case SESSION_OPENED: return "SESSION_OPENED";
      case SESSION_OPENING: return "SESSION_OPENING";
      case SESSION_ERROR: return "SESSION_ERROR";
      case SETTLED: return "SETTLED";
      case START: return "START";
      case TIMER: return "TIMER";
      case TRANSACTION_ABORTED: return "TRANSACTION_ABORTED";
      case TRANSACTION_COMMITTED: return "TRANSACTION_COMMITTED";
      case TRANSACTION_DECLARED: return "TRANSACTION_DECLARED";
      case TRANSPORT_CLOSED: return "TRANSPORT_CLOSED";
      default: return "UNKNOWN";
    }
}

}
