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
#include "proton_impl_ref.hpp"

namespace proton {

template class proton_handle<pn_delivery_t>;
typedef proton_impl_ref<delivery> PI;

delivery::delivery(pn_delivery_t *p) {
    PI::ctor(*this, p);
}
delivery::delivery() {
    PI::ctor(*this, 0);
}
delivery::delivery(const delivery& c) : proton_handle<pn_delivery_t>() {
    PI::copy(*this, c);
}
delivery& delivery::operator=(const delivery& c) {
    return PI::assign(*this, c);
}
delivery::~delivery() {
    PI::dtor(*this);
}

bool delivery::settled() {
    return pn_delivery_settled(impl_);
}

void delivery::settle() {
    pn_delivery_settle(impl_);
}

pn_delivery_t *delivery::pn_delivery() { return impl_; }

}
