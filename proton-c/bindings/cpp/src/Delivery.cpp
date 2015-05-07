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

#include "proton/cpp/Delivery.h"
#include "proton/delivery.h"

namespace proton {
namespace reactor {

template class ProtonHandle<pn_delivery_t>;
typedef ProtonImplRef<Delivery> PI;

Delivery::Delivery(pn_delivery_t *p) {
    PI::ctor(*this, p);
}
Delivery::Delivery() {
    PI::ctor(*this, 0);
}
Delivery::Delivery(const Delivery& c) : ProtonHandle<pn_delivery_t>() {
    PI::copy(*this, c);
}
Delivery& Delivery::operator=(const Delivery& c) {
    return PI::assign(*this, c);
}
Delivery::~Delivery() {
    PI::dtor(*this);
}

bool Delivery::settled() {
    return pn_delivery_settled(impl);
}

void Delivery::settle() {
    pn_delivery_settle(impl);
}

pn_delivery_t *Delivery::getPnDelivery() { return impl; }

}} // namespace proton::reactor
