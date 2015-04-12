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

#include "proton/cpp/MessagingEvent.h"
#include "proton/cpp/ProtonHandler.h"
#include "proton/cpp/MessagingHandler.h"
#include "contexts.h"

namespace proton {
namespace cpp {
namespace reactor {

MessagingEvent::MessagingEvent(pn_event_t *ce, pn_event_type_t t, Container &c) :
    ProtonEvent(ce, t, c), messagingType(PN_MESSAGING_PROTON), parentEvent(0), message(0)
{}

MessagingEvent::MessagingEvent(MessagingEventType_t t, ProtonEvent *p, Container &c) :
    ProtonEvent(NULL, PN_EVENT_NONE, c), messagingType(t), parentEvent(p), message(0) {
    if (messagingType == PN_MESSAGING_PROTON)
        throw "TODO: invalid messaging event type";
}

MessagingEvent::~MessagingEvent() {
    delete message;
}

Connection &MessagingEvent::getConnection() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getConnection();
    if (parentEvent)
        return parentEvent->getConnection();
    throw "TODO: no connection context exception";
}

Sender MessagingEvent::getSender() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getSender();
    if (parentEvent)
        return parentEvent->getSender();
    throw "TODO: no sender context exception";
}

Receiver MessagingEvent::getReceiver() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getReceiver();
    if (parentEvent)
        return parentEvent->getReceiver();
    throw "TODO: no receiver context exception";
}

Link MessagingEvent::getLink() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getLink();
    if (parentEvent)
        return parentEvent->getLink();
    throw "TODO: no link context exception";
}

Message MessagingEvent::getMessage() {
    if (message)
        return *message;
    throw "No message context for event";
}

void MessagingEvent::setMessage(Message &m) {
    if (messagingType != PN_MESSAGING_MESSAGE)
        throw "Event type does not provide message";
    delete message;
    message = new Message(m);
}

void MessagingEvent::dispatch(Handler &h) {
    if (messagingType == PN_MESSAGING_PROTON) {
        ProtonEvent::dispatch(h);
        return;
    }

    MessagingHandler *handler = dynamic_cast<MessagingHandler*>(&h);
    if (handler) {
        switch(messagingType) {

        case PN_MESSAGING_START:       handler->onStart(*this); break;
        case PN_MESSAGING_SENDABLE:    handler->onSendable(*this); break;
        case PN_MESSAGING_MESSAGE:     handler->onMessage(*this); break;

        default:
            throw "TODO: real exception";
            break;
        }
    } else {
        h.onUnhandled(*this);
    }

    // recurse through children
    for (std::vector<Handler *>::iterator child = h.childHandlersBegin(); 
         child != h.childHandlersEnd(); ++child) {
        dispatch(**child);
    }
}

}}} // namespace proton::cpp::reactor
