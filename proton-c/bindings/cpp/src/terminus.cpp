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

#include "proton_bits.hpp"

#include "proton/link.hpp"
#include "proton/link.h"

#include <limits>

namespace proton {

terminus::terminus(pn_terminus_t* t) :
    object_(t), properties_(pn_terminus_properties(t)), filter_(pn_terminus_filter(t)), parent_(0)
{}

enum expiry_policy terminus::expiry_policy() const {
    return (enum expiry_policy)pn_terminus_get_expiry_policy(object_);
}

void terminus::expiry_policy(enum expiry_policy policy) {
    pn_terminus_set_expiry_policy(object_, pn_expiry_policy_t(policy));
}

duration terminus::timeout() const {
    return duration::SECOND * pn_terminus_get_timeout(object_);
}

void terminus::timeout(duration d) {
    uint32_t seconds = 0;
    if (d == duration::FOREVER)
        seconds = std::numeric_limits<uint32_t>::max();
    else if (d != duration::IMMEDIATE) {
        uint64_t x = d.milliseconds();
        if ((std::numeric_limits<uint64_t>::max() - x) <= 500)
            seconds = std::numeric_limits<uint32_t>::max();
        else {
            x = (x + 500) / 1000;
            seconds = x < std::numeric_limits<uint32_t>::max() ? x : std::numeric_limits<uint32_t>::max();
        }
    }
    pn_terminus_set_timeout(object_, seconds);
}

enum distribution_mode terminus::distribution_mode() const {
    return (enum distribution_mode)pn_terminus_get_distribution_mode(object_);
}

void terminus::distribution_mode(enum distribution_mode mode) {
    pn_terminus_set_distribution_mode(object_, pn_distribution_mode_t(mode));
}

enum durability_mode terminus::durability_mode() {
    return (enum durability_mode) pn_terminus_get_durability(object_);
}

void terminus::durability_mode(enum durability_mode d) {
    pn_terminus_set_durability(object_, (pn_durability_t) d);
}

std::string terminus::address() const {
    return str(pn_terminus_get_address(object_));
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
