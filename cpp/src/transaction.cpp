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

#include "proton/transaction.hpp"
#include "proton/delivery.h"
#include "proton/delivery.hpp"
#include "proton/message.hpp"
#include "proton/target_options.hpp"
#include "proton/tracker.hpp"
#include "proton/transfer.hpp"

#include "proton_bits.hpp"
#include <proton/types.hpp>

#include <iostream>

namespace proton {

transaction_handler::~transaction_handler() = default;
void transaction_handler::on_transaction_declared(transaction) {}
void transaction_handler::on_transaction_committed(transaction) {}
void transaction_handler::on_transaction_aborted(transaction) {}
void transaction_handler::on_transaction_declare_failed(transaction) {}
void transaction_handler::on_transaction_commit_failed(transaction) {}

transaction::transaction() : _impl(NULL) {}  // empty transaction, not yet ready
// transaction::transaction(proton::sender& _txn_ctrl,
// proton::transaction_handler& _handler, bool _settle_before_discharge) :
//     _impl(std::make_shared<transaction_impl>(_txn_ctrl, _handler,
//     _settle_before_discharge)) {}
transaction::transaction(transaction_impl *impl)
    : _impl(impl) {}
// transaction::transaction( transaction_impl* impl): _impl(impl){}
transaction::~transaction() = default;
void transaction::commit() { _impl->commit(); };
void transaction::abort() { _impl->abort(); };
void transaction::declare() { _impl->declare(); };
bool transaction::is_empty() { return _impl == NULL; };
proton::tracker transaction::send(proton::sender s, proton::message msg) {
    return _impl->send(s, msg);
};
void transaction::handle_outcome(proton::tracker t) {
    std::cout << "    transaction::handle_outcome = NO OP base class "
              << std::endl;
    _impl->handle_outcome(t);
};

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
    delivery.transaction(transaction(this));
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

void transaction_impl::accept(tracker &t) {
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
    auto txn = t.transaction();
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
        if (vd.size() > 0) {
            txn._impl->set_id(vd[0]);
            std::cout << "    transaction_impl: handle_outcome.. txn_declared "
                         "got txnid:: "
                      << vd[0] << std::endl;
            handler->on_transaction_declared(txn);
        } else if (pn_disposition_is_failed(disposition)) {
            std::cout << "    transaction_impl: handle_outcome.. "
                         "txn_declared_failed pn_disposition_is_failed "
                      << std::endl;
            handler->on_transaction_declare_failed(txn);
        } else {
            std::cout
                << "    transaction_impl: handle_outcome.. txn_declared_failed "
                << std::endl;
            handler->on_transaction_declare_failed(txn);
        }
    } else if (_discharge == t) {
        if (pn_disposition_is_failed(disposition)) {
            if (!failed) {
                std::cout
                    << "    transaction_impl: handle_outcome.. commit failed "
                    << std::endl;
                handler->on_transaction_commit_failed(txn);
                // release pending
            }
        } else {
            if (failed) {
                handler->on_transaction_aborted(txn);
                std::cout
                    << "    transaction_impl: handle_outcome.. txn aborted"
                    << std::endl;
                // release pending
            } else {
                handler->on_transaction_committed(txn);
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

transaction transaction::mk_transaction_impl(sender &s, transaction_handler &h,
                                             bool f) {
    return transaction(new transaction_impl(s, h, f));
}
}
