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

#include "proton/codec/vector.hpp"
#include "proton/source.hpp"
#include "proton/source_options.hpp"
#include "proton/target.hpp"
#include "proton/target_options.hpp"

#include "proton_bits.hpp"

#include <limits>

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

namespace {

    void timeout(terminus &t, duration d) {
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
      pn_terminus_set_timeout(unwrap(t), seconds);
    }
}

namespace {

// Options common to sources and targets

void node_address(terminus &t, option<std::string> &addr, option<bool> &dynamic, option<bool> &anonymous) {
    if (dynamic.set && dynamic.value) {
        pn_terminus_set_dynamic(unwrap(t), true);
        pn_terminus_set_address(unwrap(t), NULL);
    } else if (anonymous.set && anonymous.value) {
        pn_terminus_set_address(unwrap(t), NULL);
    } else if (addr.set) {
        pn_terminus_set_address(unwrap(t), addr.value.c_str());
    }
}

void node_durability(terminus &t, option<enum terminus::durability_mode> &mode) {
    if (mode.set) pn_terminus_set_durability(unwrap(t), pn_durability_t(mode.value));
}

void node_expiry(terminus &t, option<enum terminus::expiry_policy> &policy, option<duration> &d) {
    if (policy.set) pn_terminus_set_expiry_policy(unwrap(t), pn_expiry_policy_t(policy.value));
    if (d.set) timeout(t, d.value);
}

}


class source_options::impl {
  public:
    option<std::string> address;
    option<bool> dynamic;
    option<bool> anonymous;
    option<enum source::durability_mode> durability_mode;
    option<duration> timeout;
    option<enum source::expiry_policy> expiry_policy;
    option<enum source::distribution_mode> distribution_mode;
    option<source::filter_map> filters;
    option<std::vector<symbol> > capabilities;

    void apply(source& s) {
        node_address(s, address, dynamic, anonymous);
        node_durability(s, durability_mode);
        node_expiry(s, expiry_policy, timeout);
        if (distribution_mode.set)
          pn_terminus_set_distribution_mode(unwrap(s), pn_distribution_mode_t(distribution_mode.value));
        if (filters.set && !filters.value.empty()) {
            // Applied at most once via source_option.  No need to clear.
            value(pn_terminus_filter(unwrap(s))) = filters.value;
        }
        if (capabilities.set) {
            value(pn_terminus_capabilities(unwrap(s))) = capabilities.value;
        }
    }
};

source_options::source_options() : impl_(new impl()) {}
source_options::source_options(const source_options& x) : impl_(new impl()) {
    *this = x;
}
source_options::~source_options() {}

source_options& source_options::operator=(const source_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

source_options& source_options::address(const std::string &addr) { impl_->address = addr; return *this; }
source_options& source_options::dynamic(bool b) { impl_->dynamic = b; return *this; }
source_options& source_options::anonymous(bool b) { impl_->anonymous = b; return *this; }
source_options& source_options::durability_mode(enum source::durability_mode m) { impl_->durability_mode = m; return *this; }
source_options& source_options::timeout(duration d) { impl_->timeout = d; return *this; }
source_options& source_options::expiry_policy(enum source::expiry_policy m) { impl_->expiry_policy = m; return *this; }
source_options& source_options::distribution_mode(enum source::distribution_mode m) { impl_->distribution_mode = m; return *this; }
source_options& source_options::filters(const source::filter_map &map) { impl_->filters = map; return *this; }
source_options& source_options::capabilities(const std::vector<symbol>& c) { impl_->capabilities = c; return *this; }

void source_options::apply(source& s) const { impl_->apply(s); }

// TARGET

class target_options::impl {
  public:
    option<std::string> address;
    option<bool> dynamic;
    option<bool> anonymous;
    option<enum target::durability_mode> durability_mode;
    option<duration> timeout;
    option<enum target::expiry_policy> expiry_policy;
    option<std::vector<symbol> > capabilities;

    void apply(target& t) {
        node_address(t, address, dynamic, anonymous);
        node_durability(t, durability_mode);
        node_expiry(t, expiry_policy, timeout);
        if (capabilities.set) {
            value(pn_terminus_capabilities(unwrap(t))) = capabilities.value;
        }
    }
};

target_options::target_options() : impl_(new impl()) {}
target_options::target_options(const target_options& x) : impl_(new impl()) {
    *this = x;
}
target_options::~target_options() {}

target_options& target_options::operator=(const target_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

target_options& target_options::address(const std::string &addr) { impl_->address = addr; return *this; }
target_options& target_options::dynamic(bool b) { impl_->dynamic = b; return *this; }
target_options& target_options::anonymous(bool b) { impl_->anonymous = b; return *this; }
target_options& target_options::durability_mode(enum target::durability_mode m) { impl_->durability_mode = m; return *this; }
target_options& target_options::timeout(duration d) { impl_->timeout = d; return *this; }
target_options& target_options::expiry_policy(enum target::expiry_policy m) { impl_->expiry_policy = m; return *this; }
target_options& target_options::capabilities(const std::vector<symbol>& c) { impl_->capabilities = c; return *this; }

void target_options::apply(target& s) const { impl_->apply(s); }



} // namespace proton
