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
#include "proton/transport.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

const int endpoint::LOCAL_UNINIT = PN_LOCAL_UNINIT;
const int endpoint::REMOTE_UNINIT = PN_REMOTE_UNINIT;
const int endpoint::LOCAL_ACTIVE = PN_LOCAL_ACTIVE;
const int endpoint::REMOTE_ACTIVE = PN_REMOTE_ACTIVE;
const int endpoint::LOCAL_CLOSED = PN_LOCAL_CLOSED;
const int endpoint::REMOTE_CLOSED = PN_REMOTE_CLOSED;
const int endpoint::LOCAL_MASK = PN_LOCAL_MASK;
const int endpoint::REMOTE_MASK = PN_REMOTE_MASK;

endpoint::~endpoint() {}

}
