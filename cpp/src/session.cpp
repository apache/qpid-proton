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
#include "proton/transaction.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/tracker.hpp"
#include "proton/transfer.hpp"

#include "contexts.hpp"
#include "link_namer.hpp"
#include "proton_bits.hpp"
#include <proton/types.hpp>

#include <proton/connection.h>
#include <proton/session.h>

#include <string>

// XXXX: Debug
#include <iostream>

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

// session_context& session::get_session_context() {
//     return session_context::get(pn_object());
// }

// TODO : WE ARE NOT RETURNING TRANSACTION FOR NOW.
void session::declare_transaction(proton::transaction_handler &handler, bool settle_before_discharge) {
    if (session_context::get(pn_object())._txn_impl != nullptr)
        throw proton::error("Transaction is already declared for this session");
    proton::connection conn = this->connection();
    class InternalTransactionHandler : public proton::messaging_handler {
        // TODO: auto_settle

        void on_tracker_settle(proton::tracker &t) override {
            std::cout<<"    [InternalTransactionHandler][on_tracker_settle] called with tracker.txn"
                 << std::endl;
            if (!t.session().txn_is_empty()) {
                 std::cout<<"    [InternalTransactionHandler] Inside if condition" << std::endl;
                t.session().txn_handle_outcome(t);
            }
        }
    };

    proton::target_options t;
    std::vector<symbol> cap = {proton::symbol("amqp:local-transactions")};
    t.capabilities(cap);
    t.type(PN_COORDINATOR);

    proton::sender_options so;
    so.name("txn-ctrl");
    so.target(t);
    static InternalTransactionHandler internal_handler; // internal_handler going out of scope. Fix it
    so.handler(internal_handler);
    std::cout<<"    [declare_transaction] txn-name sender open with handler: " << &internal_handler << std::endl;

    static proton::sender s = conn.open_sender("does not matter", so);

    settle_before_discharge = false;

    std::cout<<"    [declare_transaction] calling mk_transaction_impl" << std::endl;

// get the _txn_impl from session_context.
    session_context::get(pn_object())._txn_impl.reset(new transaction_impl(s, handler, settle_before_discharge));
    // std::cout << "Session object UPdated: " << _txn_impl << "For session at: " << this << std::endl;
    // std::cout<<"    [declare_transaction] txn address:" << (void*)_txn_impl << std::endl;

    // _txn = txn;
    // return _txn_impl;
}

// transaction::transaction() : _impl(NULL) {}  // empty transaction, not yet ready
// transaction::transaction(proton::sender& _txn_ctrl,
// proton::transaction_handler& _handler, bool _settle_before_discharge) :
//     _impl(std::make_shared<transaction_impl>(_txn_ctrl, _handler,
//     _settle_before_discharge)) {}
// transaction::transaction(transaction_impl *impl)
    // : _impl(impl) {}
// transaction::transaction( transaction_impl* impl): _impl(impl){}
// transaction::~transaction() = default;
void session::txn_commit() {session_context::get(pn_object())._txn_impl->commit(); }
void session::txn_abort() { session_context::get(pn_object())._txn_impl->abort(); }
void session::txn_declare() { session_context::get(pn_object())._txn_impl->declare(); }
bool session::txn_is_empty() { return session_context::get(pn_object())._txn_impl == NULL; }
void session::txn_accept(delivery &t) { return session_context::get(pn_object())._txn_impl->accept(t); }
proton::tracker session::txn_send(proton::sender s, proton::message msg) {
    return session_context::get(pn_object())._txn_impl->send(s, msg);
}
void session::txn_handle_outcome(proton::tracker t) {
    std::cout << "    transaction::handle_outcome = NO OP base class "
              << std::endl;
    session_context::get(pn_object())._txn_impl->handle_outcome(t);
}

// transaction session::mk_transaction_impl(sender &s, transaction_handler &h,
//                                              bool f) {
//     return transaction(new transaction_impl(s, h, f));
// }

// proton::connection session::connection() const {
//     return _txn_impl->txn_ctrl.connection();
// }

transaction_impl::transaction_impl(proton::sender &_txn_ctrl,
                                   proton::transaction_handler &_handler,
                                   bool _settle_before_discharge)
    : txn_ctrl(_txn_ctrl), handler(&_handler) {
    // bool settle_before_discharge = _settle_before_discharge;
    declare();
}

void transaction_impl::commit() {
    discharge(false);
}

void transaction_impl::abort() {
    discharge(true);
}

void transaction_impl::declare() {
    std::cout<<"    [transaction_impl][declare] staring it" << std::endl;

    proton::symbol descriptor("amqp:declare:list");
    // proton::value _value = vd;
    // TODO: How to make list;
    std::list<proton::value> vd;
    proton::value i_am_null;
    vd.push_back(i_am_null);
    proton::value _value = vd;
    std::cout<<"   [transaction_impl::declare()] value to send_ctrl: " << _value<< std::endl;
    _declare = send_ctrl(descriptor, _value );
    std::cout << "  transaction_impl::declare()... txn_impl i am is " << this
              << std::endl;
    std::cout << "   [transaction_impl::declare()] _declare is : " << _declare
              << std::endl;
}

