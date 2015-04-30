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

#include "contexts.h"
#include "proton/cpp/exceptions.h"
#include "Msg.h"
#include "proton/object.h"
#include "proton/session.h"
#include "proton/link.h"

PN_HANDLE(PNI_CPP_CONNECTION_CONTEXT)
PN_HANDLE(PNI_CPP_SESSION_CONTEXT)
PN_HANDLE(PNI_CPP_LINK_CONTEXT)
PN_HANDLE(PNI_CPP_CONTAINER_CONTEXT)

namespace proton {
namespace reactor {

void setConnectionContext(pn_connection_t *pnConnection, ConnectionImpl *connection) {
    pn_record_t *record = pn_connection_attachments(pnConnection);
    pn_record_def(record, PNI_CPP_CONNECTION_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_CONNECTION_CONTEXT, connection);
}

ConnectionImpl *getConnectionContext(pn_connection_t *pnConnection) {
    if (!pnConnection) return NULL;
    pn_record_t *record = pn_connection_attachments(pnConnection);
    ConnectionImpl *p = (ConnectionImpl *) pn_record_get(record, PNI_CPP_CONNECTION_CONTEXT);
    return p;
}


void setSessionContext(pn_session_t *pnSession, Session *session) {
    pn_record_t *record = pn_session_attachments(pnSession);
    pn_record_def(record, PNI_CPP_SESSION_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_SESSION_CONTEXT, session);
}

Session *getSessionContext(pn_session_t *pnSession) {
    if (!pnSession) return NULL;
    pn_record_t *record = pn_session_attachments(pnSession);
    Session *p = (Session *) pn_record_get(record, PNI_CPP_SESSION_CONTEXT);
    return p;
}


void setLinkContext(pn_link_t *pnLink, Link *link) {
    pn_record_t *record = pn_link_attachments(pnLink);
    pn_record_def(record, PNI_CPP_LINK_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_LINK_CONTEXT, link);
}

Link *getLinkContext(pn_link_t *pnLink) {
    if (!pnLink) return NULL;
    pn_record_t *record = pn_link_attachments(pnLink);
    Link *p = (Link *) pn_record_get(record, PNI_CPP_LINK_CONTEXT);
    return p;
}


void setContainerContext(pn_reactor_t *pnReactor, ContainerImpl *container) {
    pn_record_t *record = pn_reactor_attachments(pnReactor);
    pn_record_def(record, PNI_CPP_CONTAINER_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_CONTAINER_CONTEXT, container);
}

ContainerImpl *getContainerContext(pn_reactor_t *pnReactor) {
    pn_record_t *record = pn_reactor_attachments(pnReactor);
    ContainerImpl *p = (ContainerImpl *) pn_record_get(record, PNI_CPP_CONTAINER_CONTEXT);
    if (!p) throw ProtonException(MSG("Reactor has no C++ container context"));
    return p;
}

}} // namespace proton::reactor
