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

#include "proton/value.hpp"
#include "proton_bits.hpp"
#include <proton/codec.h>
#include <ostream>
#include <algorithm>

namespace proton {

value::value() { *this = amqp_null(); }
value::value(const value& v) { *this = v; }
value::~value() {}

value& value::operator=(const value& v) { values_ = v.values_; return *this; }

type_id value::type() const {
    const_cast<values&>(values_).rewind();
    return values_.type();
}

namespace {
template <class T> T check(T result) {
    if (result < 0)
        throw encode_error("encode: " + error_str(result));
    return result;
}
}

std::ostream& operator<<(std::ostream& o, const value& v) {
    return o << v.values_;
}

namespace {

// Compare nodes, return -1 if a<b, 0 if a==b, +1 if a>b
// Forward-declare so we can use it recursively.
int compare_next(values& a, values& b);

template <class T> int compare(const T& a, const T& b) {
    if (a < b) return -1;
    else if (a > b) return +1;
    else return 0;
}

int compare_container(values& a, values& b) {
    decoder::scope sa(a), sb(b);
    // Compare described vs. not-described.
    int cmp = compare(sa.is_described, sb.is_described);
    if (cmp) return cmp;
    // Lexical sort (including descriptor if there is one)
    size_t min_size = std::min(sa.size, sb.size) + int(sa.is_described);
    for (size_t i = 0; i < min_size; ++i) {
        cmp = compare_next(a, b);
        if (cmp) return cmp;
    }
    return compare(sa.size, sb.size);
}

template <class T> int compare_simple(values& a, values& b) {
    T va, vb;
    a >> va;
    b >> vb;
    return compare(va, vb);
}

int compare_next(values& a, values& b) {
    // Sort by type_id first.
    type_id ta = a.type(), tb = b.type();
    int cmp = compare(ta, tb);
    if (cmp) return cmp;

    switch (ta) {
      case NULl_: return 0;
      case ARRAY:
      case LIST:
      case MAP:
      case DESCRIBED:
        return compare_container(a, b);
      case BOOL: return compare_simple<amqp_bool>(a, b);
      case UBYTE: return compare_simple<amqp_ubyte>(a, b);
      case BYTE: return compare_simple<amqp_byte>(a, b);
      case USHORT: return compare_simple<amqp_ushort>(a, b);
      case SHORT: return compare_simple<amqp_short>(a, b);
      case UINT: return compare_simple<amqp_uint>(a, b);
      case INT: return compare_simple<amqp_int>(a, b);
      case CHAR: return compare_simple<amqp_char>(a, b);
      case ULONG: return compare_simple<amqp_ulong>(a, b);
      case LONG: return compare_simple<amqp_long>(a, b);
      case TIMESTAMP: return compare_simple<amqp_timestamp>(a, b);
      case FLOAT: return compare_simple<amqp_float>(a, b);
      case DOUBLE: return compare_simple<amqp_double>(a, b);
      case DECIMAL32: return compare_simple<amqp_decimal32>(a, b);
      case DECIMAL64: return compare_simple<amqp_decimal64>(a, b);
      case DECIMAL128: return compare_simple<amqp_decimal128>(a, b);
      case UUID: return compare_simple<amqp_uuid>(a, b);
      case BINARY: return compare_simple<amqp_binary>(a, b);
      case STRING: return compare_simple<amqp_string>(a, b);
      case SYMBOL: return compare_simple<amqp_symbol>(a, b);
    }
    // Invalid but equal type_id, treat as equal.
    return 0;
}

} // namespace

bool value::operator==(const value& v) const {
    values_.rewind();
    v.values_.rewind();
    return compare_next(values_, v.values_) == 0;
}

bool value::operator<(const value& v) const {
    values_.rewind();
    v.values_.rewind();
    return compare_next(values_, v.values_) < 0;
}

}
