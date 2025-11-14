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
#include "proton/session.hpp"

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/delivery.h"
#include "proton/delivery.hpp"
#include "proton/error.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender_options.hpp"
#include "proton/session_options.hpp"
#include "proton/target_options.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/tracker.hpp"
#include "proton/transfer.hpp"
#include <proton/types.hpp>

#include "contexts.hpp"
#include "link_namer.hpp"
#include "proactor_container_impl.hpp"
#include "proton_bits.hpp"
#include "types_internal.hpp"

#include <proton/connection.h>
#include <proton/session.h>

#include <string>

namespace proton {

session::~session() = default;

void session::open() {
    pn_session_open(pn_object());
}

void session::open(const session_options &opts) {
    opts.apply(*this);
    pn_session_open(pn_object());
}

void session::close()
{
    pn_session_close(pn_object());
}

container& session::container() const {
    return connection().container();
}

work_queue& session::work_queue() const {
    return connection().work_queue();
}

connection session::connection() const {
    return make_wrapper(pn_session_connection(pn_object()));
}

namespace {
std::string next_link_name(const connection& c) {
    io::link_namer* ln = connection_context::get(unwrap(c)).link_gen;

    return ln ? ln->link_name() : uuid::random().str();
}

}

sender session::open_sender(const std::string &addr) {
    return open_sender(addr, sender_options());
}

sender session::open_sender(const std::string &addr, const sender_options &so) {
    std::string name = so.get_name() ? *so.get_name() : next_link_name(connection());
    pn_link_t *lnk = pn_sender(pn_object(), name.c_str());
    pn_terminus_set_address(pn_link_target(lnk), addr.c_str());
    sender snd(make_wrapper<sender>(lnk));
    snd.open(so);
    return snd;
}

receiver session::open_receiver(const std::string &addr) {
    return open_receiver(addr, receiver_options());
}

receiver session::open_receiver(const std::string &addr, const receiver_options &ro)
{
    std::string name = ro.get_name() ? *ro.get_name() : next_link_name(connection());
    pn_link_t *lnk = pn_receiver(pn_object(), name.c_str());
    pn_terminus_set_address(pn_link_source(lnk), addr.c_str());
    receiver rcv(make_wrapper<receiver>(lnk));
    rcv.open(ro);
    return rcv;
}

error_condition session::error() const {
    return make_wrapper(pn_session_remote_condition(pn_object()));
}

size_t session::incoming_bytes() const {
    return pn_session_incoming_bytes(pn_object());
}

size_t session::outgoing_bytes() const {
    return pn_session_outgoing_bytes(pn_object());
}

sender_range session::senders() const {
    pn_link_t *lnk = pn_link_head(pn_session_connection(pn_object()), 0);
    while (lnk) {
        if (pn_link_is_sender(lnk) && pn_link_session(lnk) == pn_object())
            break;
        lnk = pn_link_next(lnk, 0);
    }
    return sender_range(sender_iterator(make_wrapper<sender>(lnk), pn_object()));
}

receiver_range session::receivers() const {
    pn_link_t *lnk = pn_link_head(pn_session_connection(pn_object()), 0);
    while (lnk) {
        if (pn_link_is_receiver(lnk) && pn_link_session(lnk) == pn_object())
            break;
        lnk = pn_link_next(lnk, 0);
    }
    return receiver_range(receiver_iterator(make_wrapper<receiver>(lnk), pn_object()));
}

session_iterator session_iterator::operator++() {
    obj_ = pn_session_next(unwrap(obj_), 0);
    return *this;
}

void session::user_data(void* user_data) const {
    pn_session_t* ssn = pn_object();
    session_context& sctx = session_context::get(ssn);
    sctx.user_data_ = user_data;
}

void* session::user_data() const {
    pn_session_t* ssn = pn_object();
    session_context& sctx = session_context::get(ssn);
    return sctx.user_data_;
}

namespace {

void transaction_discharge(const session& s, bool failed);

proton::tracker transaction_send_ctrl(sender& coordinator, const symbol& descriptor, const value& value);
void transaction_handle_outcome(const session& s, proton::tracker t);

std::unique_ptr<transaction_context>& get_transaction_context(const session& s) {
    return session_context::get(unwrap(s)).transaction_context_;
}
bool transaction_is_empty(const session& s) { return get_transaction_context(s) == nullptr; }

void transaction_delete(const session& s) { get_transaction_context(s).release(); }

}

void session::transaction_declare(bool settle_before_discharge) {
    auto& txn_context = session_context::get(pn_object()).transaction_context_;
    if (!txn_context) {
        // Create _txn_impl
        class InternalTransactionHandler : public messaging_handler {
            void on_tracker_settle(proton::tracker &t) override {
                if (!transaction_is_empty(t.session())) {
                    transaction_handle_outcome(t.session(), t);
                }
            }
        };
        auto internal_handler = std::make_unique<InternalTransactionHandler>();
        sender_options so;
        so.name("txn-ctrl")
            .handler(*internal_handler)
            .target(
                target_options{}
                    .capabilities(std::vector<symbol>{"amqp:local-transactions"})
                    .make_coordinator());

        auto s = connection().open_sender("txn coordinator", so);
        txn_context = std::make_unique<transaction_context>(s, std::move(internal_handler), settle_before_discharge);
    }

    // Declare txn
    if (txn_context->state != transaction_context::State::FREE)
        throw proton::error("This session has some associcated transaction already");
    txn_context->state = transaction_context::State::DECLARING;

    transaction_send_ctrl(txn_context->coordinator, "amqp:declare:list", std::list<proton::value>{});
}


binary session::transaction_id() const { 
    auto& txn_context = get_transaction_context(*this);
    if (txn_context) {
        return txn_context->transaction_id;
    } else {
        return binary();
    }
}
void session::transaction_commit() { transaction_discharge(*this, false); }
void session::transaction_abort() { transaction_discharge(*this, true); }
bool session::transaction_is_declared() const { return (!transaction_is_empty(*this)) && get_transaction_context(*this)->state == transaction_context::State::DECLARED; }
error_condition session::transaction_error() const {
    auto& txn_context = get_transaction_context(*this);
    if (txn_context) {
        return make_wrapper(txn_context->error);
    } else {
        return error_condition();
    }
}

messaging_handler* get_handler(const session& s) {
    pn_session_t *session = unwrap(s);
    messaging_handler *mh = internal::get_messaging_handler(session);

    // Try for connection handler if none of the above
    pn_connection_t *connection = pn_session_connection(session);
    if (connection && !mh) mh = internal::get_messaging_handler(connection);

    // Use container handler if nothing more specific (must be a container handler)
    return mh ? mh : connection_context::get(connection).container->impl_->handler_;
}

namespace {

void transaction_discharge(const session& s, bool failed) {
    auto& transaction_context = get_transaction_context(s);
    if (transaction_context->state != transaction_context::State::DECLARED)
        throw proton::error("Only a declared txn can be discharged.");
    transaction_context->state = transaction_context::State::DISCHARGING;

    transaction_context->failed = failed;
    transaction_send_ctrl(
        transaction_context->coordinator,
        "amqp:discharge:list", std::list<proton::value>{transaction_context->transaction_id, failed});
}

proton::tracker transaction_send_ctrl(sender& coordinator, const symbol& descriptor, const value& value) {
    proton::value msg_value;
    proton::codec::encoder enc(msg_value);
    enc << proton::codec::start::described()
        << descriptor
        << value
        << proton::codec::finish();

    return coordinator.send(msg_value);
}

void transaction_handle_outcome(const session&, proton::tracker t) {
    proton::session session = t.session();
    auto& session_context = session_context::get(unwrap(session));
    auto& transaction_context = session_context.transaction_context_;
    auto handler = get_handler(session);
    auto disposition = pn_delivery_remote(unwrap(t));
    if (auto *declared_disp = pn_declared_disposition(disposition); declared_disp) {
        switch (transaction_context->state) {
          case transaction_context::State::DECLARING: {
            pn_bytes_t txn_id = pn_declared_disposition_get_id(declared_disp);
            transaction_context->transaction_id = proton::bin(txn_id);
            transaction_context->state = transaction_context::State::DECLARED;
            handler->on_session_transaction_declared(session);
            return;
          }
          case transaction_context::State::DISCHARGING: {
            if (transaction_context->failed) {
                // Transaction abort is successful
                handler->on_session_transaction_aborted(session);
                transaction_delete(session);
                return;
            } else {
                // Transaction commit is successful
                handler->on_session_transaction_committed(session);
                transaction_delete(session);
                return;
            }
          }
          default:
            ;
        }
    } else if (auto rejected_disp = pn_rejected_disposition(disposition); rejected_disp) {
        switch (transaction_context->state) {
          case transaction_context::State::DECLARING:
          case transaction_context::State::DISCHARGING:
            // Currently same handling for both declare and commit failures
            // Note that rollback cannot fail in AMQP as the outcome would be the same
            transaction_context->error = pn_rejected_disposition_condition(rejected_disp);
            handler->on_session_transaction_error(session);
            transaction_delete(session);
            return;
          default:
            ;
        }
    }
    // TODO: Don't throw error here, instead detach link or close session?
    throw proton::error("reached unintended state in local transaction handler");
}

}

} // namespace proton
