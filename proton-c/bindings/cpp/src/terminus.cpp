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

terminus::type_t terminus::type() const {
    return (type_t) pn_terminus_get_type(pn_cast(this));
}

void terminus::type(type_t type) {
    pn_terminus_set_type(pn_cast(this), (pn_terminus_type_t) type);
}

terminus::expiry_policy_t terminus::expiry_policy() const {
    return (expiry_policy_t) pn_terminus_get_type(pn_cast(this));
}

void terminus::expiry_policy(expiry_policy_t policy) {
    pn_terminus_set_expiry_policy(pn_cast(this), (pn_expiry_policy_t) policy);
}

terminus::distribution_mode_t terminus::distribution_mode() const {
    return (distribution_mode_t) pn_terminus_get_type(pn_cast(this));
}

void terminus::distribution_mode(distribution_mode_t mode) {
    pn_terminus_set_distribution_mode(pn_cast(this), (pn_distribution_mode_t) mode);
}

std::string terminus::address() const {
    const char *addr = pn_terminus_get_address(pn_cast(this));
    return addr ? std::string(addr) : std::string();
}

void terminus::address(const std::string &addr) {
    pn_terminus_set_address(pn_cast(this), addr.c_str());
}

bool terminus::dynamic() const {
    return (type_t) pn_terminus_is_dynamic(pn_cast(this));
}

void terminus::dynamic(bool d) {
    pn_terminus_set_dynamic(pn_cast(this), d);
}

}
