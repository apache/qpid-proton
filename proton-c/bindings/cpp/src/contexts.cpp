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

#include "contexts.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "proton/object.h"
#include "proton/message.h"
#include "proton/session.h"
#include "proton/link.h"

PN_HANDLE(PNI_CPP_CONNECTION_CONTEXT)
PN_HANDLE(PNI_CPP_CONTAINER_CONTEXT)
PN_HANDLE(PNI_CPP_EVENT_CONTEXT)

namespace proton {

void connection_context(pn_connection_t *pn_connection, connection_impl *connection) {
    pn_record_t *record = pn_connection_attachments(pn_connection);
    pn_record_def(record, PNI_CPP_CONNECTION_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_CONNECTION_CONTEXT, connection);
}
connection_impl *connection_context(pn_connection_t *pn_connection) {
    if (!pn_connection) return NULL;
    pn_record_t *record = pn_connection_attachments(pn_connection);
    connection_impl *p = (connection_impl *) pn_record_get(record, PNI_CPP_CONNECTION_CONTEXT);
    return p;
}


void container_context(pn_reactor_t *pn_reactor, container_impl *container) {
    pn_record_t *record = pn_reactor_attachments(pn_reactor);
    pn_record_def(record, PNI_CPP_CONTAINER_CONTEXT, PN_VOID);
    pn_record_set(record, PNI_CPP_CONTAINER_CONTEXT, container);
}
container_impl *container_context(pn_reactor_t *pn_reactor) {
    pn_record_t *record = pn_reactor_attachments(pn_reactor);
    container_impl *p = (container_impl *) pn_record_get(record, PNI_CPP_CONTAINER_CONTEXT);
    if (!p) throw error(MSG("Reactor has no C++ container context"));
    return p;
}

void event_context(pn_event_t *pn_event, pn_message_t *m) {
    pn_record_t *record = pn_event_attachments(pn_event);
    pn_record_def(record, PNI_CPP_EVENT_CONTEXT, PN_OBJECT); // refcount it for life of the event
    pn_record_set(record, PNI_CPP_EVENT_CONTEXT, m);
}
pn_message_t *event_context(pn_event_t *pn_event) {
    if (!pn_event) return NULL;
    pn_record_t *record = pn_event_attachments(pn_event);
    pn_message_t *p = (pn_message_t *) pn_record_get(record, PNI_CPP_EVENT_CONTEXT);
    return p;
}


}
