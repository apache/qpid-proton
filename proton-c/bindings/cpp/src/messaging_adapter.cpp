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

#include "messaging_adapter.hpp"

#include "proton/delivery.hpp"
#include "proton/sender.hpp"
#include "proton/error.hpp"
#include "proton/tracker.hpp"
#include "proton/transport.hpp"

#include "contexts.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/handlers.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/transport.h>

namespace proton {

namespace {
void credit_topup(pn_link_t *link) {
    if (link && pn_link_is_receiver(link)) {
        int window = link_context::get(link).credit_window;
        if (window) {
            int delta = window - pn_link_credit(link);
            pn_link_flow(link, delta);
        }
    }
}
}

void messaging_adapter::on_reactor_init(proton_event &pe) {
    delegate_.on_container_start(pe.container());
}

void messaging_adapter::on_reactor_final(proton_event &pe) {
    delegate_.on_container_stop(pe.container());
}

void messaging_adapter::on_link_flow(proton_event &pe) {
    pn_event_t *pne = pe.pn_event();
    pn_link_t *lnk = pn_event_link(pne);
    // TODO: process session flow data, if no link-specific data, just return.
    if (!lnk) return;
    link_context& lctx = link_context::get(lnk);
    int state = pn_link_state(lnk);
    if ((state&PN_LOCAL_ACTIVE) && (state&PN_REMOTE_ACTIVE)) {
        if (pn_link_is_sender(lnk)) {
            if (pn_link_credit(lnk) > 0) {
                sender s(make_wrapper<sender>(lnk));
                if (pn_link_get_drain(lnk)) {
                    if (!lctx.draining) {
                        lctx.draining = true;
                        delegate_.on_sender_drain_start(s);
                    }
                }
                else {
                    lctx.draining = false;
                }
                // create on_message extended event
                delegate_.on_sendable(s);
            }
        }
        else {
            // receiver
            if (!pn_link_credit(lnk) && lctx.draining) {
                lctx.draining = false;
                receiver r(make_wrapper<receiver>(lnk));
                delegate_.on_receiver_drain_finish(r);
            }
        }
    }
    credit_topup(lnk);
}

void messaging_adapter::on_delivery(proton_event &pe) {
    pn_event_t *cevent = pe.pn_event();
    pn_link_t *lnk = pn_event_link(cevent);
    pn_delivery_t *dlv = pn_event_delivery(cevent);
    link_context& lctx = link_context::get(lnk);

    if (pn_link_is_receiver(lnk)) {
        delivery d(make_wrapper<delivery>(dlv));
        if (!pn_delivery_partial(dlv) && pn_delivery_readable(dlv)) {
            // generate on_message
            pn_connection_t *pnc = pn_session_connection(pn_link_session(lnk));
            connection_context& ctx = connection_context::get(pnc);
            // Reusable per-connection message.
            // Avoid expensive heap malloc/free overhead.
            // See PROTON-998
            class message &msg(ctx.event_message);
            msg.decode(d);
            if (pn_link_state(lnk) & PN_LOCAL_CLOSED) {
                if (lctx.auto_accept)
                    d.release();
            } else {
                delegate_.on_message(d, msg);
                if (lctx.auto_accept && !d.settled())
                    d.accept();
                if (lctx.draining && !pn_link_credit(lnk)) {
                    lctx.draining = false;
                    receiver r(make_wrapper<receiver>(lnk));
                    delegate_.on_receiver_drain_finish(r);
                }
            }
        }
        else if (pn_delivery_updated(dlv) && d.settled()) {
            delegate_.on_delivery_settle(d);
        }
        if (lctx.draining && pn_link_credit(lnk) == 0) {
            lctx.draining = false;
            pn_link_set_drain(lnk, false);
            receiver r(make_wrapper<receiver>(lnk));
            delegate_.on_receiver_drain_finish(r);
            if (lctx.pending_credit) {
                pn_link_flow(lnk, lctx.pending_credit);
                lctx.pending_credit = 0;
            }
        }
        credit_topup(lnk);
    } else {
        tracker t(make_wrapper<tracker>(dlv));
        // sender
        if (pn_delivery_updated(dlv)) {
            uint64_t rstate = pn_delivery_remote_state(dlv);
            if (rstate == PN_ACCEPTED) {
                delegate_.on_tracker_accept(t);
            }
            else if (rstate == PN_REJECTED) {
                delegate_.on_tracker_reject(t);
            }
            else if (rstate == PN_RELEASED || rstate == PN_MODIFIED) {
                delegate_.on_tracker_release(t);
            }

            if (t.settled()) {
                delegate_.on_tracker_settle(t);
            }
            if (lctx.auto_settle)
                t.settle();
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

bool is_remote_unititialised(pn_state_t state) {
    return state & PN_REMOTE_UNINIT;
}

} // namespace

void messaging_adapter::on_link_remote_detach(proton_event & pe) {
    pn_event_t *cevent = pe.pn_event();
    pn_link_t *lnk = pn_event_link(cevent);
    if (pn_link_is_receiver(lnk)) {
        receiver r(make_wrapper<receiver>(lnk));
        delegate_.on_receiver_detach(r);
    } else {
        sender s(make_wrapper<sender>(lnk));
        delegate_.on_sender_detach(s);
    }
    pn_link_detach(lnk);
}

void messaging_adapter::on_link_remote_close(proton_event &pe) {
    pn_event_t *cevent = pe.pn_event();
    pn_link_t *lnk = pn_event_link(cevent);
    if (pn_link_is_receiver(lnk)) {
        receiver r(make_wrapper<receiver>(lnk));
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            delegate_.on_receiver_error(r);
        }
        delegate_.on_receiver_close(r);
    } else {
        sender s(make_wrapper<sender>(lnk));
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            delegate_.on_sender_error(s);
        }
        delegate_.on_sender_close(s);
    }
    pn_link_close(lnk);
}

void messaging_adapter::on_session_remote_close(proton_event &pe) {
    pn_event_t *cevent = pe.pn_event();
    pn_session_t *session = pn_event_session(cevent);
    class session s(make_wrapper(session));
    if (pn_condition_is_set(pn_session_remote_condition(session))) {
        delegate_.on_session_error(s);
    }
    delegate_.on_session_close(s);
    pn_session_close(session);
}

void messaging_adapter::on_connection_remote_close(proton_event &pe) {
    pn_event_t *cevent = pe.pn_event();
    pn_connection_t *conn = pn_event_connection(cevent);
    connection c(make_wrapper(conn));
    if (pn_condition_is_set(pn_connection_remote_condition(conn))) {
        delegate_.on_connection_error(c);
    }
    delegate_.on_connection_close(c);
    pn_connection_close(conn);
}

void messaging_adapter::on_connection_remote_open(proton_event &pe) {
    // Generate on_transport_open event here until we find a better place
    transport t(make_wrapper(pn_event_transport(pe.pn_event())));
    delegate_.on_transport_open(t);

    pn_connection_t *conn = pn_event_connection(pe.pn_event());
    connection c(make_wrapper(conn));
    delegate_.on_connection_open(c);
    if (!is_local_open(pn_connection_state(conn)) && is_local_unititialised(pn_connection_state(conn))) {
        pn_connection_open(conn);
    }
}

void messaging_adapter::on_session_remote_open(proton_event &pe) {
    pn_session_t *session = pn_event_session(pe.pn_event());
    class session s(make_wrapper(session));
    delegate_.on_session_open(s);
    if (!is_local_open(pn_session_state(session)) && is_local_unititialised(pn_session_state(session))) {
        pn_session_open(session);
    }
}

void messaging_adapter::on_link_local_open(proton_event &pe) {
    credit_topup(pn_event_link(pe.pn_event()));
}

void messaging_adapter::on_link_remote_open(proton_event &pe) {
    pn_link_t *lnk = pn_event_link(pe.pn_event());
    container& c = pe.container();
    if (pn_link_is_receiver(lnk)) {
      receiver r(make_wrapper<receiver>(lnk));
      delegate_.on_receiver_open(r);
      if (is_local_unititialised(pn_link_state(lnk))) {
          r.open(c.receiver_options());
      }
    } else {
      sender s(make_wrapper<sender>(lnk));
      delegate_.on_sender_open(s);
      if (is_local_unititialised(pn_link_state(lnk))) {
          s.open(c.sender_options());
      }
    }
    credit_topup(lnk);
}

void messaging_adapter::on_transport_closed(proton_event &pe) {
    pn_transport_t *tspt = pn_event_transport(pe.pn_event());
    transport t(make_wrapper(tspt));

    // If the connection isn't open generate on_transport_open event
    // because we didn't generate it yet and the events won't match.
    pn_connection_t *conn = pn_event_connection(pe.pn_event());
    if (!conn || is_remote_unititialised(pn_connection_state(conn))) {
        delegate_.on_transport_open(t);
    }

    if (pn_condition_is_set(pn_transport_condition(tspt))) {
        delegate_.on_transport_error(t);
    }
    delegate_.on_transport_close(t);
}

}
