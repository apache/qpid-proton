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
#include "proton/MessagingEvent.hpp"
#include "proton/Connection.hpp"
#include "proton/Session.hpp"
#include "proton/MessagingAdapter.hpp"
#include "proton/Acceptor.hpp"
#include "proton/Error.hpp"
#include "ContainerImpl.hpp"
#include "PrivateImplRef.hpp"

#include "Connector.hpp"
#include "contexts.hpp"
#include "Url.hpp"

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

Container::Container() {
    ContainerImpl *cimpl = new ContainerImpl();
    PI::ctor(*this, cimpl);
}

Connection Container::connect(std::string &host, Handler *h) { return impl->connect(host, h); }

pn_reactor_t *Container::getReactor() { return impl->getReactor(); }

std::string Container::getContainerId() { return impl->getContainerId(); }

Duration Container::getTimeout() { return impl->getTimeout(); }
void Container::setTimeout(Duration timeout) { impl->setTimeout(timeout); }


Sender Container::createSender(Connection &connection, std::string &addr, Handler *h) {
    return impl->createSender(connection, addr, h);
}

Sender Container::createSender(std::string &urlString) {
    return impl->createSender(urlString);
}

Receiver Container::createReceiver(Connection &connection, std::string &addr) {
    return impl->createReceiver(connection, addr);
}

Receiver Container::createReceiver(const std::string &url) {
    return impl->createReceiver(url);
}

Acceptor Container::listen(const std::string &urlString) {
    return impl->listen(urlString);
}


void Container::run() { impl->run(); }
void Container::start() { impl->start(); }
bool Container::process() { return impl->process(); }
void Container::stop() { impl->stop(); }
void Container::wakeup() { impl->wakeup(); }
bool Container::isQuiesced() { return impl->isQuiesced(); }

}} // namespace proton::reactor
