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
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/session.h>

#include <typeinfo>

namespace proton {

namespace {
void cpp_context_finalize(void* v) { reinterpret_cast<context*>(v)->~context(); }
pn_class_t* cpp_context_class = pn_class_create("cpp_context", nullptr, cpp_context_finalize, nullptr, nullptr, nullptr);

// Handles
PN_HANDLE(CONNECTION_CONTEXT)
PN_HANDLE(LISTENER_CONTEXT)
PN_HANDLE(SESSION_CONTEXT)
PN_HANDLE(LINK_CONTEXT)
PN_HANDLE(TRANSFER_CONTEXT)

template <class T>
T* get_context(pn_record_t* record, pn_handle_t handle) {
    return reinterpret_cast<T*>(pn_record_get(record, handle));
}

}

context::~context() {}

void *context::alloc(size_t n) { return pn_class_new(cpp_context_class, n); }

pn_class_t* context::pn_class() { return cpp_context_class; }

connection_context::connection_context() :
    container(nullptr), default_session(nullptr), link_gen(nullptr), handler(nullptr), listener_context_(nullptr), user_data_(nullptr)
{}

reconnect_context::reconnect_context(const reconnect_options_base& ro) :
    reconnect_options_(ro), retries_(0), current_url_(-1), stop_reconnect_(false), reconnected_(false)
{}

listener_context::listener_context() : listen_handler_(nullptr), user_data_(nullptr) {}

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

transfer_context& transfer_context::get(pn_delivery_t* s) {
    return ref<transfer_context>(id(pn_delivery_attachments(s), TRANSFER_CONTEXT));
}

}
