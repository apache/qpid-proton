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

#include <ostream>

namespace proton {

value::value() : data_(data::create()) {}

value::value(const value& x) : data_(data::create()) { data_ = x.data_; }

value& value::operator=(const value& x) { data_ = x.data_; return *this; }

void value::clear() { data_.clear(); }

bool value::empty() const { return data_.empty(); }

class encoder value::encoder() { clear(); return data_.encoder(); }

class decoder value::decoder() const { data_.decoder().rewind(); return data_.decoder(); }

type_id value::type() const { return decoder().type(); }

bool value::operator==(const value& x) const { return data_ == x.data_; }

bool value::operator<(const value& x) const { return data_ < x.data_; }

std::ostream& operator<<(std::ostream& o, const value& v) {
    // pn_inspect prints strings with quotes which is not normal in C++.
    switch (v.type()) {
      case STRING:
      case SYMBOL:
        return o << v.get<std::string>();
      default:
        return o << v.data_;
    }
}

}
