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
#include "types_internal.hpp"

#include "contexts.hpp"
#include "link_namer.hpp"
#include "proton_bits.hpp"
#include <proton/types.hpp>

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

class transaction_impl {
  public:
    proton::sender txn_ctrl;
    proton::messaging_handler *handler = nullptr;
    proton::binary transaction_id;
    bool failed = false;
    enum State {
      FREE,
      DECLARING,
      DECLARED,
      DISCHARGING,
    };
    enum State state = State::FREE;
    std::vector<proton::tracker> pending;

    void commit();
    void abort();
    void declare();

    void discharge(bool failed);
    void release_pending();

    proton::tracker send_ctrl(proton::symbol descriptor, proton::value _value);
    void handle_outcome(proton::tracker t);
    transaction_impl(proton::sender &_txn_ctrl,
                     proton::messaging_handler &_handler,
                     bool _settle_before_discharge);
    ~transaction_impl();
};


namespace {

bool transaction_is_empty(const session& s) { return session_context::get(unwrap(s))._txn_impl == NULL; }

void transaction_handle_outcome(const session& s, proton::tracker t) {
    session_context::get(unwrap(s))._txn_impl->handle_outcome(t);
}

void transaction_delete(const session& s) { auto &_txn_impl = session_context::get(unwrap(s))._txn_impl; delete _txn_impl; _txn_impl = nullptr;}

}

void session::transaction_declare(proton::messaging_handler &handler, bool settle_before_discharge) {
    auto &txn_impl = session_context::get(pn_object())._txn_impl;
    if (txn_impl == nullptr) {
        // Create _txn_impl
        proton::connection conn = this->connection();
        class InternalTransactionHandler : public proton::messaging_handler {

            void on_tracker_settle(proton::tracker &t) override {
                if (!transaction_is_empty(t.session())) {
                    transaction_handle_outcome(t.session(), t);
                }
            }
        };

        proton::target_options opts;
        std::vector<symbol> cap = {proton::symbol("amqp:local-transactions")};
        opts.capabilities(cap);
        opts.make_coordinator();

        proton::sender_options so;
        so.name("txn-ctrl");
        so.target(opts);

        static InternalTransactionHandler internal_handler; // internal_handler going out of scope. Fix it
        so.handler(internal_handler);

        static proton::sender s = conn.open_sender("does not matter", so);

        settle_before_discharge = false;

        txn_impl = new transaction_impl(s, handler, settle_before_discharge);
    }
    // Declare txn
    txn_impl->declare();
}


proton::binary session::transaction_id() const { return session_context::get(pn_object())._txn_impl->transaction_id; }
void session::transaction_commit() { session_context::get(pn_object())._txn_impl->commit(); }
void session::transaction_abort() { session_context::get(pn_object())._txn_impl->abort(); }
bool session::transaction_is_declared() { return (!transaction_is_empty(*this)) && session_context::get(pn_object())._txn_impl->state == transaction_impl::State::DECLARED; }

transaction_impl::transaction_impl(proton::sender &_txn_ctrl,
                                   proton::messaging_handler &_handler,
                                   bool _settle_before_discharge)
    : txn_ctrl(_txn_ctrl), handler(&_handler) {
}
transaction_impl::~transaction_impl() {}

void transaction_impl::commit() {
    discharge(false);
}

void transaction_impl::abort() {
    discharge(true);
}

void transaction_impl::declare() {
    if (state != transaction_impl::State::FREE)
        throw proton::error("This session has some associcated transaction already");
    state = State::DECLARING;

    proton::symbol descriptor("amqp:declare:list");
    std::list<proton::value> vd;
    proton::value i_am_null;
    vd.push_back(i_am_null);
    proton::value _value = vd;
    send_ctrl(descriptor, _value);
}

void transaction_impl::discharge(bool _failed) {
    if (state != transaction_impl::State::DECLARED)
        throw proton::error("Only a declared txn can be discharged.");
    state = State::DISCHARGING;

    failed = _failed;
    proton::symbol descriptor("amqp:discharge:list");
    std::list<proton::value> vd;
    vd.push_back(transaction_id);
    vd.push_back(failed);
    proton::value _value = vd;
    send_ctrl(descriptor, _value);
}

proton::tracker transaction_impl::send_ctrl(proton::symbol descriptor, proton::value _value) {
    proton::value msg_value;
    proton::codec::encoder enc(msg_value);
    enc << proton::codec::start::described()
        << descriptor
        << _value
        << proton::codec::finish();


    proton::message msg = msg_value;
    proton::tracker delivery = txn_ctrl.send(msg);
    return delivery;
}

void transaction_impl::release_pending() {
    for (auto d : pending) {
        delivery d2(make_wrapper<delivery>(unwrap(d)));
        d2.release();
    }
    pending.clear();
}

void transaction_impl::handle_outcome(proton::tracker t) {
    pn_disposition_t *disposition = pn_delivery_remote(unwrap(t));
    proton::session session = t.session();
    if (state == State::DECLARING) {
        // Attempting to declare transaction
        proton::value val(pn_disposition_data(disposition));
        auto vd = get<std::vector<proton::binary>>(val);
        if (vd.size() > 0) {
            transaction_id = vd[0];
            state = State::DECLARED;
            handler->on_session_open(session);
            return;
        } else if (pn_disposition_is_failed(disposition)) {
            state = State::FREE;
            transaction_delete(session);
            // on_transaction_declare_failed
            handler->on_session_error(session);
            return;
        } else {
            state = State::FREE;
            transaction_delete(session);
            handler->on_session_error(session);
            return;
        }
    } else if (state == State::DISCHARGING) {
        // Attempting to commit/abort transaction
        if (pn_disposition_is_failed(disposition)) {
            if (!failed) {
                state = State::FREE;
                transaction_delete(session);
                handler->on_session_transaction_commit_failed(session);
                release_pending();
                return;
            } else {
                state = State::FREE;
                transaction_delete(session);
                // Transaction abort failed.
                return;
            }
        } else {
            if (failed) {
                // Transaction abort is successful
                state = State::FREE;
                transaction_delete(session);
                handler->on_session_transaction_aborted(session);
                release_pending();
                return;
            } else {
                // Transaction commit is successful
                state = State::FREE;
                transaction_delete(session);
                handler->on_session_transaction_committed(session);
                return;
            }
        }
        pending.clear();
        return;
    }
    throw proton::error("reached unintended state in local transaction handler");
}

} // namespace proton
