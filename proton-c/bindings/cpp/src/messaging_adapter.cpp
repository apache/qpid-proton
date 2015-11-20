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
#include "proton/messaging_adapter.hpp"
#include "messaging_event.hpp"
#include "proton/sender.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/link.h"
#include "proton/handlers.h"
#include "proton/delivery.h"
#include "proton/connection.h"
#include "proton/session.h"
#include "proton/message.h"

namespace proton {
messaging_adapter::messaging_adapter(messaging_handler &delegate_) :
    messaging_handler(true, delegate_.prefetch_, delegate_.auto_settle_, delegate_.auto_accept_, delegate_.peer_close_iserror_),
    delegate_(delegate_)
{}


messaging_adapter::~messaging_adapter(){}


void messaging_adapter::on_reactor_init(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        messaging_event mevent(messaging_event::START, *pe);
        delegate_.on_start(mevent);
    }
}

void messaging_adapter::on_link_flow(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_event_t *pne = pe->pn_event();
        pn_link_t *lnk = pn_event_link(pne);
        if (lnk && pn_link_is_sender(lnk) && pn_link_credit(lnk) > 0) {
            // create on_message extended event
            messaging_event mevent(messaging_event::SENDABLE, *pe);
            delegate_.on_sendable(mevent);;
        }
   }
}

void messaging_adapter::on_delivery(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->pn_event();
        pn_link_t *lnk = pn_event_link(cevent);
        delivery dlv = pe->delivery();

        if (pn_link_is_receiver(lnk)) {
            if (!dlv.partial() && dlv.readable()) {
                // generate on_message
                messaging_event mevent(messaging_event::MESSAGE, *pe);
                pn_connection_t *pnc = pn_session_connection(pn_link_session(lnk));
                struct connection_context& ctx = connection_context::get(pnc);
                // Reusable per-connection message.
                // Avoid expensive heap malloc/free overhead.
                // See PROTON-998
                class message &msg(ctx.event_message);
                mevent.message_ = &msg;
                mevent.message_->decode(lnk, dlv);
                if (pn_link_state(lnk) & PN_LOCAL_CLOSED) {
                    if (auto_accept_)
                        dlv.release();
                } else {
                    delegate_.on_message(mevent);
                    if (auto_accept_ && !dlv.settled())
                        dlv.accept();
                }
            }
            else if (dlv.updated() && dlv.settled()) {
                messaging_event mevent(messaging_event::SETTLED, *pe);
                delegate_.on_settled(mevent);
            }
        } else {
            // sender
            if (dlv.updated()) {
                amqp_ulong rstate = dlv.remote_state();
                if (rstate == PN_ACCEPTED) {
                    messaging_event mevent(messaging_event::ACCEPTED, *pe);
                    delegate_.on_accepted(mevent);
                }
                else if (rstate == PN_REJECTED) {
                    messaging_event mevent(messaging_event::REJECTED, *pe);
                    delegate_.on_rejected(mevent);
                }
                else if (rstate == PN_RELEASED || rstate == PN_MODIFIED) {
                    messaging_event mevent(messaging_event::RELEASED, *pe);
                    delegate_.on_released(mevent);
                }

                if (dlv.settled()) {
                    messaging_event mevent(messaging_event::SETTLED, *pe);
                    delegate_.on_settled(mevent);
                }
                if (auto_settle_)
                    dlv.settle();
            }
        }
    }
}

namespace {

bool is_local_open(pn_state_t state) {
    return state & PN_LOCAL_ACTIVE;
}

bool is_local_unititialised(pn_state_t state) {
    return state & PN_LOCAL_UNINIT;
}

bool is_local_closed(pn_state_t state) {
    return state & PN_LOCAL_CLOSED;
}

bool is_remote_open(pn_state_t state) {
    return state & PN_REMOTE_ACTIVE;
}

} // namespace

void messaging_adapter::on_link_remote_close(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->pn_event();
        pn_link_t *lnk = pn_event_link(cevent);
        pn_state_t state = pn_link_state(lnk);
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            messaging_event mevent(messaging_event::LINK_ERROR, *pe);
            on_link_error(mevent);
        }
        else if (is_local_closed(state)) {
            messaging_event mevent(messaging_event::LINK_CLOSED, *pe);
            on_link_closed(mevent);
        }
        else {
            messaging_event mevent(messaging_event::LINK_CLOSING, *pe);
            on_link_closing(mevent);
        }
        pn_link_close(lnk);
    }
}

void messaging_adapter::on_session_remote_close(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->pn_event();
        pn_session_t *session = pn_event_session(cevent);
        pn_state_t state = pn_session_state(session);
        if (pn_condition_is_set(pn_session_remote_condition(session))) {
            messaging_event mevent(messaging_event::SESSION_ERROR, *pe);
            on_session_error(mevent);
        }
        else if (is_local_closed(state)) {
            messaging_event mevent(messaging_event::SESSION_CLOSED, *pe);
            on_session_closed(mevent);
        }
        else {
            messaging_event mevent(messaging_event::SESSION_CLOSING, *pe);
            on_session_closing(mevent);
        }
        pn_session_close(session);
    }
}

