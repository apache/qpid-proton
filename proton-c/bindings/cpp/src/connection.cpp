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
#include "proton/container.hpp"
#include "proton/connection.hpp"
#include "proton/handler.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "contexts.hpp"
#include "connection_impl.hpp"
#include "private_impl_ref.hpp"

#include "proton/connection.h"

namespace proton {

template class handle<connection_impl>;
typedef private_impl_ref<connection> PI;

connection::connection() {PI::ctor(*this, 0); }
connection::connection(connection_impl* p) { PI::ctor(*this, p); }
connection::connection(const connection& c) : handle<connection_impl>() { PI::copy(*this, c); }

connection& connection::operator=(const connection& c) { return PI::assign(*this, c); }
connection::~connection() { PI::dtor(*this); }

connection::connection(class container &c, handler *h) {
    connection_impl *cimpl = new connection_impl(c, h);
    PI::ctor(*this, cimpl);
}

transport &connection::transport() { return impl_->transport(); }

handler* connection::override() { return impl_->override(); }
void connection::override(handler *h) { impl_->override(h); }

void connection::open() { impl_->open(); }

void connection::close() { impl_->close(); }

pn_connection_t *connection::pn_connection() { return impl_->pn_connection(); }

std::string connection::hostname() { return impl_->hostname(); }

class container &connection::container() { return impl_->container(); }

link connection::link_head(endpoint::State mask) {
    return impl_->link_head(mask);
}

}
