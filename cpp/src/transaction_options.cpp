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

#include "proton/transaction_options.hpp"

#include "proton/session.hpp"

#include "contexts.hpp"
#include "proton_bits.hpp"

namespace proton {

namespace {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

}

class transaction_options::impl {
  public:
    option<bool> auto_modify_on_abort;

    void apply(session& s) {
        auto& session_context = session_context::get(unwrap(s));
        auto& transaction_context = session_context.transaction_context_;
    
        if (auto_modify_on_abort.set) transaction_context->auto_modify_on_abort = auto_modify_on_abort.value;
    }

    void update(const impl& o) { auto_modify_on_abort.update(o.auto_modify_on_abort); }
};

transaction_options::transaction_options() : impl_(new impl()) {}
transaction_options::transaction_options(const transaction_options& o) : impl_(new impl()) { *this = o; }
transaction_options::~transaction_options() = default;

transaction_options& transaction_options::operator=(const transaction_options& o) {
    *impl_ = *o.impl_;
    return *this;
}

void transaction_options::update(const transaction_options& o) { impl_->update(*o.impl_); }

transaction_options& transaction_options::auto_modify_on_abort(bool b) {
    impl_->auto_modify_on_abort = b;
    return *this;
}

void transaction_options::apply(session& s) const { impl_->apply(s); }

} // namespace proton
