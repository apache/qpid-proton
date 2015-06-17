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
#include "proton/Container.hpp"
#include "proton/BlockingConnection.hpp"
#include "proton/BlockingSender.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/exceptions.hpp"
#include "Msg.hpp"
#include "BlockingConnectionImpl.hpp"
#include "PrivateImplRef.hpp"

namespace proton {
namespace reactor {

template class Handle<BlockingConnectionImpl>;
typedef PrivateImplRef<BlockingConnection> PI;

BlockingConnection::BlockingConnection() {PI::ctor(*this, 0); }

BlockingConnection::BlockingConnection(const BlockingConnection& c) : Handle<BlockingConnectionImpl>() { PI::copy(*this, c); }

BlockingConnection& BlockingConnection::operator=(const BlockingConnection& c) { return PI::assign(*this, c); }
BlockingConnection::~BlockingConnection() { PI::dtor(*this); }

BlockingConnection::BlockingConnection(std::string &url, Duration d, SslDomain *ssld, Container *c) {
    BlockingConnectionImpl *cimpl = new BlockingConnectionImpl(url, d,ssld, c);
    PI::ctor(*this, cimpl);
}

void BlockingConnection::close() { impl->close(); }

void BlockingConnection::wait(WaitCondition &cond) { return impl->wait(cond); }
void BlockingConnection::wait(WaitCondition &cond, std::string &msg, Duration timeout) {
    return impl->wait(cond, msg, timeout);
}

BlockingSender BlockingConnection::createSender(std::string &address, Handler *h) {
    Sender sender = impl->container.createSender(impl->connection, address, h);
    return BlockingSender(*this, sender);
}

Duration BlockingConnection::getTimeout() { return impl->getTimeout(); }

}} // namespace proton::reactor
