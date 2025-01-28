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
#include "proton/receiver_options.hpp"
#include "proton/sender_options.hpp"
#include "proton/session_options.hpp"
#include "proton/target_options.hpp"
#include "proton/transaction.hpp"
#include "proton/messaging_handler.hpp"

#include "contexts.hpp"
#include "link_namer.hpp"
#include "proton_bits.hpp"

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

transaction session::declare_transaction(proton::transaction_handler &handler, bool settle_before_discharge) {
    proton::connection conn = this->connection();
    class InternalTransactionHandler : public proton::messaging_handler {
        // TODO: auto_settle

        void on_tracker_settle(proton::tracker &t) override {
            std::cout<<"    [InternalTransactionHandler][on_tracker_settle] called with tracker.txn"
                 << std::endl;
            if (!t.transaction().is_empty()) {
                t.transaction().handle_outcome(t);
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

    auto txn =
        transaction::mk_transaction_impl(s, handler, settle_before_discharge);
    std::cout<<"    [declare_transaction] txn address:" << &txn << std::endl;

    return txn;
}

} // namespace proton
