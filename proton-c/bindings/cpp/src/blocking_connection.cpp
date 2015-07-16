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
#include "proton/blocking_connection.hpp"
#include "proton/blocking_sender.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/url.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "blocking_connection_impl.hpp"
#include "private_impl_ref.hpp"

namespace proton {

template class handle<blocking_connection_impl>;
typedef private_impl_ref<blocking_connection> PI;

blocking_connection::blocking_connection() {PI::ctor(*this, 0); }

blocking_connection::blocking_connection(const blocking_connection& c) : handle<blocking_connection_impl>() { PI::copy(*this, c); }

blocking_connection& blocking_connection::operator=(const blocking_connection& c) { return PI::assign(*this, c); }
blocking_connection::~blocking_connection() { PI::dtor(*this); }

blocking_connection::blocking_connection(const proton::url &url, duration d, ssl_domain *ssld, container *c) {
    blocking_connection_impl *cimpl = new blocking_connection_impl(url, d,ssld, c);
    PI::ctor(*this, cimpl);
}

void blocking_connection::close() { impl_->close(); }

void blocking_connection::wait(wait_condition &cond) { return impl_->wait(cond); }
void blocking_connection::wait(wait_condition &cond, std::string &msg, duration timeout) {
    return impl_->wait(cond, msg, timeout);
}

blocking_sender blocking_connection::create_sender(const std::string &address, handler *h) {
    sender sender = impl_->container_.create_sender(impl_->connection_, address, h);
    return blocking_sender(*this, sender);
}

duration blocking_connection::timeout() { return impl_->timeout(); }

}
