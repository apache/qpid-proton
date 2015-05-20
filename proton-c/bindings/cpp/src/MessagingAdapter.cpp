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
#include "proton/cpp/MessagingAdapter.h"
#include "proton/cpp/MessagingEvent.h"
#include "proton/cpp/Sender.h"
#include "proton/cpp/exceptions.h"
#include "Msg.h"

#include "proton/link.h"
#include "proton/handlers.h"
#include "proton/delivery.h"
#include "proton/connection.h"
#include "proton/session.h"

namespace proton {
namespace reactor {
MessagingAdapter::MessagingAdapter(MessagingHandler &delegate_) :
    MessagingHandler(true, delegate_.prefetch, delegate_.autoSettle, delegate_.autoAccept, delegate_.peerCloseIsError),
    delegate(delegate_)
{};


MessagingAdapter::~MessagingAdapter(){};


void MessagingAdapter::onReactorInit(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        MessagingEvent mevent(PN_MESSAGING_START, *pe);
        delegate.onStart(mevent);
    }
}

void MessagingAdapter::onLinkFlow(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *pne = pe->getPnEvent();
        pn_link_t *lnk = pn_event_link(pne);
        if (lnk && pn_link_is_sender(lnk) && pn_link_credit(lnk) > 0) {
            // create onMessage extended event
            MessagingEvent mevent(PN_MESSAGING_SENDABLE, *pe);
            delegate.onSendable(mevent);;
        }
   }
}

namespace {
Message receiveMessage(pn_link_t *lnk, pn_delivery_t *dlv) {
    std::string buf;
    size_t sz = pn_delivery_pending(dlv);
    buf.resize(sz);
    ssize_t n = pn_link_recv(lnk, (char *) buf.data(), sz);
    if (n != (ssize_t) sz)
        throw ProtonException(MSG("link read failure"));
    Message m;
    m. decode(buf);
    pn_link_advance(lnk);
    return m;
}
} // namespace

void MessagingAdapter::onDelivery(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_link_t *lnk = pn_event_link(cevent);
        pn_delivery_t *dlv = pn_event_delivery(cevent);

        if (pn_link_is_receiver(lnk)) {
            if (!pn_delivery_partial(dlv) && pn_delivery_readable(dlv)) {
                // generate onMessage
                MessagingEvent mevent(PN_MESSAGING_MESSAGE, *pe);
                Message m(receiveMessage(lnk, dlv));
                mevent.setMessage(m);
                if (pn_link_state(lnk) & PN_LOCAL_CLOSED) {
                    if (autoAccept) {
                        pn_delivery_update(dlv, PN_RELEASED);
                        pn_delivery_settle(dlv);
                    }
                }
                else {
                    try {
                        delegate.onMessage(mevent);
                        if (autoAccept) {
                            pn_delivery_update(dlv, PN_ACCEPTED);
                            pn_delivery_settle(dlv);
                        }
                    }
                    catch (MessageReject &) {
                        pn_delivery_update(dlv, PN_REJECTED);
                        pn_delivery_settle(dlv);
                    }
                    catch (MessageRelease &) {
                        pn_delivery_update(dlv, PN_REJECTED);
                        pn_delivery_settle(dlv);
                    }
                }
            }
            else if (pn_delivery_updated(dlv) && pn_delivery_settled(dlv)) {
                MessagingEvent mevent(PN_MESSAGING_SETTLED, *pe);
                delegate.onSettled(mevent);
            }
        } else {
            // Sender
            if (pn_delivery_updated(dlv)) {
                uint64_t rstate = pn_delivery_remote_state(dlv);
                if (rstate == PN_ACCEPTED) {
                    MessagingEvent mevent(PN_MESSAGING_ACCEPTED, *pe);
                    delegate.onAccepted(mevent);
                }
                else if (rstate = PN_REJECTED) {
                    MessagingEvent mevent(PN_MESSAGING_REJECTED, *pe);
                    delegate.onRejected(mevent);
                }
                else if (rstate == PN_RELEASED || rstate == PN_MODIFIED) {
                    MessagingEvent mevent(PN_MESSAGING_RELEASED, *pe);
                    delegate.onReleased(mevent);
                }

                if (pn_delivery_settled(dlv)) {
                    MessagingEvent mevent(PN_MESSAGING_SETTLED, *pe);
                    delegate.onSettled(mevent);
                }
                if (autoSettle)
                    pn_delivery_settle(dlv);
            }
        }
    }
}

