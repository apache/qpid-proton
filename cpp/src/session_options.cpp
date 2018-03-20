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

#include "proton/session_options.hpp"
#include "proton/session.hpp"
#include "proton/connection.hpp"
#include "proton/container.hpp"

#include <proton/session.h>

#include "messaging_adapter.hpp"
#include "proactor_container_impl.hpp"
#include "proton_bits.hpp"

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

class session_options::impl {
  public:
    option<messaging_handler *> handler;

    void apply(session& s) {
        if (s.uninitialized()) {
            if (handler.set && handler.value) container::impl::set_handler(s, handler.value);
        }
    }

};

session_options::session_options() : impl_(new impl()) {}
session_options::session_options(const session_options& x) : impl_(new impl()) {
    *this = x;
}
session_options::~session_options() {}

session_options& session_options::operator=(const session_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

session_options& session_options::handler(class messaging_handler &h) { impl_->handler = &h; return *this; }

void session_options::apply(session& s) const { impl_->apply(s); }




} // namespace proton
