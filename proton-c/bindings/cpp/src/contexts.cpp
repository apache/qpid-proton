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
#include "reactor.hpp"

#include "proton/error.hpp"

#include "proton/connection.h"
#include "proton/object.h"
#include "proton/link.h"
#include "proton/message.h"
#include "proton/reactor.h"
#include "proton/session.h"

#include <typeinfo>

namespace proton {

namespace {
void cpp_context_finalize(void* v) { reinterpret_cast<context*>(v)->~context(); }
#define CID_cpp_context CID_pn_object
#define cpp_context_initialize NULL
#define cpp_context_finalize cpp_context_finalize
#define cpp_context_hashcode NULL
#define cpp_context_compare NULL
#define cpp_context_inspect NULL
pn_class_t cpp_context_class = PN_CLASS(cpp_context);

// Handles
PN_HANDLE(CONNECTION_CONTEXT)
PN_HANDLE(CONTAINER_CONTEXT)
PN_HANDLE(LISTENER_CONTEXT)
PN_HANDLE(LINK_CONTEXT)

void set_context(pn_record_t* record, pn_handle_t handle, const pn_class_t *clazz, void* value)
{
    pn_record_def(record, handle, clazz);
    pn_record_set(record, handle, value);
}

template <class T>
T* get_context(pn_record_t* record, pn_handle_t handle) {
    return reinterpret_cast<T*>(pn_record_get(record, handle));
}

}

context::~context() {}

void *context::alloc(size_t n) { return pn_object_new(&cpp_context_class, n); }

pn_class_t* context::pn_class() { return &cpp_context_class; }


context::id connection_context::id(pn_connection_t* c) {
    return context::id(pn_connection_attachments(c), CONNECTION_CONTEXT);
}

void container_context::set(const reactor& r, container& c) {
    set_context(pn_reactor_attachments(r.pn_object()), CONTAINER_CONTEXT, PN_VOID, &c);
}

container &container_context::get(pn_reactor_t *pn_reactor) {
    container *ctx = get_context<container>(pn_reactor_attachments(pn_reactor), CONTAINER_CONTEXT);
    if (!ctx) throw error(MSG("Reactor has no C++ container context"));
    return *ctx;
}

listener_context& listener_context::get(pn_acceptor_t* a) {
    // A Proton C pn_acceptor_t is really just a selectable
    pn_selectable_t *sel = reinterpret_cast<pn_selectable_t*>(a);

    listener_context* ctx =
        get_context<listener_context>(pn_selectable_attachments(sel), LISTENER_CONTEXT);
    if (!ctx) {
        ctx =  context::create<listener_context>();
        set_context(pn_selectable_attachments(sel), LISTENER_CONTEXT, context::pn_class(), ctx);
        pn_decref(ctx);
    }
    return *ctx;
}

link_context& link_context::get(pn_link_t* l) {
    link_context* ctx =
        get_context<link_context>(pn_link_attachments(l), LINK_CONTEXT);
    if (!ctx) {
        ctx =  context::create<link_context>();
        set_context(pn_link_attachments(l), LINK_CONTEXT, context::pn_class(), ctx);
        pn_decref(ctx);
    }
    return *ctx;
}

}
