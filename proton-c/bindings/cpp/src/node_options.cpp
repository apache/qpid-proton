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

#include "proton/source_options.hpp"
#include "proton/source.hpp"
#include "proton/target_options.hpp"
#include "proton/target.hpp"

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
    void address(terminus &t, const std::string &s) { pn_terminus_set_address(unwrap(t), s.c_str()); }
    void set_dynamic(terminus &t, bool b) { pn_terminus_set_dynamic(unwrap(t), b); }
    void durability_mode(terminus &t, enum durability_mode m) { pn_terminus_set_durability(unwrap(t), pn_durability_t(m)); }
    void expiry_policy(terminus &t, enum expiry_policy p) { pn_terminus_set_expiry_policy(unwrap(t), pn_expiry_policy_t(p)); }
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

void node_address(terminus &t, option<std::string> &addr, option<bool> &dynamic) {
    if (dynamic.set && dynamic.value) {
        set_dynamic(t, true);
        // Ignore any addr value for dynamic.
        return;
    }
    if (addr.set) {
        address(t, addr.value);
    }
}

void node_durability(terminus &t, option<enum durability_mode> &mode) {
    if (mode.set) durability_mode(t, mode.value);
}

void node_expiry(terminus &t, option<enum expiry_policy> &policy, option<duration> &d) {
    if (policy.set) expiry_policy(t, policy.value);
    if (d.set) timeout(t, d.value);
}

}


class source_options::impl {
  public:
    option<std::string> address;
    option<bool> dynamic;
    option<enum durability_mode> durability_mode;
    option<duration> timeout;
    option<enum expiry_policy> expiry_policy;
    option<enum distribution_mode> distribution_mode;
    option<source::filter_map> filters;

    void apply(source& s) {
        node_address(s, address, dynamic);
        node_durability(s, durability_mode);
        node_expiry(s, expiry_policy, timeout);
        if (distribution_mode.set)
          pn_terminus_set_distribution_mode(unwrap(s), pn_distribution_mode_t(distribution_mode.value));
        if (filters.set && !filters.value.empty()) {
            // Applied at most once via source_option.  No need to clear.
            codec::encoder e(pn_terminus_filter(unwrap(s)));
            e << filters.value;
        }
    }

    void update(const impl& x) {
        address.update(x.address);
        dynamic.update(x.dynamic);
        durability_mode.update(x.durability_mode);
        timeout.update(x.timeout);
        expiry_policy.update(x.expiry_policy);
        distribution_mode.update(x.distribution_mode);
        filters.update(x.filters);
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

void source_options::update(const source_options& x) { impl_->update(*x.impl_); }

source_options& source_options::address(const std::string &addr) { impl_->address = addr; return *this; }
source_options& source_options::dynamic(bool b) { impl_->dynamic = b; return *this; }
source_options& source_options::durability_mode(enum durability_mode m) { impl_->durability_mode = m; return *this; }
source_options& source_options::timeout(duration d) { impl_->timeout = d; return *this; }
source_options& source_options::expiry_policy(enum expiry_policy m) { impl_->expiry_policy = m; return *this; }
source_options& source_options::distribution_mode(enum distribution_mode m) { impl_->distribution_mode = m; return *this; }
source_options& source_options::filters(const source::filter_map &map) { impl_->filters = map; return *this; }

void source_options::apply(source& s) const { impl_->apply(s); }

// TARGET

class target_options::impl {
  public:
    option<std::string> address;
    option<bool> dynamic;
    option<enum durability_mode> durability_mode;
    option<duration> timeout;
    option<enum expiry_policy> expiry_policy;

    void apply(target& t) {
        node_address(t, address, dynamic);
        node_durability(t, durability_mode);
        node_expiry(t, expiry_policy, timeout);
    }

    void update(const impl& x) {
        address.update(x.address);
        dynamic.update(x.dynamic);
        durability_mode.update(x.durability_mode);
        timeout.update(x.timeout);
        expiry_policy.update(x.expiry_policy);
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

void target_options::update(const target_options& x) { impl_->update(*x.impl_); }

target_options& target_options::address(const std::string &addr) { impl_->address = addr; return *this; }
target_options& target_options::dynamic(bool b) { impl_->dynamic = b; return *this; }
target_options& target_options::durability_mode(enum durability_mode m) { impl_->durability_mode = m; return *this; }
target_options& target_options::timeout(duration d) { impl_->timeout = d; return *this; }
target_options& target_options::expiry_policy(enum expiry_policy m) { impl_->expiry_policy = m; return *this; }

void target_options::apply(target& s) const { impl_->apply(s); }



} // namespace proton
