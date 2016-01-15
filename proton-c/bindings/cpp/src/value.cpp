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

#include "proton_bits.hpp"
#include "proton/data.hpp"
#include "proton/value.hpp"
#include "proton/scalar.hpp"

#include <ostream>

namespace proton {

value::value() {}
value::value(const value& x) { *this = x; }
#if PN_HAS_CPP11
value::value(value&& x) { swap(*this, x); }
#endif

value& value::operator=(const value& x) {
    if (this != &x) {
        if (x.empty())
            clear();
        else
            encode() << x;
    }
    return *this;
}

void swap(value& x, value& y) { std::swap(x.data_, y.data_); }

void value::clear() { if (!!data_) data_.clear(); }

bool value::empty() const { return !data_ || data_.empty(); }

// On demand
inline data& value::data() const { if (!data_) data_ = proton::data::create(); return data_; }

class encoder value::encode() { clear(); return data().encoder(); }

class decoder value::decode() const { return data().decoder() >> rewind(); }

type_id value::type() const { return decode().type(); }

bool operator==(const value& x, const value& y) {
    if (x.empty() && y.empty()) return true;
    if (x.empty() || y.empty()) return false;
    return  x.data().equal(y.data());
}

bool operator<(const value& x, const value& y) {
    if (x.empty() && y.empty()) return false;
    if (x.empty()) return true; // empty is < !empty
    return x.data().less(y.data());
}

std::ostream& operator<<(std::ostream& o, const value& v) {
    if (v.empty())
        return o << "<empty>";
    // pn_inspect prints strings with quotes which is not normal in C++.
    switch (v.type()) {
      case STRING:
      case SYMBOL:
        return o << v.get<std::string>();
      default:
        return o << v.data();
    }
}

int64_t value::as_int() const { return get<scalar>().as_int(); }
uint64_t value::as_uint() const { return get<scalar>().as_uint(); }
double value::as_double() const { return get<scalar>().as_double(); }
std::string value::as_string() const { return get<scalar>().as_string(); }

}