namespace {

bool isLocalOpen(pn_state_t state) {
    return state & PN_LOCAL_ACTIVE;
}

bool isLocalUnititialised(pn_state_t state) {
    return state & PN_LOCAL_UNINIT;
}

bool isLocalClosed(pn_state_t state) {
    return state & PN_LOCAL_CLOSED;
}

bool isRemoteOpen(pn_state_t state) {
    return state & PN_REMOTE_ACTIVE;
}

bool isRemoteClosed(pn_state_t state) {
    return state & PN_REMOTE_CLOSED;
}

} // namespace

void MessagingAdapter::onLinkRemoteClose(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_link_t *lnk = pn_event_link(cevent);
        pn_state_t state = pn_link_state(lnk);
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            MessagingEvent mevent(PN_MESSAGING_LINK_ERROR, *pe);
            onLinkError(mevent);
        }
        else if (isLocalClosed(state)) {
            MessagingEvent mevent(PN_MESSAGING_LINK_CLOSED, *pe);
            onLinkClosed(mevent);
        }
        else {
            MessagingEvent mevent(PN_MESSAGING_LINK_CLOSING, *pe);
            onLinkClosing(mevent);
        }
        pn_link_close(lnk);
    }
}

void MessagingAdapter::onSessionRemoteClose(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_session_t *session = pn_event_session(cevent);
        pn_state_t state = pn_session_state(session);
        if (pn_condition_is_set(pn_session_remote_condition(session))) {
            MessagingEvent mevent(PN_MESSAGING_SESSION_ERROR, *pe);
            onSessionError(mevent);
        }
        else if (isLocalClosed(state)) {
            MessagingEvent mevent(PN_MESSAGING_SESSION_CLOSED, *pe);
            onSessionClosed(mevent);
        }
        else {
            MessagingEvent mevent(PN_MESSAGING_SESSION_CLOSING, *pe);
            onSessionClosing(mevent);
        }
        pn_session_close(session);
    }
}

void MessagingAdapter::onConnectionRemoteClose(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_connection_t *connection = pn_event_connection(cevent);
        pn_state_t state = pn_connection_state(connection);
        if (pn_condition_is_set(pn_connection_remote_condition(connection))) {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_ERROR, *pe);
            onConnectionError(mevent);
        }
        else if (isLocalClosed(state)) {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_CLOSED, *pe);
            onConnectionClosed(mevent);
        }
        else {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_CLOSING, *pe);
            onConnectionClosing(mevent);
        }
        pn_connection_close(connection);
    }
}

void MessagingAdapter::onConnectionLocalOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->getPnEvent());
        if (isRemoteOpen(pn_connection_state(connection))) {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_OPENED, *pe);
            onConnectionOpened(mevent);
        }
    }
}

void MessagingAdapter::onConnectionRemoteOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->getPnEvent());
        if (isLocalOpen(pn_connection_state(connection))) {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_OPENED, *pe);
            onConnectionOpened(mevent);
        }
        else if (isLocalUnititialised(pn_connection_state(connection))) {
            MessagingEvent mevent(PN_MESSAGING_CONNECTION_OPENING, *pe);
            onConnectionOpening(mevent);
            pn_connection_open(connection);
        }
    }
}

