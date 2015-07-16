/*
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
 */

#include "proton/data.hpp"
#include "proton/codec.h"
#include "proton_bits.hpp"
#include <utility>

namespace proton {

data::data() : data_(::pn_data(0)), own_(true) {}

data::data(pn_data_t* p) : data_(p), own_(false) { }

data::data(const data& x) : data_(::pn_data(0)), own_(true) { *this = x; }

data::~data() { if (own_ && data_) ::pn_data_free(data_); }

void data::view(pn_data_t* new_data) {
    if (data_ && own_) pn_data_free(data_);
    data_ = new_data;
    own_ = false;
}

void data::swap(data& x) {
    std::swap(data_, x.data_);
    std::swap(own_, x.own_);
}

data& data::operator=(const data& x) {
    if (this != &x) {
        if (!own_) {
            data_ = ::pn_data(::pn_data_size(x.data_));
            own_ = true;
        } else {
            clear();
        }
        ::pn_data_copy(data_, x.data_);
    }
    return *this;
}

void data::clear() { ::pn_data_clear(data_); }

void data::rewind() { ::pn_data_rewind(data_); }

bool data::empty() const { return ::pn_data_size(data_) == 0; }

std::ostream& operator<<(std::ostream& o, const data& d) { return o << pn_object(d.data_); }

}
