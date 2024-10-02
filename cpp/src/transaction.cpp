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
    proton::symbol descriptor("amqp:declare:list");
    // proton::value _value = vd;
    // TODO: How to make list;
    std::vector<uint16_t> vd;
    proton::value _value;
    // proton::get()
    _declare = send_ctrl(descriptor, _value );
}

void transaction_impl::discharge(bool failed) {
    failed = failed;
    proton::symbol descriptor("amqp:declare:list");;
    proton::value _value;
    proton::tracker discharge = send_ctrl(descriptor, _value);
}

proton::tracker transaction_impl::send_ctrl(proton::symbol descriptor, proton::value _value) {
    proton::message msg = _value; // TODO
    proton::tracker delivery = txn_ctrl->send(msg);
    delivery.transaction(*this);
    return delivery;
}

proton::tracker transaction_impl::send(proton::sender s, proton::message msg) {
    proton::tracker tracker = s.send(msg);
    return tracker;
}

void transaction_impl::handle_outcome(proton::tracker t) {
    // this->handler.on_transaction_declared();

}

transaction mk_transaction_impl(sender& s, transaction_handler& h, bool f) {
    return transaction_impl{s, h, f};
}

}
