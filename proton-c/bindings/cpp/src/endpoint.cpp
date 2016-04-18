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

#include "proton/endpoint.hpp"

#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/link.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace {

inline bool uninitialized(int state) { return state & PN_LOCAL_UNINIT; }
inline bool local_active(int state) { return state & PN_LOCAL_ACTIVE; }
inline bool remote_active(int state) { return state & PN_REMOTE_ACTIVE; }
inline bool closed(int state) { return (state & PN_LOCAL_CLOSED) && (state & PN_REMOTE_CLOSED); }

}

namespace proton {

bool connection::uninitialized() const { return ::uninitialized(pn_connection_state(pn_object())); }
bool connection::local_active() const { return ::local_active(pn_connection_state(pn_object())); }
bool connection::remote_active() const { return ::remote_active(pn_connection_state(pn_object())); }
bool connection::closed() const { return ::closed(pn_connection_state(pn_object())); }

bool session::uninitialized() const { return ::uninitialized(pn_session_state(pn_object())); }
bool session::local_active() const { return ::local_active(pn_session_state(pn_object())); }
bool session::remote_active() const { return ::remote_active(pn_session_state(pn_object())); }
bool session::closed() const { return ::closed(pn_session_state(pn_object())); }

bool link::uninitialized() const { return ::uninitialized(pn_link_state(pn_object())); }
bool link::local_active() const { return ::local_active(pn_link_state(pn_object())); }
bool link::remote_active() const { return ::remote_active(pn_link_state(pn_object())); }
bool link::closed() const { return ::closed(pn_link_state(pn_object())); }

endpoint::~endpoint() {}

}
