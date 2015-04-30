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
#include "proton/cpp/Container.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Handler.h"
#include "proton/cpp/exceptions.h"
#include "Msg.h"
#include "contexts.h"
#include "ConnectionImpl.h"
#include "PrivateImplRef.h"

#include "proton/connection.h"

namespace proton {
namespace reactor {

template class Handle<ConnectionImpl>;
typedef PrivateImplRef<Connection> PI;

Connection::Connection() {}
Connection::Connection(ConnectionImpl* p) { PI::ctor(*this, p); }
Connection::Connection(const Connection& c) : Handle<ConnectionImpl>() { PI::copy(*this, c); }

Connection& Connection::operator=(const Connection& c) { return PI::assign(*this, c); }
Connection::~Connection() { PI::dtor(*this); }

Connection::Connection(Container &c) {
    ConnectionImpl *cimpl = new ConnectionImpl(c);
    PI::ctor(*this, cimpl);
}

Transport &Connection::getTransport() { return impl->getTransport(); }

Handler* Connection::getOverride() { return impl->getOverride(); }
void Connection::setOverride(Handler *h) { impl->setOverride(h); }

void Connection::open() { impl->open(); }

void Connection::close() { impl->close(); }

pn_connection_t *Connection::getPnConnection() { return impl->getPnConnection(); }

std::string Connection::getHostname() { return impl->getHostname(); }

Connection &Connection::getConnection() {
    return (*this);
}

Container &Connection::getContainer() { return impl->getContainer(); }

}} // namespace proton::reactor
