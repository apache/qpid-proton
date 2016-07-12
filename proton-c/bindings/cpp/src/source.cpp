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

#include "proton/source.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#include "proton_bits.hpp"

namespace proton {

// Set parent_ non-null when the local terminus is authoritative and may need to be looked up.
source::source(pn_terminus_t *t) : terminus(make_wrapper(t)) {}

source::source(const sender& snd) : terminus(make_wrapper(pn_link_remote_source(unwrap(snd)))) { parent_ = unwrap(snd); }

source::source(const receiver& rcv) : terminus(make_wrapper(pn_link_remote_source(unwrap(rcv)))) {}

std::string source::address() const {
    pn_terminus_t *authoritative = object_;
    if (parent_ && pn_terminus_is_dynamic(object_))
        authoritative = pn_link_source(parent_);
    return str(pn_terminus_get_address(authoritative));
}

enum source::distribution_mode source::distribution_mode() const {
  return (enum distribution_mode)pn_terminus_get_distribution_mode(object_);
}

source::filter_map source::filters() const {
    codec::decoder d(make_wrapper(pn_terminus_filter(object_)));
    filter_map map;
    if (!d.empty()) {
        d.rewind();
        d >> map;
    }
    return map;
}

}
