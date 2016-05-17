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
#include "proton_bits.hpp"

#include "proton/connection.hpp"
#include "proton/endpoint.hpp"
#include "proton/error_condition.hpp"
#include "proton/link.hpp"
#include "proton/session.hpp"

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>

namespace {

inline bool uninitialized(int state) { return state & PN_LOCAL_UNINIT; }
inline bool active(int state) { return state & PN_LOCAL_ACTIVE; }
inline bool closed(int state) { return (state & PN_LOCAL_CLOSED) && (state & PN_REMOTE_CLOSED); }
}

namespace proton {

bool connection::uninitialized() const { return ::uninitialized(pn_connection_state(pn_object())); }
bool connection::active() const { return ::active(pn_connection_state(pn_object())); }
bool connection::closed() const { return ::closed(pn_connection_state(pn_object())); }

void connection::close(const error_condition& condition) {
    set_error_condition(condition, pn_connection_condition(pn_object()));
    close();
}

bool session::uninitialized() const { return ::uninitialized(pn_session_state(pn_object())); }
bool session::active() const { return ::active(pn_session_state(pn_object())); }
bool session::closed() const { return ::closed(pn_session_state(pn_object())); }

void session::close(const error_condition& condition) {
    set_error_condition(condition, pn_session_condition(pn_object()));
    close();
}

bool link::uninitialized() const { return ::uninitialized(pn_link_state(pn_object())); }
bool link::active() const { return ::active(pn_link_state(pn_object())); }
bool link::closed() const { return ::closed(pn_link_state(pn_object())); }

void link::close(const error_condition& condition) {
    set_error_condition(condition, pn_link_condition(pn_object()));
    close();
}

endpoint::~endpoint() {}

}
