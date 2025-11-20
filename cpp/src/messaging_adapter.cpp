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

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/delivery.hpp"
#include "proton/error.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/receiver.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/target_options.hpp"
#include "proton/session.hpp"
#include "proton/tracker.hpp"
#include "proton/transport.hpp"

#include "contexts.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "types_internal.hpp"

#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/disposition.h>
#include <proton/handlers.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/transport.h>

#include "tracing_private.hpp"

#include <assert.h>

namespace proton {

namespace {
// This must only be called for receiver links
void credit_topup(pn_link_t *link) {
    assert(pn_link_is_receiver(link));
    int window = link_context::get(link).credit_window;
    if (window) {
        int delta = window - pn_link_credit(link);
        pn_link_flow(link, delta);
    }
}


void on_link_flow(messaging_handler& handler, pn_event_t* event) {
    pn_link_t *lnk = pn_event_link(event);
    // TODO: process session flow data, if no link-specific data, just return.
    if (!lnk) return;
    int state = pn_link_state(lnk);
    if (pn_terminus_get_type(pn_link_remote_target(lnk)) == PN_COORDINATOR ||
        ((state & PN_LOCAL_ACTIVE) && (state & PN_REMOTE_ACTIVE))) {
        link_context& lctx = link_context::get(lnk);
        if (pn_link_is_sender(lnk)) {
            if (pn_link_credit(lnk) > 0) {
                sender s(make_wrapper<sender>(lnk));
                bool draining = pn_link_get_drain(lnk);
                if ( draining && !lctx.draining) {
                    handler.on_sender_drain_start(s);
                }
                lctx.draining = draining;
                // create on_message extended event
                handler.on_sendable(s);
            }
        } else {
            // receiver
            if (!pn_link_credit(lnk) && lctx.draining) {
                lctx.draining = false;
                pn_link_set_drain(lnk, false);
                receiver r(make_wrapper<receiver>(lnk));
                handler.on_receiver_drain_finish(r);
            }
            credit_topup(lnk);
        }
    }
}

// Decode the message corresponding to a delivery from a link.
void message_decode(message& msg, proton::delivery delivery) {
    std::vector<char> buf;
    buf.resize(pn_delivery_pending(unwrap(delivery)));
    if (buf.empty())
        throw error("message decode: no delivery pending on link");
    proton::receiver link = delivery.receiver();
    assert(!buf.empty());
    ssize_t n = pn_link_recv(unwrap(link), const_cast<char *>(&buf[0]), buf.size());
    if (n != ssize_t(buf.size())) throw error(MSG("receiver read failure"));
    msg.clear();
    msg.decode(buf);
    pn_link_advance(unwrap(link));
}

bool transaction_coordinator_sender(const sender& s) {
    auto& txn_context = session_context::get(unwrap(s.session())).transaction_context_;
    return txn_context && (txn_context->coordinator == unwrap(s));
}

void handle_transaction_coordinator_outcome(messaging_handler& handler, tracker t) {
    auto session = t.session();
    auto& session_context = session_context::get(unwrap(session));
    auto& transaction_context = session_context.transaction_context_;
    auto state = transaction_context->state;
    auto disposition = pn_delivery_remote(unwrap(t));
    if (auto *declared_disp = pn_declared_disposition(disposition); declared_disp) {
        switch (state) {
          case transaction_context::State::DECLARING: {
            pn_bytes_t txn_id = pn_declared_disposition_get_id(declared_disp);
            transaction_context->transaction_id = proton::bin(txn_id);
            transaction_context->state = transaction_context::State::DECLARED;
            handler.on_session_transaction_declared(session);
            return;
          }
          case transaction_context::State::NO_TRANSACTION:
          case transaction_context::State::DECLARED:
          case transaction_context::State::DISCHARGING:
            // Don't throw error here, instead close link with error
            make_wrapper(transaction_context->coordinator).close(error_condition{"amqp:not-allowed", "Received transaction declared disposition in invalid state"});
            transaction_context.release();
        }
    } else if (pn_disposition_type(disposition) == PN_ACCEPTED) {
        switch (state) {
          case transaction_context::State::DISCHARGING: {
            if (transaction_context->failed) {
                // Transaction abort is successful
                transaction_context->state = transaction_context::State::NO_TRANSACTION;
                handler.on_session_transaction_aborted(session);
                return;
            } else {
                // Transaction commit is successful
                transaction_context->state = transaction_context::State::NO_TRANSACTION;
                handler.on_session_transaction_committed(session);
                return;
            }
          }
          case transaction_context::State::NO_TRANSACTION:
          case transaction_context::State::DECLARING:
          case transaction_context::State::DECLARED:
            // TODO: Don't throw error here, instead detach link or close session?
            make_wrapper(transaction_context->coordinator).close(error_condition{"amqp:not-allowed", "Received transaction accepted disposition in invalid state"});
            transaction_context.release();
        }
    } else if (auto rejected_disp = pn_rejected_disposition(disposition); rejected_disp) {
        switch (state) {
          case transaction_context::State::DECLARING:
            transaction_context->state = transaction_context::State::NO_TRANSACTION;
            transaction_context->error = pn_rejected_disposition_condition(rejected_disp);
            handler.on_session_transaction_error(session);
            return;
          case transaction_context::State::DISCHARGING:
            // Note that rollback cannot fail in AMQP as the outcome would be the same,
            // so don't count rejected discharge as an error (although it is a protocol error).
            if (!transaction_context->failed) {
                transaction_context->state = transaction_context::State::NO_TRANSACTION;
                transaction_context->error = pn_rejected_disposition_condition(rejected_disp);
                handler.on_session_transaction_error(session);
                return;
            }
          case transaction_context::State::NO_TRANSACTION:
          case transaction_context::State::DECLARED:
            // TODO: Don't throw error here, instead detach link or close session?
            make_wrapper(transaction_context->coordinator).close(error_condition{"amqp:not-allowed", "Received transaction rejected disposition in invalid state"});
            transaction_context.release();
        }
    }
    // TODO: Don't ignore unexpected disposition here, instead detach link or close session?
}

void on_delivery(messaging_handler& handler, pn_event_t* event) {
    pn_link_t *lnk = pn_event_link(event);
    pn_delivery_t *dlv = pn_event_delivery(event);
    link_context& lctx = link_context::get(lnk);
    Tracing& ot = Tracing::getTracing();

    if (pn_link_is_receiver(lnk)) {
        delivery d(make_wrapper<delivery>(dlv));
        if (pn_delivery_aborted(dlv)) {
            pn_delivery_settle(dlv);
            pn_link_flow(lnk, 1);
            pn_link_advance(lnk);
        }
        else if (!pn_delivery_partial(dlv) && pn_delivery_readable(dlv)) {
            // generate on_message
            pn_connection_t *pnc = pn_session_connection(pn_link_session(lnk));
            connection_context& ctx = connection_context::get(pnc);
            // Reusable per-connection message.
            // Avoid expensive heap malloc/free overhead.
            // See PROTON-998
            class message &msg(ctx.event_message);
            message_decode(msg, d);
            if (pn_link_state(lnk) & PN_LOCAL_CLOSED) {
                if (lctx.auto_accept)
                    d.release();
            } else {
                ot.on_message_handler(handler, d, msg);
                if (lctx.auto_accept && pn_delivery_local_state(dlv) == 0) // Not set by handler
                    d.accept();
                if (lctx.draining && !pn_link_credit(lnk)) {
                    lctx.draining = false;
                    pn_link_set_drain(lnk, false);
                    receiver r(make_wrapper<receiver>(lnk));
                    handler.on_receiver_drain_finish(r);
                }
            }
        }
        else if (pn_delivery_updated(dlv) && d.settled()) {
            handler.on_delivery_settle(d);
        }
        if (lctx.draining && pn_link_credit(lnk) == 0) {
            lctx.draining = false;
            pn_link_set_drain(lnk, false);
            receiver r(make_wrapper<receiver>(lnk));
            handler.on_receiver_drain_finish(r);
            if (lctx.pending_credit) {
                pn_link_flow(lnk, lctx.pending_credit);
                lctx.pending_credit = 0;
            }
        }
        credit_topup(lnk);
    } else {
        // sender
        if (pn_delivery_updated(dlv)) {
            tracker t(make_wrapper<tracker>(dlv));
            // Check for outcome from a transaction coordinator
            if (transaction_coordinator_sender(t.sender())) {
                handle_transaction_coordinator_outcome(handler, t);
                t.settle();
                return;
            }
            ot.on_settled_span(t);
            switch(pn_delivery_remote_state(dlv)) {
            case PN_ACCEPTED:
                handler.on_tracker_accept(t);
                break;
            case PN_REJECTED:
                handler.on_tracker_reject(t);
                break;
            case PN_RELEASED:
            case PN_MODIFIED:
                handler.on_tracker_release(t);
                break;
            }
            if (t.settled()) {
                handler.on_tracker_settle(t);
                if (lctx.auto_settle)
                    t.settle();
            }
        }
    }
}

bool is_remote_uninitialized(pn_state_t state) {
    return state & PN_REMOTE_UNINIT;
}

void on_link_remote_detach(messaging_handler& handler, pn_event_t* event) {
    pn_link_t *lnk = pn_event_link(event);
    if (pn_link_is_receiver(lnk)) {
        receiver r(make_wrapper<receiver>(lnk));
        handler.on_receiver_detach(r);
    } else {
        sender s(make_wrapper<sender>(lnk));
        handler.on_sender_detach(s);
    }
    pn_link_detach(lnk);
}

void on_link_remote_close(messaging_handler& handler, pn_event_t* event) {
    pn_link_t *lnk = pn_event_link(event);
    if (pn_link_is_receiver(lnk)) {
        receiver r(make_wrapper<receiver>(lnk));
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            handler.on_receiver_error(r);
        }
        handler.on_receiver_close(r);
    } else {
        sender s(make_wrapper<sender>(lnk));
        if (pn_condition_is_set(pn_link_remote_condition(lnk))) {
            handler.on_sender_error(s);
        }
        handler.on_sender_close(s);
    }
    pn_link_close(lnk);
}

void on_session_remote_close(messaging_handler& handler, pn_event_t* event) {
    pn_session_t *session = pn_event_session(event);
    class session s(make_wrapper(session));
    if (pn_condition_is_set(pn_session_remote_condition(session))) {
        handler.on_session_error(s);
    }
    handler.on_session_close(s);
    pn_session_close(session);
}

void on_connection_remote_close(messaging_handler& handler, pn_event_t* event) {
    pn_connection_t *conn = pn_event_connection(event);
    connection c(make_wrapper(conn));
    if (pn_condition_is_set(pn_connection_remote_condition(conn))) {
        handler.on_connection_error(c);
    }
    handler.on_connection_close(c);
    pn_connection_close(conn);
}

void on_connection_bound(messaging_handler& handler, pn_event_t* event) {
    connection c(make_wrapper(pn_event_connection(event)));
}

void on_connection_remote_open(messaging_handler& handler, pn_event_t* event) {
    // Generate on_transport_open event here until we find a better place
    transport t(make_wrapper(pn_event_transport(event)));
    handler.on_transport_open(t);

    pn_connection_t *conn = pn_event_connection(event);
    connection c(make_wrapper(conn));
    handler.on_connection_open(c);
}

void on_session_remote_open(messaging_handler& handler, pn_event_t* event) {
    pn_session_t *session = pn_event_session(event);
    class session s(make_wrapper(session));
    handler.on_session_open(s);
}

void on_link_local_open(messaging_handler& handler, pn_event_t* event) {
    pn_link_t* lnk = pn_event_link(event);
    if ( pn_link_is_receiver(lnk) ) {
        credit_topup(lnk);
    // We know local is active so don't check for it
    } else if ( pn_link_state(lnk)&PN_REMOTE_ACTIVE && pn_link_credit(lnk) > 0) {
        sender s(make_wrapper<sender>(lnk));
        handler.on_sendable(s);
    }
}

void on_link_remote_open(messaging_handler& handler, pn_event_t* event) {
    auto lnk = pn_event_link(event);
    if (pn_link_state(lnk) & PN_LOCAL_UNINIT) { // Incoming link
        // Copy source and target from remote end.
        pn_terminus_copy(pn_link_source(lnk), pn_link_remote_source(lnk));
        pn_terminus_copy(pn_link_target(lnk), pn_link_remote_target(lnk));
    }
    if (pn_link_is_receiver(lnk)) {
      receiver r(make_wrapper<receiver>(lnk));
      handler.on_receiver_open(r);
      credit_topup(lnk);
    } else {
      sender s(make_wrapper<sender>(lnk));
      handler.on_sender_open(s);
    }
}

void on_transport_closed(messaging_handler& handler, pn_event_t* event) {
    pn_transport_t *tspt = pn_event_transport(event);
    transport t(make_wrapper(tspt));

    // If the connection isn't open generate on_transport_open event
    // because we didn't generate it yet and the events won't match.
    pn_connection_t *conn = pn_event_connection(event);
    if (!conn || is_remote_uninitialized(pn_connection_state(conn))) {
        handler.on_transport_open(t);
    }

    if (pn_condition_is_set(pn_transport_condition(tspt))) {
        handler.on_transport_error(t);
    }
    handler.on_transport_close(t);
}

void on_connection_wake(messaging_handler& handler, pn_event_t* event) {
    connection c(make_wrapper(pn_event_connection(event)));
    handler.on_connection_wake(c);
}

}

