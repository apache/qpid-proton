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

namespace proton {
namespace reactor {

class ConnectionImpl;
void setConnectionContext(pn_connection_t *pnConnection, ConnectionImpl *connection);
ConnectionImpl *getConnectionContext(pn_connection_t *pnConnection);

class Session;
void setSessionContext(pn_session_t *pnSession, Session *session);
Session *getSessionContext(pn_session_t *pnSession);

class Link;
void setLinkContext(pn_link_t *pnLink, Link *link);
Link *getLinkContext(pn_link_t *pnLink);

class ContainerImpl;
void setContainerContext(pn_reactor_t *pnReactor, ContainerImpl *container);
ContainerImpl *getContainerContext(pn_reactor_t *pnReactor);

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_CONTEXTS_H*/
