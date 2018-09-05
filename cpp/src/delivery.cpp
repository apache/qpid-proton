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

#include "proton/delivery.hpp"

#include "proton/receiver.hpp"

#include "proton_bits.hpp"

#include <proton/delivery.h>

namespace {

void settle_delivery(pn_delivery_t* o, uint64_t state) {
    pn_delivery_update(o, state);
    pn_delivery_settle(o);
}

}

namespace proton {

delivery::delivery(pn_delivery_t* d): transfer(make_wrapper(d)) {}
receiver delivery::receiver() const { return make_wrapper<class receiver>(pn_delivery_link(pn_object())); }
delivery::~delivery() {}
void delivery::accept() { settle_delivery(pn_object(), ACCEPTED); }
void delivery::reject() { settle_delivery(pn_object(), REJECTED); }
void delivery::release() { settle_delivery(pn_object(), RELEASED); }
void delivery::modify() { settle_delivery(pn_object(), MODIFIED); }

}
