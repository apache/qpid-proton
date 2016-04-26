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

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

namespace internal {
class noderef {
    // friend class to handle private access on terminus.  TODO: change to newer single friend mechanism.
  public:
    static void address(terminus &t, const std::string &s) { t.address(s); }
    static void dynamic(terminus &t, bool b) { t.dynamic(b); }
    static void durability_mode(terminus &t, enum durability_mode m) { t.durability_mode(m); }
    static void expiry_policy(terminus &t, enum expiry_policy p) { t.expiry_policy(p); }
    static void timeout(terminus &t, duration d) { t.timeout(d); }
};
}

namespace {

// Options common to sources and targets

using internal::noderef;

void node_address(internal::terminus &t, option<std::string> &addr, option<bool> &dynamic) {
    if (dynamic.set && dynamic.value) {
        noderef::dynamic(t, true);
        // Ignore any addr value for dynamic.
        return;
    }
    if (addr.set) {
        noderef::address(t, addr.value);
    }
}

void node_durability(internal::terminus &t, option<enum durability_mode> &mode) {
    if (mode.set) noderef::durability_mode(t, mode.value);
}

void node_expiry(internal::terminus &t, option<enum expiry_policy> &policy, option<duration> &d) {
    if (policy.set) noderef::expiry_policy(t, policy.value);
    if (d.set) noderef::timeout(t, d.value);
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

    void apply(source& s) {
        node_address(s, address, dynamic);
        node_durability(s, durability_mode);
        node_expiry(s, expiry_policy, timeout);
        if (distribution_mode.set) s.distribution_mode(distribution_mode.value);
    }

    void update(const impl& x) {
        address.update(x.address);
        dynamic.update(x.dynamic);
        durability_mode.update(x.durability_mode);
        timeout.update(x.timeout);
        expiry_policy.update(x.expiry_policy);
        distribution_mode.update(x.distribution_mode);
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
