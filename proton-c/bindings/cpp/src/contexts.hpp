#ifndef PROTON_CPP_CONTEXTS_H
#define PROTON_CPP_CONTEXTS_H

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
#include "proton/reactor.h"
#include "proton/connection.h"
#include "proton/message.h"

namespace proton {

class connection_impl;
void connection_context(pn_connection_t *pn_connection, connection_impl *connection);
connection_impl *connection_context(pn_connection_t *pn_connection);

class session;
void session_context(pn_session_t *pn_session, session *session);
session *session_context(pn_session_t *pn_session);

class link;
void link_context(pn_link_t *pn_link, link *link);
link *link_context(pn_link_t *pn_link);

class container_impl;
void container_context(pn_reactor_t *pn_reactor, container_impl *container);
container_impl *container_context(pn_reactor_t *pn_reactor);

void event_context(pn_event_t *pn_event, pn_message_t *m);
pn_message_t *event_context(pn_event_t *pn_event);

}

#endif  /*!PROTON_CPP_CONTEXTS_H*/
