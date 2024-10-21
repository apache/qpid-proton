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

#include "proton/message.hpp"
#include "proton/transaction.hpp"
#include "proton/target_options.hpp"
#include "proton/tracker.hpp"

#include <proton/types.hpp>

#include <iostream>

namespace proton {

transaction_handler::~transaction_handler() = default;
void transaction_handler::on_transaction_declared(transaction &) {}
void transaction_handler::on_transaction_committed(transaction &) {}
void transaction_handler::on_transaction_aborted(transaction &) {}
void transaction_handler::on_transaction_declare_failed(transaction &) {}
void transaction_handler::on_transaction_commit_failed(transaction &) {}

transaction::~transaction() = default;
void transaction::commit() {};
void transaction::abort() {};
void transaction::declare() {};
proton::tracker transaction::send(proton::sender s, proton::message msg) { return {}; };
void transaction::handle_outcome(proton::tracker t) {
    std::cout<<"    transaction_impl::handle_outcome = NO OP base class " << std::endl;

};

class transaction_impl : public transaction {
  public:
    proton::sender* txn_ctrl = nullptr;
    proton::transaction_handler* handler = nullptr;
    // TODO int
    int id = 0;
    proton::tracker _declare;
    proton::tracker _discharge;
    bool failed = false;
    std::vector<proton::tracker> pending;

    transaction_impl(proton::sender& _txn_ctrl, proton::transaction_handler& _handler, bool _settle_before_discharge);
    void commit() override;
    void abort() override;
    void declare() override;
    proton::tracker send(proton::sender s, proton::message msg) override;

    void discharge(bool failed);
    proton::tracker send_ctrl(proton::symbol descriptor, proton::value _value);
    void handle_outcome(proton::tracker t);
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
    delivery.transaction(*this);
    std::cout << "    [transaction_impl::send_ctrl] sending done. I guess queued! " << std::endl;
    return delivery;
}

proton::tracker transaction_impl::send(proton::sender s, proton::message msg) {
    proton::tracker tracker = s.send(msg);
    return tracker;
}

void transaction_impl::handle_outcome(proton::tracker t) {
    // TODO: handle outcome
    if(_declare == t) {
        std::cout<<"    transaction_impl::handle_outcome => got _declare" << std::endl;

    }
    std::cout<<"    transaction_impl::handle_outcome => calling txn declared. handler: " << handler << std::endl;
    handler->on_transaction_declared(*this);

}

transaction mk_transaction_impl(sender& s, transaction_handler& h, bool f) {
    return transaction_impl{s, h, f};
}

}
