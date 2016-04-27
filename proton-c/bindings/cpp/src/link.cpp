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

#include "proton_bits.hpp"

#include "proton/link.hpp"
#include "proton/error.hpp"
#include "proton/connection.hpp"

#include "msg.hpp"
#include "container_impl.hpp"
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {
namespace internal {

void link::attach() {
    pn_link_open(pn_object());
}

void link::close() {
    pn_link_close(pn_object());
}

void link::detach() {
    pn_link_detach(pn_object());
}

int link::credit() const {
    return pn_link_credit(pn_object());
}

int link::queued() { return pn_link_queued(pn_object()); }
int link::unsettled() { return pn_link_unsettled(pn_object()); }
int link::drained() { return pn_link_drained(pn_object()); }

std::string link::name() const { return str(pn_link_name(pn_object()));}

container& link::container() const {
    return connection().container();
}

class connection link::connection() const {
    return pn_session_connection(pn_link_session(pn_object()));
}

class session link::session() const {
    return pn_link_session(pn_object());
}

void link::handler(proton_handler &h) {
    pn_record_t *record = pn_link_attachments(pn_object());
    internal::pn_ptr<pn_handler_t> chandler = connection().container().impl_->cpp_handler(&h);
    pn_record_set_handler(record, chandler.get());
}

void link::detach_handler() {
    pn_record_t *record = pn_link_attachments(pn_object());
    pn_record_set_handler(record, 0);
}

error_condition link::error() const {
    return pn_link_remote_condition(pn_object());
}

ssize_t link::recv(char* buffer, size_t size) {
    return pn_link_recv(pn_object(), buffer, size);
}

bool link::advance() {
    return pn_link_advance(pn_object());
}

sender_options::sender_settle_mode link::sender_settle_mode() {
    return (sender_options::sender_settle_mode) pn_link_snd_settle_mode(pn_object());
}

void link::sender_settle_mode(sender_options::sender_settle_mode mode) {
    pn_link_set_snd_settle_mode(pn_object(), (pn_snd_settle_mode_t) mode);
}

receiver_options::receiver_settle_mode link::receiver_settle_mode() {
    return (receiver_options::receiver_settle_mode) pn_link_rcv_settle_mode(pn_object());
}

void link::receiver_settle_mode(receiver_options::receiver_settle_mode mode) {
    pn_link_set_rcv_settle_mode(pn_object(), (pn_rcv_settle_mode_t) mode);
}

sender_options::sender_settle_mode link::remote_sender_settle_mode() {
    return (sender_options::sender_settle_mode) pn_link_remote_snd_settle_mode(pn_object());
}

receiver_options::receiver_settle_mode link::remote_receiver_settle_mode() {
    return (receiver_options::receiver_settle_mode) pn_link_remote_rcv_settle_mode(pn_object());
}

link_context &link::context() {
    return link_context::get(pn_object());
}

}}
