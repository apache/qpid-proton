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
#include "proton/cpp/MessagingEvent.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Session.h"
#include "proton/cpp/MessagingAdapter.h"
#include "proton/cpp/Acceptor.h"
#include "proton/cpp/exceptions.h"
#include "ContainerImpl.h"
#include "PrivateImplRef.h"
#include "LogInternal.h"

#include "Connector.h"
#include "contexts.h"
#include "Url.h"
#include "platform.h"

#include "proton/connection.h"
#include "proton/session.h"

namespace proton {
namespace reactor {

template class Handle<ContainerImpl>;
typedef PrivateImplRef<Container> PI;

Container::Container(ContainerImpl* p) { PI::ctor(*this, p); }
Container::Container(const Container& c) : Handle<ContainerImpl>() { PI::copy(*this, c); }
Container& Container::operator=(const Container& c) { return PI::assign(*this, c); }
Container::~Container() { PI::dtor(*this); }

Container::Container(MessagingHandler &mhandler) {
    ContainerImpl *cimpl = new ContainerImpl(mhandler);
    PI::ctor(*this, cimpl);
}

Connection Container::connect(std::string &host) { return impl->connect(host); }

pn_reactor_t *Container::getReactor() { return impl->getReactor(); }

pn_handler_t *Container::getGlobalHandler() { return impl->getGlobalHandler(); }

std::string Container::getContainerId() { return impl->getContainerId(); }


Sender Container::createSender(Connection &connection, std::string &addr) {
    return impl->createSender(connection, addr);
}

Sender Container::createSender(std::string &urlString) {
    return impl->createSender(urlString);
}

Receiver Container::createReceiver(Connection &connection, std::string &addr) {
    return impl->createReceiver(connection, addr);
}

Acceptor Container::listen(const std::string &urlString) {
    return impl->listen(urlString);
}


void Container::run() {
    impl->run();
}

}} // namespace proton::reactor
