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
#include "proton/cpp/Link.h"
#include "proton/cpp/exceptions.h"
#include "proton/cpp/Connection.h"
#include "ConnectionImpl.h"
#include "Msg.h"
#include "contexts.h"
#include "ProtonImplRef.h"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {
namespace reactor {

template class ProtonHandle<pn_link_t>;
typedef ProtonImplRef<Link> PI;

Link::Link(pn_link_t* p) {
    verifyType(p);
    PI::ctor(*this, p);
    if (p) senderLink = pn_link_is_sender(p);
}
Link::Link() {
    PI::ctor(*this, 0);
}
Link::Link(const Link& c) : ProtonHandle<pn_link_t>() {
    verifyType(impl);
    PI::copy(*this, c);
    senderLink = c.senderLink;
}
Link& Link::operator=(const Link& c) {
    verifyType(impl);
    senderLink = c.senderLink;
    return PI::assign(*this, c);
}
Link::~Link() { PI::dtor(*this); }

void Link::verifyType(pn_link_t *l) {} // Generic link can be sender or receiver

pn_link_t *Link::getPnLink() const { return impl; }

void Link::open() {
    pn_link_open(impl);
}

void Link::close() {
    pn_link_close(impl);
}

bool Link::isSender() {
    return impl && senderLink;
}

bool Link::isReceiver() {
    return impl && !senderLink;
}

int Link::getCredit() {
    return pn_link_credit(impl);
}

Terminus Link::getSource() {
    return Terminus(pn_link_source(impl), this);
}

Terminus Link::getTarget() {
    return Terminus(pn_link_target(impl), this);
}

Terminus Link::getRemoteSource() {
    return Terminus(pn_link_remote_source(impl), this);
}

Terminus Link::getRemoteTarget() {
    return Terminus(pn_link_remote_target(impl), this);
}

std::string Link::getName() {
    return std::string(pn_link_name(impl));
}

Connection &Link::getConnection() {
    pn_session_t *s = pn_link_session(impl);
    pn_connection_t *c = pn_session_connection(s);
    return ConnectionImpl::getReactorReference(c);
}

Link Link::getNext(Endpoint::State mask) {

    return Link(pn_link_next(impl, (pn_state_t) mask));
}

}} // namespace proton::reactor
