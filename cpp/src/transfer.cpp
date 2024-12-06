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

#include "proton/delivery.hpp"

#include "proton/connection.hpp"
// #include "proton/transaction.hpp"
#include "proton/link.hpp"
#include "proton/session.hpp"

#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/session.h>

#include "proton_bits.hpp"

#include <iostream>
#include <ostream>

namespace proton {

session transfer::session() const { return make_wrapper(pn_link_session(pn_delivery_link(pn_object()))); }
connection transfer::connection() const { return make_wrapper(pn_session_connection(pn_link_session(pn_delivery_link(pn_object())))); }
container& transfer::container() const { return connection().container(); }
work_queue& transfer::work_queue() const { return connection().work_queue(); }


bool transfer::settled() const { return pn_delivery_settled(pn_object()); }

void transfer::settle() { pn_delivery_settle(pn_object()); }

enum transfer::state transfer::state() const { return static_cast<enum state>(pn_delivery_remote_state(pn_object())); }

std::string to_string(enum transfer::state s) { return pn_disposition_type_name(s); }
std::ostream& operator<<(std::ostream& o, const enum transfer::state s) { return o << to_string(s); }

void transfer::transaction(proton::transaction t) {
    transfer_context &cc = transfer_context::get(pn_object());
    cc.transaction_ = std::make_unique<proton::transaction>(t);
}

transaction transfer::transaction() const {
    transfer_context& cc = transfer_context::get(pn_object());
    return *cc.transaction_;
}

void transfer::user_data(void* user_data) const {
    transfer_context& cc = transfer_context::get(pn_object());
    cc.user_data_ = user_data;
}

void* transfer::user_data() const {
    transfer_context& cc = transfer_context::get(pn_object());
    return cc.user_data_;
}

disposition transfer::remote() {
    auto me = pn_object();
    std::cout << "   transfer::remote ME => " << me << std::endl;

    auto dd = pn_delivery_remote(me);
    std::cout << "   transfer::remote dd => " << dd << std::endl;
    std::cout << "   transfer::remote.data dd => "
              << proton::value(pn_disposition_data(dd)) << std::endl;
    auto d2 = make_wrapper<disposition>(dd);
    std::cout << "   transfer::remote d2 ready => " << std::endl;
    return d2;
}
disposition transfer::local() {
    return make_wrapper<disposition>(pn_delivery_local(pn_object()));
}

proton::value disposition::data() const {
    return proton::value(pn_disposition_data(pn_object()));
}
}
