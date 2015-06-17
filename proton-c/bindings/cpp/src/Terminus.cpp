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

#include "proton/Link.hpp"
#include "proton/link.h"

namespace proton {
namespace reactor {

template class ProtonHandle<pn_terminus_t>;
typedef ProtonImplRef<Terminus> PI;

// Note: the pn_terminus_t is not ref counted.  We count the parent link.

Terminus::Terminus() : link(0) {
    impl = 0;
}

Terminus::Terminus(pn_terminus_t *p, Link *l) : link(l) {
    impl = p;
    pn_incref(link->getPnLink());
}
Terminus::Terminus(const Terminus& c) : ProtonHandle<pn_terminus_t>() {
    impl = c.impl;
    link = c.link;
    pn_incref(link->getPnLink());
}
Terminus& Terminus::operator=(const Terminus& c) {
    if (impl == c.impl) return *this;
    if (impl) pn_decref(link->getPnLink());
    impl = c.impl;
    link = c.link;
    pn_incref(link->getPnLink());
    return *this;
}
Terminus::~Terminus() {
    if (impl)
        pn_decref(link->getPnLink());
}

pn_terminus_t *Terminus::getPnTerminus() { return impl; }

Terminus::Type Terminus::getType() {
    return (Type) pn_terminus_get_type(impl);
}

void Terminus::setType(Type type) {
    pn_terminus_set_type(impl, (pn_terminus_type_t) type);
}

Terminus::ExpiryPolicy Terminus::getExpiryPolicy() {
    return (ExpiryPolicy) pn_terminus_get_type(impl);
}

void Terminus::setExpiryPolicy(ExpiryPolicy policy) {
    pn_terminus_set_expiry_policy(impl, (pn_expiry_policy_t) policy);
}

Terminus::DistributionMode Terminus::getDistributionMode() {
    return (DistributionMode) pn_terminus_get_type(impl);
}

void Terminus::setDistributionMode(DistributionMode mode) {
    pn_terminus_set_distribution_mode(impl, (pn_distribution_mode_t) mode);
}

std::string Terminus::getAddress() {
    const char *addr = pn_terminus_get_address(impl);
    return addr ? std::string(addr) : std::string();
}

void Terminus::setAddress(std::string &addr) {
    pn_terminus_set_address(impl, addr.c_str());
}

bool Terminus::isDynamic() {
    return (Type) pn_terminus_is_dynamic(impl);
}

void Terminus::setDynamic(bool d) {
    pn_terminus_set_dynamic(impl, d);
}

}} // namespace proton::reactor
