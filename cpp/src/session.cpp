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
    proton::transaction_handler *handler = nullptr;
    proton::binary id;
    proton::tracker _declare;
    proton::tracker _discharge;
    bool failed = false;
    enum State {
      FREE,
      DECLARING,
      DECLARED,
      DISCHARE,
    };
    enum State state = State::FREE;
    std::vector<proton::tracker> pending;

    void commit();
    void abort();
    void declare();
    proton::tracker send(proton::sender s, proton::message msg);

    void discharge(bool failed);
    void release_pending();
    void accept(delivery &d);
    void update(tracker &d, uint64_t state);
    void set_id(binary _id);

    proton::tracker send_ctrl(proton::symbol descriptor, proton::value _value);
    void handle_outcome(proton::tracker t);
    transaction_impl(proton::sender &_txn_ctrl,
                     proton::transaction_handler &_handler,
                     bool _settle_before_discharge);
    ~transaction_impl();
};

// TODO : WE ARE NOT RETURNING TRANSACTION FOR NOW.
void session::declare_transaction(proton::transaction_handler &handler, bool settle_before_discharge) {
    auto &txn_impl = session_context::get(pn_object())._txn_impl;
    if (txn_impl == nullptr) {
        // Create _txn_impl
        proton::connection conn = this->connection();
        class InternalTransactionHandler : public proton::messaging_handler {

            void on_tracker_settle(proton::tracker &t) override {
                if (!t.session().txn_is_empty()) {
                    t.session().txn_handle_outcome(t);
                }
            }
        };

        proton::coordinator_options co;
        std::vector<symbol> cap = {proton::symbol("amqp:local-transactions")};
        co.capabilities(cap);


        proton::sender_options so;
        so.name("txn-ctrl");
        so.coordinator(co);

        static InternalTransactionHandler internal_handler; // internal_handler going out of scope. Fix it
        so.handler(internal_handler);

        static proton::sender s = conn.open_sender("does not matter", so);

        settle_before_discharge = false;

        txn_impl = new transaction_impl(s, handler, settle_before_discharge);
    }
    // Declare txn
    txn_impl->declare();

}

// clean up txn internally
void session::txn_delete() { auto &_txn_impl = session_context::get(pn_object())._txn_impl; delete _txn_impl; _txn_impl = nullptr;}
void session::txn_commit() { session_context::get(pn_object())._txn_impl->commit(); }
void session::txn_abort() { session_context::get(pn_object())._txn_impl->abort(); }
void session::txn_declare() { session_context::get(pn_object())._txn_impl->declare(); }
bool session::txn_is_empty() { return session_context::get(pn_object())._txn_impl == NULL; }
bool session::txn_is_declared() { return (!txn_is_empty()) && session_context::get(pn_object())._txn_impl->state == transaction_impl::State::DECLARED; }
void session::txn_accept(delivery &t) { return session_context::get(pn_object())._txn_impl->accept(t); }
proton::tracker session::txn_send(proton::sender s, proton::message msg) {
    return session_context::get(pn_object())._txn_impl->send(s, msg);
}
void session::txn_handle_outcome(proton::tracker t) {
    session_context::get(pn_object())._txn_impl->handle_outcome(t);
}

transaction_impl::transaction_impl(proton::sender &_txn_ctrl,
                                   proton::transaction_handler &_handler,
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
    _declare = send_ctrl(descriptor, _value );
}

void transaction_impl::discharge(bool _failed) {
    if (state != transaction_impl::State::DECLARED)
        throw proton::error("Only a delcared txn can be discharged.");
    state = State::DISCHARE;
            
    failed = _failed;
    proton::symbol descriptor("amqp:discharge:list");
    std::list<proton::value> vd;
    vd.push_back(id);
    vd.push_back(failed);
    proton::value _value = vd;
    _discharge = send_ctrl(descriptor, _value);
}

void transaction_impl::set_id(binary _id) {
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
    proton::tracker delivery = txn_ctrl.send(msg);
    return delivery;
}

proton::tracker transaction_impl::send(proton::sender s, proton::message msg) {
    if (state != transaction_impl::State::DECLARED)
        throw proton::error("Only a delcared txn can send a message");
    proton::tracker tracker = s.send(msg);
    update(tracker, 0x34);
    return tracker;
}

void transaction_impl::accept(delivery &t) {
    t.settle();
}

void transaction_impl::update(tracker &t, uint64_t state) {
    if (state) {
        proton::value data(pn_disposition_data(pn_delivery_local(unwrap(t))));
        std::list<proton::value> data_to_send;
        data_to_send.push_back(id);
        data = data_to_send;

        pn_delivery_update(unwrap(t), state);
    }
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
    if(_declare == t) {
        proton::value val(pn_disposition_data(disposition));
        auto vd = get<std::vector<proton::binary>>(val);
        // TODO: Ensure _txn_impl is not NULL.
        if (vd.size() > 0) {
           session_context::get(t.session().pn_object())._txn_impl->set_id(vd[0]);
            state = State::DECLARED;
            handler->on_transaction_declared(t.session());
        } else if (pn_disposition_is_failed(disposition)) {
            state = State::FREE;
            t.session().txn_delete();
            handler->on_transaction_declare_failed(t.session());
            return;
        } else {
            state = State::FREE;
            t.session().txn_delete();
            handler->on_transaction_declare_failed(t.session());
            return;
        }
    } else if (_discharge == t) {
        if (pn_disposition_is_failed(disposition)) {
            if (!failed) {
                state = State::FREE;
                t.session().txn_delete();
                handler->on_transaction_commit_failed(t.session());
                // release pending
                return;
            }
        } else {
            if (failed) {
                state = State::FREE;
                t.session().txn_delete();
                handler->on_transaction_aborted(t.session());
                // release pending
                return;
            } else {
                state = State::FREE;
                t.session().txn_delete();
                handler->on_transaction_committed(t.session());
                return;
            }
        }
        pending.clear();
    } else {
        std::cout << "    transaction_impl::handle_outcome => got NONE!"
                  << std::endl;
    }
}

} // namespace proton
