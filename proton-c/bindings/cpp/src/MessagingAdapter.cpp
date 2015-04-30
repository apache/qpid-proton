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

namespace proton {
namespace reactor {

MessagingAdapter::MessagingAdapter(MessagingHandler &d) : delegate(d), handshaker(pn_handshaker()) {
    pn_handler_t *flowcontroller = pn_flowcontroller(10);
    pn_handler_add(handshaker, flowcontroller);
    pn_decref(flowcontroller);
};
MessagingAdapter::~MessagingAdapter(){
    pn_decref(handshaker);
};

void MessagingAdapter::onReactorInit(Event &e) {
    // create onStart extended event
    MessagingEvent mevent(PN_MESSAGING_START, NULL, e.getContainer());
    mevent.dispatch(delegate);
}

void MessagingAdapter::onLinkFlow(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *pne = pe->getPnEvent();
        pn_link_t *lnk = pn_event_link(pne);
        if (lnk && pn_link_is_sender(lnk) && pn_link_credit(lnk) > 0) {
            // create onMessage extended event
            MessagingEvent mevent(PN_MESSAGING_SENDABLE, pe, e.getContainer());
            mevent.dispatch(delegate);
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
                MessagingEvent mevent(PN_MESSAGING_MESSAGE, pe, pe->getContainer());
                Message m(receiveMessage(lnk, dlv));
                mevent.setMessage(m);
                // TODO: check if endpoint closed...
                mevent.dispatch(delegate);
                // only do auto accept for now
                pn_delivery_update(dlv, PN_ACCEPTED);
                pn_delivery_settle(dlv);
                // TODO: generate onSettled
            }
        } else {
            // Sender
            if (pn_delivery_updated(dlv)) {
                uint64_t rstate = pn_delivery_remote_state(dlv);
                if (rstate == PN_ACCEPTED)
                    // generate onAccepted
                    MessagingEvent(PN_MESSAGING_ACCEPTED, pe, pe->getContainer()).dispatch(delegate);
                else if (rstate = PN_REJECTED)
                    MessagingEvent(PN_MESSAGING_REJECTED, pe, pe->getContainer()).dispatch(delegate);
                else if (rstate == PN_RELEASED || rstate == PN_MODIFIED)
                    MessagingEvent(PN_MESSAGING_RELEASED, pe, pe->getContainer()).dispatch(delegate);

                if (pn_delivery_settled(dlv))
                    MessagingEvent(PN_MESSAGING_SETTLED, pe, pe->getContainer()).dispatch(delegate);

                pn_delivery_settle(dlv); // TODO: only if auto settled
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

void MessagingAdapter::onConnectionRemoteClose(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_connection_t *conn = pn_event_connection(cevent);
        // TODO: remote condition -> error
        if (isLocalClosed(pn_connection_state(conn))) {
            MessagingEvent(PN_MESSAGING_CONNECTION_CLOSED, pe, pe->getContainer()).dispatch(delegate);
        }
        else {
            MessagingEvent(PN_MESSAGING_CONNECTION_CLOSING, pe, pe->getContainer()).dispatch(delegate);
        }
        pn_connection_close(conn);
    }
}


void MessagingAdapter::onLinkRemoteOpen(Event &e) {
    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->getPnEvent();
        pn_link_t *link = pn_event_link(cevent);
        // TODO: remote condition -> error
        if (isLocalOpen(pn_link_state(link))) {
            MessagingEvent(PN_MESSAGING_LINK_OPENED, pe, pe->getContainer()).dispatch(delegate);
        }
        else if (isLocalUnititialised(pn_link_state(link))) {
            MessagingEvent(PN_MESSAGING_LINK_OPENING, pe, pe->getContainer()).dispatch(delegate);
            pn_link_open(link);
        }
    }
}


void MessagingAdapter::onUnhandled(Event &e) {
    // Until this code fleshes out closer to python's, cheat a bit with a pn_handshaker

    ProtonEvent *pe = dynamic_cast<ProtonEvent*>(&e);
    if (pe) {
        pn_event_type_t type = (pn_event_type_t) pe->getType();
        if (type != PN_EVENT_NONE) {
            pn_handler_dispatch(handshaker, pe->getPnEvent(), type);
        }
    }
}



}} // namespace proton::reactor
