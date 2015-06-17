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

#include "proton/Data.hpp"
#include "proton/codec.h"
#include "proton_bits.hpp"
#include <utility>

namespace proton {

Data::Data() : data(pn_data(0)), own_(true) {}

Data::Data(pn_data_t* p) : data(p), own_(false) { }

Data::Data(const Data& x) : data(pn_data(0)), own_(true) { *this = x; }

Data::~Data() { if (own_ && data) pn_data_free(data); }

void Data::view(pn_data_t* newData) {
    if (data && own_) pn_data_free(data);
    data = newData;
    own_ = false;
}

void Data::swap(Data& x) {
    std::swap(data, x.data);
    std::swap(own_, x.own_);
}

Data& Data::operator=(const Data& x) {
    if (this != &x) {
        if (!own_) {
            data = pn_data(pn_data_size(x.data));
            own_ = true;
        } else {
            clear();
        }
        pn_data_copy(data, x.data);
    }
    return *this;
}

void Data::clear() { pn_data_clear(data); }

bool Data::empty() const { return pn_data_size(data) == 0; }

std::ostream& operator<<(std::ostream& o, const Data& d) { return o << PnObject(d.data); }

}
