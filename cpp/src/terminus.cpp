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

#include "proton/duration.hpp"
#include "proton/terminus.hpp"
#include "proton/value.hpp"
#include "proton/codec/vector.hpp"

#include "proton_bits.hpp"

namespace proton {

terminus::terminus(pn_terminus_t* t) :
    object_(t), parent_(0)
{}

enum terminus::expiry_policy terminus::expiry_policy() const {
    return (enum expiry_policy)pn_terminus_get_expiry_policy(object_);
}

duration terminus::timeout() const {
    return duration::SECOND * pn_terminus_get_timeout(object_);
}

enum terminus::durability_mode terminus::durability_mode() {
    return (enum durability_mode) pn_terminus_get_durability(object_);
}

bool terminus::dynamic() const {
    return pn_terminus_is_dynamic(object_);
}

bool terminus::anonymous() const {
    return pn_terminus_get_address(object_) == NULL;
}

value terminus::node_properties() const {
    return value(pn_terminus_properties(object_));
}

std::vector<symbol> terminus::capabilities() const {
    value caps(pn_terminus_capabilities(object_));
    return caps.empty() ? std::vector<symbol>() : caps.get<std::vector<symbol> >();
}

}
