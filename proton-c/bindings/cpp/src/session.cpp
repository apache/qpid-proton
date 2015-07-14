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
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/session.hpp"
#include "proton/connection.hpp"
#include "connection_impl.hpp"
#include "proton_impl_ref.hpp"

namespace proton {

template class proton_handle<pn_session_t>;
typedef proton_impl_ref<session> PI;

session::session(pn_session_t *p) {
    PI::ctor(*this, p);
}
session::session() {
    PI::ctor(*this, 0);
}
session::session(const session& c) : proton_handle<pn_session_t>() {
    PI::copy(*this, c);
}
session& session::operator=(const session& c) {
    return PI::assign(*this, c);
}
session::~session() {
    PI::dtor(*this);
}

pn_session_t *session::pn_session() { return impl_; }

void session::open() {
    pn_session_open(impl_);
}

connection &session::connection() {
    pn_connection_t *c = pn_session_connection(impl_);
    return connection_impl::reactor_reference(c);
}

receiver session::create_receiver(const std::string& name) {
    pn_link_t *link = pn_receiver(impl_, name.c_str());
    return receiver(link);
}

sender session::create_sender(const std::string& name) {
    pn_link_t *link = pn_sender(impl_, name.c_str());
    return sender(link);
}

}
