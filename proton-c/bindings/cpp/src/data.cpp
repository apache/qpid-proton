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

#include <proton/codec.h>

#include <ostream>

namespace proton {

data& data::operator=(const data& x) { ::pn_data_copy(pn_object(), x.pn_object()); return *this; }

void data::clear() { ::pn_data_clear(pn_object()); }

bool data::empty() const { return ::pn_data_size(pn_object()) == 0; }

uintptr_t data::point() const { return pn_data_point(pn_object()); }

void data::restore(uintptr_t h) { pn_data_restore(pn_object(), h); }

void data::narrow() { pn_data_narrow(pn_object()); }

void data::widen() { pn_data_widen(pn_object()); }

int data::append(data src) { return pn_data_append(pn_object(), src.pn_object());}

int data::appendn(data src, int limit) { return pn_data_appendn(pn_object(), src.pn_object(), limit);}

bool data::next() const { return pn_data_next(pn_object()); }


namespace {
struct save_state {
    data data_;
    uintptr_t handle;
    save_state(data d) : data_(d), handle(data_.point()) {}
    ~save_state() { if (!!data_) data_.restore(handle); }
};
}

std::ostream& operator<<(std::ostream& o, const data& d) {
    save_state s(d.pn_object());
    d.decoder().rewind();
    return o << inspectable(d.pn_object());
}

owned_object<pn_data_t> data::create() { return pn_data(0); }

encoder data::encoder() { return proton::encoder(pn_object()); }
decoder data::decoder() { return proton::decoder(pn_object()); }

type_id data::type() const { return decoder().type(); }

namespace {

// Compare nodes, return -1 if a<b, 0 if a==b, +1 if a>b
// Forward-declare so we can use it recursively.
int compare_next(data& a, data& b);

template <class T> int compare(const T& a, const T& b) {
    if (a < b) return -1;
    else if (a > b) return +1;
    else return 0;
}

int compare_container(data& a, data& b) {
    scope sa(a.decoder()), sb(b.decoder());
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

template <class T> int compare_simple(data& a, data& b) {
    T va = T();
    T vb = T();
    a.decoder() >> va;
    b.decoder() >> vb;
    return compare(va, vb);
}

int compare_next(data& a, data& b) {
    // Sort by type_id first.
    type_id ta = a.type(), tb = b.type();
    int cmp = compare(ta, tb);
    if (cmp) return cmp;

    switch (ta) {
      case NULL_: return 0;
      case ARRAY:
      case LIST:
      case MAP:
      case DESCRIBED:
        return compare_container(a, b);
      case BOOLEAN: return compare_simple<amqp_boolean>(a, b);
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

int compare(data& a, data& b) {
    save_state s1(a);
    save_state s2(b);
    a.decoder().rewind();
    b.decoder().rewind();
    while (a.decoder().more() && b.decoder().more()) {
        int cmp = compare_next(a, b);
        if (cmp != 0) return cmp;
    }
    if (b.decoder().more()) return -1;
    if (a.decoder().more()) return 1;
    return 0;
}
} // namespace

bool data::operator==(const data& x) const {
    return compare(const_cast<data&>(*this), const_cast<data&>(x)) == 0;
}
bool data::operator<(const data& x) const {
    return compare(const_cast<data&>(*this), const_cast<data&>(x)) < 0;
}

}
