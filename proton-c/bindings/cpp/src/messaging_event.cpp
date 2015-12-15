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

        case messaging_event::START:            handler->on_start(*this); break;
        case messaging_event::SENDABLE:         handler->on_sendable(*this); break;
        case messaging_event::MESSAGE:          handler->on_message(*this); break;
        case messaging_event::DISCONNECT:       handler->on_disconnect(*this); break;

        case messaging_event::CONNECTION_CLOSE: handler->on_connection_close(*this); break;
        case messaging_event::CONNECTION_ERROR: handler->on_connection_error(*this); break;
        case messaging_event::CONNECTION_OPEN:  handler->on_connection_open(*this); break;

        case messaging_event::SESSION_CLOSE:    handler->on_session_close(*this); break;
        case messaging_event::SESSION_ERROR:    handler->on_session_error(*this); break;
        case messaging_event::SESSION_OPEN:     handler->on_session_open(*this); break;

        case messaging_event::LINK_CLOSE:       handler->on_link_close(*this); break;
        case messaging_event::LINK_ERROR:       handler->on_link_error(*this); break;
        case messaging_event::LINK_OPEN:        handler->on_link_open(*this); break;

        case messaging_event::DELIVERY_ACCEPT:  handler->on_delivery_accept(*this); break;
        case messaging_event::DELIVERY_REJECT:  handler->on_delivery_reject(*this); break;
        case messaging_event::DELIVERY_RELEASE: handler->on_delivery_release(*this); break;
        case messaging_event::DELIVERY_SETTLE:  handler->on_delivery_settle(*this); break;

        case messaging_event::TRANSACTION_DECLARE: handler->on_transaction_declare(*this); break;
        case messaging_event::TRANSACTION_COMMIT:  handler->on_transaction_commit(*this); break;
        case messaging_event::TRANSACTION_ABORT:   handler->on_transaction_abort(*this); break;

        case messaging_event::TIMER:            handler->on_timer(*this); break;

        default:
            throw error(MSG("Unknown messaging event type " << type_));
            break;
        }
    } else {
        h.on_unhandled(*this);
    }

    // recurse through children
    for (handler::iterator child = h.children_.begin(); child != h.children_.end(); ++child) {
        dispatch(**child);
    }
}

std::string messaging_event::name() const {
    switch (type()) {
      case PROTON: return pn_event_type_name(pn_event_type_t(proton_event::type()));
      case START:            return "START";
      case MESSAGE:          return "MESSAGE";
      case SENDABLE:         return "SENDABLE";
      case DISCONNECT:       return "DISCONNECT";
      case DELIVERY_ACCEPT:  return "DELIVERY_ACCEPT";
      case DELIVERY_REJECT:  return "DELIVERY_REJECT";
      case DELIVERY_RELEASE: return "DELIVERY_RELEASE";
      case DELIVERY_SETTLE:  return "DELIVERY_SETTLE";
      case CONNECTION_CLOSE: return "CONNECTION_CLOSE";
      case CONNECTION_ERROR: return "CONNECTION_ERROR";
      case CONNECTION_OPEN:  return "CONNECTION_OPEN";
      case LINK_CLOSE:       return "LINK_CLOSE";
      case LINK_OPEN:        return "LINK_OPEN";
      case LINK_ERROR:       return "LINK_ERROR";
      case SESSION_CLOSE:    return "SESSION_CLOSE";
      case SESSION_OPEN:     return "SESSION_OPEN";
      case SESSION_ERROR:    return "SESSION_ERROR";
      case TRANSACTION_ABORT:   return "TRANSACTION_ABORT";
      case TRANSACTION_COMMIT:  return "TRANSACTION_COMMIT";
      case TRANSACTION_DECLARE: return "TRANSACTION_DECLARE";
      case TIMER:            return "TIMER";
      default: return "UNKNOWN";
    }
}

}