void MessagingAdapter::onSessionLocalOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->getPnEvent());
        if (isRemoteOpen(pn_session_state(session))) {
            MessagingEvent mevent(PN_MESSAGING_SESSION_OPENED, *pe);
            onSessionOpened(mevent);
        }
    }
}

void MessagingAdapter::onSessionRemoteOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->getPnEvent());
        if (isLocalOpen(pn_session_state(session))) {
            MessagingEvent mevent(PN_MESSAGING_SESSION_OPENED, *pe);
            onSessionOpened(mevent);
        }
        else if (isLocalUnititialised(pn_session_state(session))) {
            MessagingEvent mevent(PN_MESSAGING_SESSION_OPENING, *pe);
            onSessionOpening(mevent);
            pn_session_open(session);
        }
    }
}

void MessagingAdapter::onLinkLocalOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->getPnEvent());
        if (isRemoteOpen(pn_link_state(link))) {
            MessagingEvent mevent(PN_MESSAGING_LINK_OPENED, *pe);
            onLinkOpened(mevent);
        }
    }
}

void MessagingAdapter::onLinkRemoteOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->getPnEvent());
        if (isLocalOpen(pn_link_state(link))) {
            MessagingEvent mevent(PN_MESSAGING_LINK_OPENED, *pe);
            onLinkOpened(mevent);
        }
        else if (isLocalUnititialised(pn_link_state(link))) {
            MessagingEvent mevent(PN_MESSAGING_LINK_OPENING, *pe);
            onLinkOpening(mevent);
            pn_link_open(link);
        }
    }
}

void MessagingAdapter::onTransportTailClosed(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_connection_t *conn = pn_event_connection(pe->getPnEvent());
        if (conn && isLocalOpen(pn_connection_state(conn))) {
            MessagingEvent mevent(PN_MESSAGING_DISCONNECTED, *pe);
            delegate.onDisconnected(mevent);
        }
    }
}


void MessagingAdapter::onConnectionOpened(Event &e) {
    delegate.onConnectionOpened(e);
}

void MessagingAdapter::onSessionOpened(Event &e) {
    delegate.onSessionOpened(e);
}

void MessagingAdapter::onLinkOpened(Event &e) {
    delegate.onLinkOpened(e);
}

void MessagingAdapter::onConnectionOpening(Event &e) {
    delegate.onConnectionOpening(e);
}

void MessagingAdapter::onSessionOpening(Event &e) {
    delegate.onSessionOpening(e);
}

void MessagingAdapter::onLinkOpening(Event &e) {
    delegate.onLinkOpening(e);
}

void MessagingAdapter::onConnectionError(Event &e) {
    delegate.onConnectionError(e);
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->getPnEvent());
        pn_connection_close(connection);
    }
}

void MessagingAdapter::onSessionError(Event &e) {
    delegate.onSessionError(e);
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->getPnEvent());
        pn_session_close(session);
    }
}

void MessagingAdapter::onLinkError(Event &e) {
    delegate.onLinkError(e);
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->getPnEvent());
        pn_link_close(link);
    }
}

void MessagingAdapter::onConnectionClosed(Event &e) {
    delegate.onConnectionClosed(e);
}

void MessagingAdapter::onSessionClosed(Event &e) {
    delegate.onSessionClosed(e);
}

void MessagingAdapter::onLinkClosed(Event &e) {
    delegate.onLinkClosed(e);
}

void MessagingAdapter::onConnectionClosing(Event &e) {
    delegate.onConnectionClosing(e);
    if (peerCloseIsError)
        onConnectionError(e);
}

void MessagingAdapter::onSessionClosing(Event &e) {
    delegate.onSessionClosing(e);
    if (peerCloseIsError)
        onSessionError(e);
}

void MessagingAdapter::onLinkClosing(Event &e) {
    delegate.onLinkClosing(e);
    if (peerCloseIsError)
        onLinkError(e);
}

void MessagingAdapter::onUnhandled(Event &e) {
}

}} // namespace proton::reactor
