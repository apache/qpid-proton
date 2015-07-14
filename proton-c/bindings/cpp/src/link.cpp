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
#include "proton/link.hpp"
#include "proton/error.hpp"
#include "proton/connection.hpp"
#include "connection_impl.hpp"
#include "msg.hpp"
#include "contexts.hpp"
#include "proton_impl_ref.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

template class proton_handle<pn_link_t>;
typedef proton_impl_ref<link> PI;

link::link(pn_link_t* p) {
    verify_type(p);
    PI::ctor(*this, p);
    if (p) sender_link = pn_link_is_sender(p);
}
link::link() {
    PI::ctor(*this, 0);
}
link::link(const link& c) : proton_handle<pn_link_t>() {
    verify_type(impl_);
    PI::copy(*this, c);
    sender_link = c.sender_link;
}
link& link::operator=(const link& c) {
    verify_type(impl_);
    sender_link = c.sender_link;
    return PI::assign(*this, c);
}
link::~link() { PI::dtor(*this); }

void link::verify_type(pn_link_t *l) {} // Generic link can be sender or receiver

pn_link_t *link::pn_link() const { return impl_; }

void link::open() {
    pn_link_open(impl_);
}

void link::close() {
    pn_link_close(impl_);
}

bool link::is_sender() {
    return impl_ && sender_link;
}

bool link::is_receiver() {
    return impl_ && !sender_link;
}

int link::credit() {
    return pn_link_credit(impl_);
}

terminus link::source() {
    return terminus(pn_link_source(impl_), this);
}

terminus link::target() {
    return terminus(pn_link_target(impl_), this);
}

terminus link::remote_source() {
    return terminus(pn_link_remote_source(impl_), this);
}

terminus link::remote_target() {
    return terminus(pn_link_remote_target(impl_), this);
}

std::string link::name() {
    return std::string(pn_link_name(impl_));
}

class connection &link::connection() {
    pn_session_t *s = pn_link_session(impl_);
    pn_connection_t *c = pn_session_connection(s);
    return connection_impl::reactor_reference(c);
}

link link::next(endpoint::state mask) {

    return link(pn_link_next(impl_, (pn_state_t) mask));
}

}
