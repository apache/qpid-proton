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
#include "proton/message.hpp"
#include "proton/target_options.hpp"
#include "proton/tracker.hpp"

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
proton::tracker transaction::send(proton::sender s, proton::message msg) {
    return _impl->send(s, msg);
};
void transaction::handle_outcome(proton::tracker t) {
    std::cout << "    transaction::handle_outcome = NO OP base class "
              << std::endl;
    _impl->handle_outcome(t);
};


transaction_impl::transaction_impl(proton::sender& _txn_ctrl, proton::transaction_handler& _handler, bool _settle_before_discharge):
    txn_ctrl(&_txn_ctrl),
    handler(&_handler)
{
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

void transaction_impl::discharge(bool failed) {
    failed = failed;
    proton::symbol descriptor("amqp:declare:list");
    proton::value _value;
    proton::tracker discharge = send_ctrl(descriptor, _value);
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
    proton::tracker delivery = txn_ctrl->send(msg);
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
    return tracker;
}

void transaction_impl::handle_outcome(proton::tracker t) {

    // std::vector<std::string> _data =
    // proton::get<std::vector<std::string>>(val);
    auto txn = t.transaction();
    std::cout << "  handle_outcome::txn_impl i am is " << this << std::endl;
    std::cout << "  handle_outcome::_declare is " << _declare << std::endl;
    std::cout << "  handle_outcome::tracker is " << t << std::endl;

    // TODO: handle outcome
    if(_declare == t) {
        std::cout<<"    transaction_impl::handle_outcome => got _declare" << std::endl;
        pn_disposition_t *disposition = pn_delivery_remote(unwrap(t));
        proton::value val(pn_disposition_data(disposition));
        auto vd = get<std::vector<proton::binary>>(val);
        txn._impl->id = vd[0];
        std::cout << "    transaction_impl: handle_outcome.. got txnid:: "
                  << vd[0] << std::endl;
        handler->on_transaction_declared(txn);
    } else if (_discharge == t) {
        std::cout << "    transaction_impl::handle_outcome => got _discharge"
                  << std::endl;
        handler->on_transaction_committed(txn);
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