void messaging_adapter::on_connection_remote_close(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_event_t *cevent = pe->pn_event();
        pn_connection_t *connection = pn_event_connection(cevent);
        pn_state_t state = pn_connection_state(connection);
        if (pn_condition_is_set(pn_connection_remote_condition(connection))) {
            messaging_event mevent(messaging_event::CONNECTION_ERROR, *pe);
            on_connection_error(mevent);
        }
        else if (is_local_closed(state)) {
            messaging_event mevent(messaging_event::CONNECTION_CLOSED, *pe);
            on_connection_closed(mevent);
        }
        else {
            messaging_event mevent(messaging_event::CONNECTION_CLOSING, *pe);
            on_connection_closing(mevent);
        }
        pn_connection_close(connection);
    }
}

void messaging_adapter::on_connection_local_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->pn_event());
        if (is_remote_open(pn_connection_state(connection))) {
            messaging_event mevent(messaging_event::CONNECTION_OPENED, *pe);
            on_connection_opened(mevent);
        }
    }
}

void messaging_adapter::on_connection_remote_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->pn_event());
        if (is_local_open(pn_connection_state(connection))) {
            messaging_event mevent(messaging_event::CONNECTION_OPENED, *pe);
            on_connection_opened(mevent);
        }
        else if (is_local_unititialised(pn_connection_state(connection))) {
            messaging_event mevent(messaging_event::CONNECTION_OPENING, *pe);
            on_connection_opening(mevent);
            pn_connection_open(connection);
        }
    }
}

void messaging_adapter::on_session_local_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->pn_event());
        if (is_remote_open(pn_session_state(session))) {
            messaging_event mevent(messaging_event::SESSION_OPENED, *pe);
            on_session_opened(mevent);
        }
    }
}

void messaging_adapter::on_session_remote_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->pn_event());
        if (is_local_open(pn_session_state(session))) {
            messaging_event mevent(messaging_event::SESSION_OPENED, *pe);
            on_session_opened(mevent);
        }
        else if (is_local_unititialised(pn_session_state(session))) {
            messaging_event mevent(messaging_event::SESSION_OPENING, *pe);
            on_session_opening(mevent);
            pn_session_open(session);
        }
    }
}

void messaging_adapter::on_link_local_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->pn_event());
        if (is_remote_open(pn_link_state(link))) {
            messaging_event mevent(messaging_event::LINK_OPENED, *pe);
            on_link_opened(mevent);
        }
    }
}

void messaging_adapter::on_link_remote_open(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->pn_event());
        if (is_local_open(pn_link_state(link))) {
            messaging_event mevent(messaging_event::LINK_OPENED, *pe);
            on_link_opened(mevent);
        }
        else if (is_local_unititialised(pn_link_state(link))) {
            messaging_event mevent(messaging_event::LINK_OPENING, *pe);
            on_link_opening(mevent);
            pn_link_open(link);
        }
    }
}

void messaging_adapter::on_transport_tail_closed(event &e) {
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_connection_t *conn = pn_event_connection(pe->pn_event());
        if (conn && is_local_open(pn_connection_state(conn))) {
            messaging_event mevent(messaging_event::DISCONNECTED, *pe);
            delegate_.on_disconnected(mevent);
        }
    }
}


void messaging_adapter::on_connection_opened(event &e) {
    delegate_.on_connection_opened(e);
}

void messaging_adapter::on_session_opened(event &e) {
    delegate_.on_session_opened(e);
}

void messaging_adapter::on_link_opened(event &e) {
    delegate_.on_link_opened(e);
}

void messaging_adapter::on_connection_opening(event &e) {
    delegate_.on_connection_opening(e);
}

void messaging_adapter::on_session_opening(event &e) {
    delegate_.on_session_opening(e);
}

void messaging_adapter::on_link_opening(event &e) {
    delegate_.on_link_opening(e);
}

void messaging_adapter::on_connection_error(event &e) {
    delegate_.on_connection_error(e);
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_connection_t *connection = pn_event_connection(pe->pn_event());
        pn_connection_close(connection);
    }
}

void messaging_adapter::on_session_error(event &e) {
    delegate_.on_session_error(e);
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_session_t *session = pn_event_session(pe->pn_event());
        pn_session_close(session);
    }
}

void messaging_adapter::on_link_error(event &e) {
    delegate_.on_link_error(e);
    proton_event *pe = dynamic_cast<proton_event*>(&e);
    if (pe) {
        pn_link_t *link = pn_event_link(pe->pn_event());
        pn_link_close(link);
    }
}

void messaging_adapter::on_connection_closed(event &e) {
    delegate_.on_connection_closed(e);
}

void messaging_adapter::on_session_closed(event &e) {
    delegate_.on_session_closed(e);
}

void messaging_adapter::on_link_closed(event &e) {
    delegate_.on_link_closed(e);
}

void messaging_adapter::on_connection_closing(event &e) {
    delegate_.on_connection_closing(e);
    if (peer_close_iserror_)
        on_connection_error(e);
}

void messaging_adapter::on_session_closing(event &e) {
    delegate_.on_session_closing(e);
    if (peer_close_iserror_)
        on_session_error(e);
}

void messaging_adapter::on_link_closing(event &e) {
    delegate_.on_link_closing(e);
    if (peer_close_iserror_)
        on_link_error(e);
}

void messaging_adapter::on_unhandled(event &e) {
}

}
