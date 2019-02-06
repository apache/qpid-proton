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
#include "proton/internal/data.hpp"
#include "proton/value.hpp"
#include "proton/types.hpp"
#include "proton/scalar.hpp"
#include "proton/error.hpp"

#include <ostream>
#include <sstream>

namespace proton {

using codec::decoder;
using codec::encoder;
using codec::start;

value::value() {}
value::value(pn_data_t *d) { data_ = make_wrapper(d); }
value::value(const value& x) { *this = x; }
#if PN_CPP_HAS_RVALUE_REFERENCES
value::value(value&& x) { swap(*this, x); }
value& value::operator=(value&& x) { swap(*this, x); return *this; }
#endif

value& value::operator=(const value& x) {
    if (this != &x) {
        if (x.empty())
            clear();
        else
            data().copy(x.data_);
    }
    return *this;
}

void swap(value& x, value& y) { std::swap(x.data_, y.data_); }

void value::clear() { if (!!data_) data_.clear(); }

namespace internal {

// On demand
internal::data& value_base::data() {
    if (!data_)
        data_ = internal::data::create();
    return data_;
}

}

type_id value::type() const {
    return (!data_ || data_.empty()) ? NULL_TYPE : codec::decoder(*this).next_type();
}

bool value::empty() const { return type() == NULL_TYPE; }

namespace {

// Compare nodes, return -1 if a<b, 0 if a==b, +1 if a>b
// Forward-declare so we can use it recursively.
int compare_next(decoder& a, decoder& b);

template <class T> int compare(const T& a, const T& b) {
    if (a < b) return -1;
    else if (a > b) return +1;
    else return 0;
}

int compare_container(decoder& a, decoder& b) {
    start sa, sb;
    a >> sa;
    b >> sb;
    // Compare described vs. not-described.
    int cmp = compare(sa.is_described, sb.is_described);
    if (cmp) return cmp;
    // Lexical sort (including descriptor if there is one)
    size_t min_size = std::min(sa.size, sb.size) + size_t(sa.is_described);
    for (size_t i = 0; i < min_size; ++i) {
        cmp = compare_next(a, b);
        if (cmp) return cmp;
    }
    return compare(sa.size, sb.size);
}

template <class T> int compare_simple(decoder& a, decoder& b) {
    T va = T();
    T vb = T();
    a >> va;
    b >> vb;
    return compare(va, vb);
}

int compare_next(decoder& a, decoder& b) {
    // Sort by type_id first.
    type_id ta = a.next_type(), tb = b.next_type();
    int cmp = compare(ta, tb);
    if (cmp) return cmp;

    switch (ta) {
      case NULL_TYPE: return 0;
      case ARRAY:
      case LIST:
      case MAP:
      case DESCRIBED:
        return compare_container(a, b);
      case BOOLEAN: return compare_simple<bool>(a, b);
      case UBYTE: return compare_simple<uint8_t>(a, b);
      case BYTE: return compare_simple<int8_t>(a, b);
      case USHORT: return compare_simple<uint16_t>(a, b);
      case SHORT: return compare_simple<int16_t>(a, b);
      case UINT: return compare_simple<uint32_t>(a, b);
      case INT: return compare_simple<int32_t>(a, b);
      case ULONG: return compare_simple<uint64_t>(a, b);
      case LONG: return compare_simple<int64_t>(a, b);
      case CHAR: return compare_simple<wchar_t>(a, b);
      case TIMESTAMP: return compare_simple<timestamp>(a, b);
      case FLOAT: return compare_simple<float>(a, b);
      case DOUBLE: return compare_simple<double>(a, b);
      case DECIMAL32: return compare_simple<decimal32>(a, b);
      case DECIMAL64: return compare_simple<decimal64>(a, b);
      case DECIMAL128: return compare_simple<decimal128>(a, b);
      case UUID: return compare_simple<uuid>(a, b);
      case BINARY: return compare_simple<binary>(a, b);
      case STRING: return compare_simple<std::string>(a, b);
      case SYMBOL: return compare_simple<symbol>(a, b);
    }
// Avoid unreached diagnostic from clang
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
    // Invalid but equal type_id, treat as equal.
    return 0;
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

int compare(const value& x, const value& y) {
    decoder a(x), b(y);
    internal::state_guard s1(a), s2(b);
    a.rewind();
    b.rewind();
    while (a.more() && b.more()) {
        int cmp = compare_next(a, b);
        if (cmp != 0) return cmp;
    }
    if (b.more()) return -1;
    if (a.more()) return 1;
    return 0;
}

} // namespace

bool operator==(const value& x, const value& y) {
    if (x.empty() && y.empty()) return true;
    if (x.empty() || y.empty()) return false;
    return compare(x, y) == 0;
}

bool operator<(const value& x, const value& y) {
    if (x.empty() && y.empty()) return false;
    if (x.empty()) return true; // empty is < !empty
    return compare(x, y) < 0;
}

std::ostream& operator<<(std::ostream& o, const value& x) {
    if (type_id_is_scalar(x.type()))
        return o << proton::get<scalar>(x); // Print as a scalar
    // Use pn_inspect for complex types.
    proton::decoder d(x);
    return o << d;
}

std::string to_string(const value& x) {
    std::ostringstream os;
    os << std::boolalpha << x;
    return os.str();
}

void value::reset(pn_data_t *d) { data_ = make_wrapper(d); }

} // namespace proton
