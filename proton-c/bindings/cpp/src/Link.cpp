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

namespace {

static inline void throwIfNull(pn_link_t *l) { if (!l) throw ProtonException(MSG("Disassociated link")); }

}

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
    throwIfNull(impl);
    pn_link_open(impl);
}

void Link::close() {
    throwIfNull(impl);
    pn_link_close(impl);
}

bool Link::isSender() {
    return impl && senderLink;
}

bool Link::isReceiver() {
    return impl && !senderLink;
}

int Link::getCredit() {
    throwIfNull(impl);
    return pn_link_credit(impl);
}

Connection &Link::getConnection() {
    throwIfNull(impl);
    pn_session_t *s = pn_link_session(impl);
    pn_connection_t *c = pn_session_connection(s);
    return ConnectionImpl::getReactorReference(c);
}

}} // namespace proton::reactor
