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
#include "proton/BlockingLink.hpp"
#include "proton/BlockingConnection.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/WaitCondition.hpp"
#include "proton/Error.hpp"
#include "Msg.hpp"


namespace proton {
namespace reactor {

namespace {
struct LinkOpened : public WaitCondition {
    LinkOpened(pn_link_t *l) : pnLink(l) {}
    bool achieved() { return !(pn_link_state(pnLink) & PN_REMOTE_UNINIT); }
    pn_link_t *pnLink;
};

struct LinkClosed : public WaitCondition {
    LinkClosed(pn_link_t *l) : pnLink(l) {}
    bool achieved() { return (pn_link_state(pnLink) & PN_REMOTE_CLOSED); }
    pn_link_t *pnLink;
};

struct LinkNotOpen : public WaitCondition {
    LinkNotOpen(pn_link_t *l) : pnLink(l) {}
    bool achieved() { return !(pn_link_state(pnLink) & PN_REMOTE_ACTIVE); }
    pn_link_t *pnLink;
};


} // namespace


BlockingLink::BlockingLink(BlockingConnection *c, pn_link_t *pnl) : connection(*c), link(pnl) {
    std::string msg = "Opening link " + link.getName();
    LinkOpened linkOpened(link.getPnLink());
    connection.wait(linkOpened, msg);
}

BlockingLink::~BlockingLink() {}

void BlockingLink::waitForClosed(Duration timeout) {
    std::string msg = "Closing link " + link.getName();
    LinkClosed linkClosed(link.getPnLink());
    connection.wait(linkClosed, msg);
    checkClosed();
}

void BlockingLink::checkClosed() {
    pn_link_t * pnLink = link.getPnLink();
    if (pn_link_state(pnLink) & PN_REMOTE_CLOSED) {
        link.close();
        // TODO: LinkDetached exception
        throw Error(MSG("Link detached"));
    }
}

void BlockingLink::close() {
    link.close();
    std::string msg = "Closing link " + link.getName();
    LinkNotOpen linkNotOpen(link.getPnLink());
    connection.wait(linkNotOpen, msg);
}

}} // namespace proton::reactor
