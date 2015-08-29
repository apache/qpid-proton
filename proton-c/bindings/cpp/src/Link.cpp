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
#include "contexts.h"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {
namespace cpp {
namespace reactor {


Link::Link(pn_link_t *lnk, bool isSenderLink) : pnLink(lnk), senderLink(isSenderLink)
{
    if (!lnk)
        throw "TODO: some exception";
    if (isSenderLink != pn_link_is_sender(lnk))
        throw "TODO: exception for wrong link type";
    pn_incref(pnLink);
}

Link::~Link() {
    pn_decref(pnLink);
}

    Link::Link(const Link& l) : pnLink(l.pnLink), senderLink(l.senderLink) {
    pn_incref(pnLink);
}

Link& Link::operator=(const Link& l) {
    pnLink = l.pnLink;
    senderLink = l.senderLink;
    pn_incref(pnLink);
    return *this;
}

pn_link_t *Link::getPnLink() { return pnLink; }

void Link::open() {
    pn_link_open(pnLink);
}

void Link::close() {
    pn_link_close(pnLink);
}

bool Link::isSender() {
    return senderLink;
}

bool Link::isReceiver() {
    return !senderLink;
}

int Link::getCredit() {
    return pn_link_credit(pnLink);
}

Connection &Link::getConnection() {
    pn_session_t *s = pn_link_session(pnLink);
    pn_connection_t *c = pn_session_connection(s);
    return *getConnectionContext(c);
}

}}} // namespace proton::cpp::reactor
