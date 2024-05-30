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
#include "proton/tracker.hpp"

namespace proton {

void transaction_handler::on_transaction_declared(Transaction &) {}
void transaction_handler::on_transaction_committed(Transaction &) {}
void transaction_handler::on_transaction_aborted(Transaction &) {}
void transaction_handler::on_transaction_declare_failed(Transaction &) {}
void transaction_handler::on_transaction_commit_failed(Transaction &) {}

Transaction::Transaction(proton::sender _txn_ctrl, proton::transaction_handler _handler, bool _settle_before_discharge) {
    txn_ctrl = _txn_ctrl;
    handler = _handler;
    bool settle_before_discharge = _settle_before_discharge;
    declare();
}

void Transaction::commit() {
    discharge(false);
}

void Transaction::abort() {
    discharge(true);
}

void Transaction::declare() {
    proton::symbol descriptor("amqp:declare:list");
    // proton::value _value = vd;
    // TODO: How to make list;
    std::vector<uint16_t> vd({NULL});
    proton::value _value;
    // proton::get()
    _declare = send_ctrl(descriptor, _value );
}

void Transaction::discharge(bool failed) {
    this->failed = failed;
    proton::symbol descriptor("amqp:declare:list");;
    proton::value _value;
    proton::tracker discharge = send_ctrl(descriptor, _value);
}

proton::tracker Transaction::send_ctrl(proton::symbol descriptor, proton::value _value) {
    proton::message msg = _value; // TODO
    proton::tracker delivery = txn_ctrl.send(msg);
    delivery.set_transaction(this);
    return delivery;
}

proton::tracker Transaction::send(proton::sender s, proton::message msg) {
    proton::tracker tracker = s.send(msg);
    return tracker;
}

void Transaction::handle_outcome(proton::tracker t) {
    // this->handler.on_transaction_declared();

}

}
