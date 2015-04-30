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

#include "proton/cpp/ProtonEvent.h"
#include "proton/cpp/ProtonHandler.h"
#include "proton/cpp/exceptions.h"
#include "proton/cpp/Container.h"

#include "ConnectionImpl.h"
#include "Msg.h"
#include "contexts.h"

namespace proton {
namespace reactor {

ProtonEvent::ProtonEvent(pn_event_t *ce, pn_event_type_t t, Container &c) :
        pnEvent(ce),
        type((int) t),
        container(c)
{}

int ProtonEvent::getType() { return type; }

pn_event_t *ProtonEvent::getPnEvent() { return pnEvent; }

Container &ProtonEvent::getContainer() { return container; }

Connection &ProtonEvent::getConnection() {
    pn_connection_t *conn = pn_event_connection(getPnEvent());
    if (!conn)
        throw ProtonException(MSG("No connection context for this event"));
    return ConnectionImpl::getReactorReference(conn);
}

Sender ProtonEvent::getSender() {
    pn_link_t *lnk = pn_event_link(getPnEvent());
    if (lnk && pn_link_is_sender(lnk))
        return Sender(lnk);
    throw ProtonException(MSG("No sender context for this event"));
}

Receiver ProtonEvent::getReceiver() {
    pn_link_t *lnk = pn_event_link(getPnEvent());
    if (lnk && pn_link_is_receiver(lnk))
        return Receiver(lnk);
    throw ProtonException(MSG("No receiver context for this event"));
}

Link ProtonEvent::getLink() {
    pn_link_t *lnk = pn_event_link(getPnEvent());
    if (lnk)
        if (pn_link_is_sender(lnk))
            return Sender(lnk);
        else
            return Receiver(lnk);
    throw ProtonException(MSG("No link context for this event"));
}




void ProtonEvent::dispatch(Handler &h) {
    ProtonHandler *handler = dynamic_cast<ProtonHandler*>(&h);

    if (handler) {
        switch(type) {

        case PN_REACTOR_INIT: handler->onReactorInit(*this); break;
        case PN_REACTOR_QUIESCED: handler->onReactorQuiesced(*this); break;
        case PN_REACTOR_FINAL: handler->onReactorFinal(*this); break;

        case PN_TIMER_TASK: handler->onTimerTask(*this); break;

        case PN_CONNECTION_INIT: handler->onConnectionInit(*this); break;
        case PN_CONNECTION_BOUND: handler->onConnectionBound(*this); break;
        case PN_CONNECTION_UNBOUND: handler->onConnectionUnbound(*this); break;
        case PN_CONNECTION_LOCAL_OPEN: handler->onConnectionLocalOpen(*this); break;
        case PN_CONNECTION_LOCAL_CLOSE: handler->onConnectionLocalClose(*this); break;
        case PN_CONNECTION_REMOTE_OPEN: handler->onConnectionRemoteOpen(*this); break;
        case PN_CONNECTION_REMOTE_CLOSE: handler->onConnectionRemoteClose(*this); break;
        case PN_CONNECTION_FINAL: handler->onConnectionFinal(*this); break;

        case PN_SESSION_INIT: handler->onSessionInit(*this); break;
        case PN_SESSION_LOCAL_OPEN: handler->onSessionLocalOpen(*this); break;
        case PN_SESSION_LOCAL_CLOSE: handler->onSessionLocalClose(*this); break;
        case PN_SESSION_REMOTE_OPEN: handler->onSessionRemoteOpen(*this); break;
        case PN_SESSION_REMOTE_CLOSE: handler->onSessionRemoteClose(*this); break;
        case PN_SESSION_FINAL: handler->onSessionFinal(*this); break;

        case PN_LINK_INIT: handler->onLinkInit(*this); break;
        case PN_LINK_LOCAL_OPEN: handler->onLinkLocalOpen(*this); break;
        case PN_LINK_LOCAL_CLOSE: handler->onLinkLocalClose(*this); break;
        case PN_LINK_LOCAL_DETACH: handler->onLinkLocalDetach(*this); break;
        case PN_LINK_REMOTE_OPEN: handler->onLinkRemoteOpen(*this); break;
        case PN_LINK_REMOTE_CLOSE: handler->onLinkRemoteClose(*this); break;
        case PN_LINK_REMOTE_DETACH: handler->onLinkRemoteDetach(*this); break;
        case PN_LINK_FLOW: handler->onLinkFlow(*this); break;
        case PN_LINK_FINAL: handler->onLinkFinal(*this); break;

        case PN_DELIVERY: handler->onDelivery(*this); break;

        case PN_TRANSPORT: handler->onTransport(*this); break;
        case PN_TRANSPORT_ERROR: handler->onTransportError(*this); break;
        case PN_TRANSPORT_HEAD_CLOSED: handler->onTransportHeadClosed(*this); break;
        case PN_TRANSPORT_TAIL_CLOSED: handler->onTransportTailClosed(*this); break;
        case PN_TRANSPORT_CLOSED: handler->onTransportClosed(*this); break;

        case PN_SELECTABLE_INIT: handler->onSelectableInit(*this); break;
        case PN_SELECTABLE_UPDATED: handler->onSelectableUpdated(*this); break;
        case PN_SELECTABLE_READABLE: handler->onSelectableReadable(*this); break;
        case PN_SELECTABLE_WRITABLE: handler->onSelectableWritable(*this); break;
        case PN_SELECTABLE_EXPIRED: handler->onSelectableExpired(*this); break;
        case PN_SELECTABLE_ERROR: handler->onSelectableError(*this); break;
        case PN_SELECTABLE_FINAL: handler->onSelectableFinal(*this); break;
        default:
            throw ProtonException(MSG("Invalid Proton event type " << type));
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
