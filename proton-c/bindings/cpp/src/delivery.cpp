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
#include "proton/delivery.h"

namespace proton {

bool delivery::settled() const { return pn_delivery_settled(pn_cast(this)); }

void delivery::settle() { pn_delivery_settle(pn_cast(this)); }

void delivery::update(delivery::state state) { pn_delivery_update(pn_cast(this), state); }

void delivery::settle(delivery::state state) {
    update(state);
    settle();
}

bool delivery::partial()  const { return pn_delivery_partial(pn_cast(this)); }
bool delivery::readable() const { return pn_delivery_readable(pn_cast(this)); }
bool delivery::writable() const { return pn_delivery_writable(pn_cast(this)); }
bool delivery::updated()  const { return pn_delivery_updated(pn_cast(this)); }

void delivery::clear()  { pn_delivery_clear(pn_cast(this)); }
delivery::state delivery::remote_state() const { return state(pn_delivery_remote_state(pn_cast(this))); }
}
