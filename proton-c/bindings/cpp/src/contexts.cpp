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
#include "msg.hpp"

#include "proton/error.hpp"
#include "proton/handler.hpp"

#include "proton/object.h"
#include "proton/message.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

namespace {

// A proton class for counted c++ objects used as proton attachments
extern pn_class_t* COUNTED_CONTEXT;
#define CID_cpp_context CID_pn_void
static const pn_class_t *cpp_context_reify(void *object) { return COUNTED_CONTEXT; }
#define cpp_context_new NULL
#define cpp_context_free NULL
#define cpp_context_initialize NULL
void cpp_context_incref(void* p) { proton::incref(reinterpret_cast<counted*>(p)); }
void cpp_context_decref(void* p) { proton::decref(reinterpret_cast<counted*>(p)); }
// Always return 1 to prevent the class finalizer logic running after we are deleted.
int cpp_context_refcount(void* p) { return 1; }
#define cpp_context_finalize NULL
#define cpp_context_hashcode NULL
#define cpp_context_compare NULL
#define cpp_context_inspect NULL

pn_class_t COUNTED_CONTEXT_ = PN_METACLASS(cpp_context);
pn_class_t *COUNTED_CONTEXT = &COUNTED_CONTEXT_;
}


void set_context(pn_record_t* record, pn_handle_t handle, counted* value)
{
    pn_record_def(record, handle, COUNTED_CONTEXT);
    pn_record_set(record, handle, value);
}

counted* get_context(pn_record_t* record, pn_handle_t handle) {
    return reinterpret_cast<counted*>(pn_record_get(record, handle));
}

// Connection context

PN_HANDLE(CONNECTION_CONTEXT)

connection_context::connection_context() : handler(0), default_session(0), container_impl(0) {}
connection_context::~connection_context() { delete handler; }

struct connection_context& connection_context::get(pn_connection_t* c) {
    connection_context* ctx = reinterpret_cast<connection_context*>(
        get_context(pn_connection_attachments(c), CONNECTION_CONTEXT));
    if (!ctx) {
        ctx = new connection_context();
        set_context(pn_connection_attachments(c), CONNECTION_CONTEXT, ctx);
    }
    return *ctx;
}

PN_HANDLE(CONTAINER_CONTEXT)

void container_context(pn_reactor_t *r, container& c) {
    pn_record_t *record = pn_reactor_attachments(r);
    pn_record_def(record, CONTAINER_CONTEXT, PN_VOID);
    pn_record_set(record, CONTAINER_CONTEXT, &c);
}

container &container_context(pn_reactor_t *pn_reactor) {
    pn_record_t *record = pn_reactor_attachments(pn_reactor);
    container *ctx = reinterpret_cast<container*>(pn_record_get(record, CONTAINER_CONTEXT));
    if (!ctx) throw error(MSG("Reactor has no C++ container context"));
    return *ctx;
}

PN_HANDLE(EVENT_CONTEXT)

void event_context(pn_event_t *pn_event, pn_message_t *m) {
    pn_record_t *record = pn_event_attachments(pn_event);
    pn_record_def(record, EVENT_CONTEXT, PN_OBJECT); // refcount it for life of the event
    pn_record_set(record, EVENT_CONTEXT, m);
}

pn_message_t *event_context(pn_event_t *pn_event) {
    if (!pn_event) return NULL;
    pn_record_t *record = pn_event_attachments(pn_event);
    pn_message_t *ctx = (pn_message_t *) pn_record_get(record, EVENT_CONTEXT);
    return ctx;
}


}
