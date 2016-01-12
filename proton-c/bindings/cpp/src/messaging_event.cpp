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

#include "messaging_event.hpp"
#include "proton/message.hpp"
#include "proton/handler.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/transport.hpp"
#include "proton/error.hpp"

#include "contexts.hpp"
#include "msg.hpp"
#include "proton_handler.hpp"

#include "proton/reactor.h"
#include "proton/event.h"
#include "proton/link.h"

/*
 * Performance note:
 * See comments for handler_context::dispatch() in container_impl.cpp.
 */

namespace proton {

messaging_event::messaging_event(event_type t, proton_event &p) :
    type_(t), parent_event_(&p), message_(0)
{}

messaging_event::~messaging_event() {}

messaging_event::event_type messaging_event::type() const { return type_; }

container& messaging_event::container() const {
    if (parent_event_)
        return parent_event_->container();
    throw error(MSG("No container context for event"));
}

transport messaging_event::transport() const {
    if (parent_event_)
        return parent_event_->transport();
    throw error(MSG("No transport context for event"));
}

connection messaging_event::connection() const {
    if (parent_event_)
        return parent_event_->connection();
    throw error(MSG("No connection context for event"));
}

session messaging_event::session() const {
    if (parent_event_)
        return parent_event_->session();
    throw error(MSG("No session context for event"));
}

sender messaging_event::sender() const {
    if (parent_event_)
        return parent_event_->sender();
    throw error(MSG("No sender context for event"));
}

receiver messaging_event::receiver() const {
    if (parent_event_)
        return parent_event_->receiver();
    throw error(MSG("No receiver context for event"));
}

link messaging_event::link() const {
    if (parent_event_)
        return parent_event_->link();
    throw error(MSG("No link context for event"));
}

delivery messaging_event::delivery() const {
    if (parent_event_)
        return parent_event_->delivery();
    throw error(MSG("No delivery context for event"));
}

message &messaging_event::message() const {
    if (type_ != messaging_event::MESSAGE || !parent_event_)
        throw error(MSG("event type does not provide message"));
    return *message_;
}

std::string messaging_event::name() const {
    switch (type()) {
      case START:            return "START";
      case MESSAGE:          return "MESSAGE";
      case SENDABLE:         return "SENDABLE";
      case TRANSPORT_CLOSE:  return "TRANSPORT_CLOSE";
      case TRANSPORT_ERROR:  return "TRANSPORT_ERROR";
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
    }
    return "unknown";
}

}