void transaction_impl::discharge(bool _failed) {
    failed = _failed;
    proton::symbol descriptor("amqp:discharge:list");
    std::list<proton::value> vd;
    vd.push_back(id);
    vd.push_back(failed);
    proton::value _value = vd;
    _discharge = send_ctrl(descriptor, _value);
}

void transaction_impl::set_id(binary _id) {
    std::cout << "    TXN ID: " << _id << " from " << this << std::endl;
    id = _id;
}

proton::tracker transaction_impl::send_ctrl(proton::symbol descriptor, proton::value _value) {
    proton::value msg_value;
    proton::codec::encoder enc(msg_value);
    enc << proton::codec::start::described()
        << descriptor
        << _value
        << proton::codec::finish();


    proton::message msg = msg_value;
    std::cout << "    [transaction_impl::send_ctrl] sending " << msg << std::endl;
    proton::tracker delivery = txn_ctrl.send(msg);
    std::cout << "    # declare, delivery as tracker: " << delivery
              << std::endl;
    // TODO: Ensure we can access session from delivery.
    // delivery.transaction(transaction(this));
    std::cout
        << "    [transaction_impl::send_ctrl] sending done. I guess queued! "
        << delivery << std::endl;
    return delivery;
}

proton::tracker transaction_impl::send(proton::sender s, proton::message msg) {
    proton::tracker tracker = s.send(msg);
    std::cout << "    transaction_impl::send " << id << ", done: " << msg
              << " tracker: " << tracker << std::endl;
    update(tracker, 0x34);
    std::cout << "     transaction_impl::send, update" << std::endl;
    return tracker;
}

void transaction_impl::accept(delivery &t) {
    // TODO: settle-before-discharge
    t.settle();
    // pending.push_back(d);
}

// TODO: use enum transfer::state
void transaction_impl::update(tracker &t, uint64_t state) {
    if (state) {
        proton::value data(pn_disposition_data(pn_delivery_local(unwrap(t))));
        std::list<proton::value> data_to_send;
        data_to_send.push_back(id);
        data = data_to_send;

        pn_delivery_update(unwrap(t), state);
        // pn_delivery_settle(o);
        // delivery.update(0x34)
    }
}

void transaction_impl::release_pending() {
    for (auto d : pending) {
        // d.update(released);
        // d.settle();
        // TODO: fix it
        delivery d2(make_wrapper<delivery>(unwrap(d)));
        d2.release();
    }
    pending.clear();
}

void transaction_impl::handle_outcome(proton::tracker t) {

    // std::vector<std::string> _data =
    // proton::get<std::vector<std::string>>(val);
    // auto _session = t.session();
    std::cout << "  ## handle_outcome::txn_impl i am is " << this << std::endl;
    std::cout << "  ## handle_outcome::_declare is " << _declare << std::endl;
    std::cout << "  ## handle_outcome::tracker is " << t << std::endl;

    pn_disposition_t *disposition = pn_delivery_remote(unwrap(t));
    // TODO: handle outcome
    if(_declare == t) {
        std::cout << "    transaction_impl::handle_outcome => got _declare"
                  << std::endl;
        proton::value val(pn_disposition_data(disposition));
        auto vd = get<std::vector<proton::binary>>(val);
        // TODO: Ensure _txn_impl is not NULL.
        if (vd.size() > 0) {
           session_context::get(t.session().pn_object())._txn_impl->set_id(vd[0]);
            std::cout << "    transaction_impl: handle_outcome.. txn_declared "
                         "got txnid:: "
                      << vd[0] << std::endl;
            handler->on_transaction_declared(t.session());
        } else if (pn_disposition_is_failed(disposition)) {
            std::cout << "    transaction_impl: handle_outcome.. "
                         "txn_declared_failed pn_disposition_is_failed "
                      << std::endl;
            handler->on_transaction_declare_failed(t.session());
        } else {
            std::cout
                << "    transaction_impl: handle_outcome.. txn_declared_failed "
                << std::endl;
            handler->on_transaction_declare_failed(t.session());
        }
    } else if (_discharge == t) {
        if (pn_disposition_is_failed(disposition)) {
            if (!failed) {
                std::cout
                    << "    transaction_impl: handle_outcome.. commit failed "
                    << std::endl;
                handler->on_transaction_commit_failed(t.session());
                // release pending
            }
        } else {
            if (failed) {
                handler->on_transaction_aborted(t.session());
                std::cout
                    << "    transaction_impl: handle_outcome.. txn aborted"
                    << std::endl;
                // release pending
            } else {
                handler->on_transaction_committed(t.session());
                std::cout
                    << "    transaction_impl: handle_outcome.. txn commited"
                    << std::endl;
            }
        }
        pending.clear();
    } else {
        std::cout << "    transaction_impl::handle_outcome => got NONE!"
                  << std::endl;
    }
}

} // namespace proton