void messaging_adapter::dispatch(messaging_handler& h, pn_event_t* event)
{
    pn_event_type_t type = pn_event_type(event);

    // If this is an event for an (internal) transaction coordinator link set the handler to a null handler
    // Unless its a delivery event which we need to process for transaction outcomes
    messaging_handler& handler = [&]() -> messaging_handler& {
        if (pn_link_t *lnk = pn_event_link(event);
            type != PN_DELIVERY &&
            lnk && pn_link_is_sender(lnk) &&
            transaction_coordinator_sender(sender(make_wrapper<sender>(lnk)))) {
            static messaging_handler null_handler;
            return null_handler;
        } else {
            return h;
        }
    }();

    // Only handle events we are interested in
    switch(type) {
      case PN_CONNECTION_BOUND: on_connection_bound(handler, event); break;
      case PN_CONNECTION_REMOTE_OPEN: on_connection_remote_open(handler, event); break;
      case PN_CONNECTION_REMOTE_CLOSE: on_connection_remote_close(handler, event); break;

      case PN_SESSION_REMOTE_OPEN: on_session_remote_open(handler, event); break;
      case PN_SESSION_REMOTE_CLOSE: on_session_remote_close(handler, event); break;

      case PN_LINK_LOCAL_OPEN: on_link_local_open(handler, event); break;
      case PN_LINK_REMOTE_OPEN: on_link_remote_open(handler, event); break;
      case PN_LINK_REMOTE_CLOSE: on_link_remote_close(handler, event); break;
      case PN_LINK_REMOTE_DETACH: on_link_remote_detach(handler, event); break;
      case PN_LINK_FLOW: on_link_flow(handler, event); break;

      case PN_DELIVERY: on_delivery(handler, event); break;

      case PN_TRANSPORT_CLOSED: on_transport_closed(handler, event); break;

      case PN_CONNECTION_WAKE: on_connection_wake(handler, event); break;

      // Ignore everything else
      default: break;
    }
}

}
