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
#include "proton/cpp/Session.h"
#include "contexts.h"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/cpp/Session.h"
#include "proton/cpp/Connection.h"
#include "ConnectionImpl.h"

namespace proton {
namespace reactor {


Session::Session(pn_session_t *s) : pnSession(s)
{
    pn_incref(pnSession);
}

Session::~Session() {
    pn_decref(pnSession);
}

pn_session_t *Session::getPnSession() { return pnSession; }

void Session::open() {
    pn_session_open(pnSession);
}

Connection &Session::getConnection() {
    pn_connection_t *c = pn_session_connection(pnSession);
    return ConnectionImpl::getReactorReference(c);
}

Receiver Session::createReceiver(std::string name) {
    pn_link_t *link = pn_receiver(pnSession, name.c_str());
    return Receiver(link);
}

Sender Session::createSender(std::string name) {
    pn_link_t *link = pn_sender(pnSession, name.c_str());
    return Sender(link);
}

}} // namespace proton::reactor
