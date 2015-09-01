/*
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
 */

#include "proton/facade.hpp"
#include <proton/object.h>
#include <assert.h>

// For empty check.
#include "proton/acceptor.hpp"
#include "proton/connection.hpp"
#include "proton/data.hpp"
#include "proton/decoder.hpp"
#include "proton/delivery.hpp"
#include "proton/encoder.hpp"
#include "proton/facade.hpp"
#include "proton/link.hpp"
#include "proton/message.hpp"
#include "proton/session.hpp"
#include "proton/terminus.hpp"
#include "proton/transport.hpp"

namespace proton {

void incref(const pn_counted* p) {
    if (p) pn_incref(const_cast<pn_counted*>(p)); 
}

void decref(const pn_counted* p) {
    if (p) pn_decref(const_cast<pn_counted*>(p));
}

#if PN_USE_CPP11
// Make sure facade types are empty.
#define CHECK_EMPTY(T) static_assert(std::is_empty<T>::value,  "facade " #T " not empty")

CHECK_EMPTY(acceptor);
CHECK_EMPTY(connection);
CHECK_EMPTY(data);
CHECK_EMPTY(decoder);
CHECK_EMPTY(delivery);
CHECK_EMPTY(encoder);
CHECK_EMPTY(link);
CHECK_EMPTY(message);
CHECK_EMPTY(session);
CHECK_EMPTY(terminus);
CHECK_EMPTY(transport);

#endif
}
