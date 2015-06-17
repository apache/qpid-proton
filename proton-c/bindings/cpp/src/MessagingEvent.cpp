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

#include "proton/MessagingEvent.hpp"
#include "proton/Message.hpp"
#include "proton/ProtonHandler.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/Error.hpp"
#include "Msg.hpp"
#include "contexts.hpp"

namespace proton {
namespace reactor {

MessagingEvent::MessagingEvent(pn_event_t *ce, pn_event_type_t t, Container &c) :
    ProtonEvent(ce, t, c), messagingType(PN_MESSAGING_PROTON), parentEvent(0), message(0)
{}

MessagingEvent::MessagingEvent(MessagingEventType_t t, ProtonEvent &p) :
    ProtonEvent(NULL, PN_EVENT_NONE, p.getContainer()), messagingType(t), parentEvent(&p), message(0) {
    if (messagingType == PN_MESSAGING_PROTON)
        throw Error(MSG("invalid messaging event type"));
}

MessagingEvent::~MessagingEvent() {
    delete message;
}

Connection &MessagingEvent::getConnection() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getConnection();
    if (parentEvent)
        return parentEvent->getConnection();
    throw Error(MSG("No connection context for event"));
}

Sender MessagingEvent::getSender() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getSender();
    if (parentEvent)
        return parentEvent->getSender();
    throw Error(MSG("No sender context for event"));
}

Receiver MessagingEvent::getReceiver() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getReceiver();
    if (parentEvent)
        return parentEvent->getReceiver();
    throw Error(MSG("No receiver context for event"));
}

Link MessagingEvent::getLink() {
    if (messagingType == PN_MESSAGING_PROTON)
        return ProtonEvent::getLink();
    if (parentEvent)
        return parentEvent->getLink();
    throw Error(MSG("No link context for event"));
}

Message MessagingEvent::getMessage() {
    if (parentEvent) {
        pn_message_t *m = getEventContext(parentEvent->getPnEvent());
        if (m)
            return Message(m);
    }
    throw Error(MSG("No message context for event"));
}

void MessagingEvent::setMessage(Message &m) {
    if (messagingType != PN_MESSAGING_MESSAGE || !parentEvent)
        throw Error(MSG("Event type does not provide message"));
    setEventContext(parentEvent->getPnEvent(), m.pnMessage());
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
        case PN_MESSAGING_ACCEPTED:    handler->onAccepted(*this); break;
        case PN_MESSAGING_REJECTED:    handler->onRejected(*this); break;
        case PN_MESSAGING_RELEASED:    handler->onReleased(*this); break;
        case PN_MESSAGING_SETTLED:     handler->onSettled(*this); break;

        case PN_MESSAGING_CONNECTION_CLOSING:     handler->onConnectionClosing(*this); break;
        case PN_MESSAGING_CONNECTION_CLOSED:      handler->onConnectionClosed(*this); break;
        case PN_MESSAGING_CONNECTION_ERROR:       handler->onConnectionError(*this); break;
        case PN_MESSAGING_CONNECTION_OPENING:     handler->onConnectionOpening(*this); break;
        case PN_MESSAGING_CONNECTION_OPENED:      handler->onConnectionOpened(*this); break;

        case PN_MESSAGING_LINK_CLOSED:            handler->onLinkClosed(*this); break;
        case PN_MESSAGING_LINK_CLOSING:           handler->onLinkClosing(*this); break;
        case PN_MESSAGING_LINK_ERROR:             handler->onLinkError(*this); break;
        case PN_MESSAGING_LINK_OPENING:           handler->onLinkOpening(*this); break;
        case PN_MESSAGING_LINK_OPENED:            handler->onLinkOpened(*this); break;

        case PN_MESSAGING_SESSION_CLOSED:         handler->onSessionClosed(*this); break;
        case PN_MESSAGING_SESSION_CLOSING:        handler->onSessionClosing(*this); break;
        case PN_MESSAGING_SESSION_ERROR:          handler->onSessionError(*this); break;
        case PN_MESSAGING_SESSION_OPENING:        handler->onSessionOpening(*this); break;
        case PN_MESSAGING_SESSION_OPENED:         handler->onSessionOpened(*this); break;

        case PN_MESSAGING_TRANSPORT_CLOSED:       handler->onTransportClosed(*this); break;
        default:
            throw Error(MSG("Unkown messaging event type " << messagingType));
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

}} // namespace proton::reactor
