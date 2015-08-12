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

#include "proton/link.hpp"
#include "proton/link.h"

namespace proton {

terminus::terminus(pn_terminus_t *p, pn_link_t* l) : wrapper<pn_terminus_t>(p), link_(l) {}

terminus::type_t terminus::type() {
    return (type_t) pn_terminus_get_type(get());
}

void terminus::type(type_t type) {
    pn_terminus_set_type(get(), (pn_terminus_type_t) type);
}

terminus::expiry_policy_t terminus::expiry_policy() {
    return (expiry_policy_t) pn_terminus_get_type(get());
}

void terminus::expiry_policy(expiry_policy_t policy) {
    pn_terminus_set_expiry_policy(get(), (pn_expiry_policy_t) policy);
}

terminus::distribution_mode_t terminus::distribution_mode() {
    return (distribution_mode_t) pn_terminus_get_type(get());
}

void terminus::distribution_mode(distribution_mode_t mode) {
    pn_terminus_set_distribution_mode(get(), (pn_distribution_mode_t) mode);
}

std::string terminus::address() {
    const char *addr = pn_terminus_get_address(get());
    return addr ? std::string(addr) : std::string();
}

void terminus::address(const std::string &addr) {
    pn_terminus_set_address(get(), addr.c_str());
}

bool terminus::is_dynamic() {
    return (type_t) pn_terminus_is_dynamic(get());
}

void terminus::dynamic(bool d) {
    pn_terminus_set_dynamic(get(), d);
}

}
