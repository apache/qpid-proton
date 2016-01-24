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

terminus::terminus(pn_terminus_t* t) :
    object_(t), properties_(pn_terminus_properties(t)), filter_(pn_terminus_filter(t))
{}

enum terminus::type terminus::type() const {
    return (enum type)pn_terminus_get_type(object_);
}

void terminus::type(enum type type0) {
    pn_terminus_set_type(object_, pn_terminus_type_t(type0));
}

enum terminus::expiry_policy terminus::expiry_policy() const {
    return (enum expiry_policy)pn_terminus_get_expiry_policy(object_);
}

void terminus::expiry_policy(enum expiry_policy policy) {
    pn_terminus_set_expiry_policy(object_, pn_expiry_policy_t(policy));
}

uint32_t terminus::timeout() const {
    return pn_terminus_get_timeout(object_);
}

void terminus::timeout(uint32_t seconds) {
    pn_terminus_set_timeout(object_, seconds);
}

enum terminus::distribution_mode terminus::distribution_mode() const {
    return (enum distribution_mode)pn_terminus_get_distribution_mode(object_);
}

void terminus::distribution_mode(enum distribution_mode mode) {
    pn_terminus_set_distribution_mode(object_, pn_distribution_mode_t(mode));
}

enum terminus::durability terminus::durability() {
    return (enum durability) pn_terminus_get_durability(object_);
}

void terminus::durability(enum durability d) {
    pn_terminus_set_durability(object_, (pn_durability_t) d);
}

std::string terminus::address() const {
    const char *addr = pn_terminus_get_address(object_);
    return addr ? std::string(addr) : std::string();
}

void terminus::address(const std::string &addr) {
    pn_terminus_set_address(object_, addr.c_str());
}

bool terminus::dynamic() const {
    return pn_terminus_is_dynamic(object_);
}

void terminus::dynamic(bool d) {
    pn_terminus_set_dynamic(object_, d);
}

value& terminus::filter() { return filter_; }
const value& terminus::filter() const { return filter_; }


value& terminus::node_properties() { return properties_; }
const value& terminus::node_properties() const { return properties_; }

}
