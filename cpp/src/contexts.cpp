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
#include "reconnect_options_impl.hpp"
#include "proton_bits.hpp"

#include "proton/connection_options.hpp"
#include "proton/error.hpp"
#include "proton/reconnect_options.hpp"

#include <proton/connection.h>
#include <proton/object.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/session.h>

#include <typeinfo>

namespace proton {

namespace {
void cpp_context_finalize(void* v) { reinterpret_cast<context*>(v)->~context(); }
#define CID_cpp_context CID_pn_object
#define cpp_context_initialize NULL
#define cpp_context_hashcode NULL
#define cpp_context_compare NULL
#define cpp_context_inspect NULL
pn_class_t cpp_context_class = PN_CLASS(cpp_context);

// Handles
PN_HANDLE(CONNECTION_CONTEXT)
PN_HANDLE(LISTENER_CONTEXT)
PN_HANDLE(SESSION_CONTEXT)
PN_HANDLE(LINK_CONTEXT)

template <class T>
T* get_context(pn_record_t* record, pn_handle_t handle) {
    return reinterpret_cast<T*>(pn_record_get(record, handle));
}

}

context::~context() {}

void *context::alloc(size_t n) { return pn_object_new(&cpp_context_class, n); }

pn_class_t* context::pn_class() { return &cpp_context_class; }

connection_context::connection_context() :
    container(0), default_session(0), link_gen(0), handler(0), listener_context_(0)
{}

reconnect_context::reconnect_context(const reconnect_options_base& ro) :
    reconnect_options_(ro), retries_(0), current_url_(-1), stop_reconnect_(false), reconnected_(false)
{}

listener_context::listener_context() : listen_handler_(0) {}

connection_context& connection_context::get(pn_connection_t *c) {
    return ref<connection_context>(id(pn_connection_attachments(c), CONNECTION_CONTEXT));
}

listener_context& listener_context::get(pn_listener_t* l) {
    return ref<listener_context>(id(pn_listener_attachments(l), LISTENER_CONTEXT));
}

link_context& link_context::get(pn_link_t* l) {
    return ref<link_context>(id(pn_link_attachments(l), LINK_CONTEXT));
}

session_context& session_context::get(pn_session_t* s) {
    return ref<session_context>(id(pn_session_attachments(s), SESSION_CONTEXT));
}

}
