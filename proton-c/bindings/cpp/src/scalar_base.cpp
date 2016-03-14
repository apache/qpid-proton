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

#include "msg.hpp"
#include "types_internal.hpp"

#include "proton/binary.hpp"
#include "proton/decimal.hpp"
#include "proton/scalar_base.hpp"
#include "proton/symbol.hpp"
#include "proton/timestamp.hpp"
#include "proton/type_traits.hpp"
#include "proton/uuid.hpp"

#include <ostream>

namespace proton {

scalar_base::scalar_base() { atom_.type = PN_NULL; }
scalar_base::scalar_base(const pn_atom_t& a) { set(a); }
scalar_base::scalar_base(const scalar_base& x) { set(x.atom_); }

scalar_base& scalar_base::operator=(const scalar_base& x) {
    if (this != &x)
        set(x.atom_);
    return *this;
}

type_id scalar_base::type() const { return type_id(atom_.type); }

void scalar_base::set(const binary& x, pn_type_t t) {
    atom_.type = t;
    bytes_ = x;
    atom_.u.as_bytes = pn_bytes(bytes_);
}

void scalar_base::set(const pn_atom_t& atom) {
    if (type_id_is_string_like(type_id(atom.type))) {
        set(bin(atom.u.as_bytes), atom.type);
    } else {
        atom_ = atom;
        bytes_.clear();
    }
}

void scalar_base::put_(bool x) { atom_.u.as_bool = x; atom_.type = PN_BOOL; }
void scalar_base::put_(uint8_t x) { atom_.u.as_ubyte = x; atom_.type = PN_UBYTE; }
void scalar_base::put_(int8_t x) { atom_.u.as_byte = x; atom_.type = PN_BYTE; }
void scalar_base::put_(uint16_t x) { atom_.u.as_ushort = x; atom_.type = PN_USHORT; }
void scalar_base::put_(int16_t x) { atom_.u.as_short = x; atom_.type = PN_SHORT; }
void scalar_base::put_(uint32_t x) { atom_.u.as_uint = x; atom_.type = PN_UINT; }
void scalar_base::put_(int32_t x) { atom_.u.as_int = x; atom_.type = PN_INT; }
void scalar_base::put_(uint64_t x) { atom_.u.as_ulong = x; atom_.type = PN_ULONG; }
void scalar_base::put_(int64_t x) { atom_.u.as_long = x; atom_.type = PN_LONG; }
void scalar_base::put_(wchar_t x) { atom_.u.as_char = x; atom_.type = PN_CHAR; }
void scalar_base::put_(float x) { atom_.u.as_float = x; atom_.type = PN_FLOAT; }
void scalar_base::put_(double x) { atom_.u.as_double = x; atom_.type = PN_DOUBLE; }
void scalar_base::put_(timestamp x) { atom_.u.as_timestamp = x.ms(); atom_.type = PN_TIMESTAMP; }
void scalar_base::put_(const decimal32& x) { byte_copy(atom_.u.as_decimal32, x); atom_.type = PN_DECIMAL32;; }
void scalar_base::put_(const decimal64& x) { byte_copy(atom_.u.as_decimal64, x); atom_.type = PN_DECIMAL64; }
void scalar_base::put_(const decimal128& x) { byte_copy(atom_.u.as_decimal128, x); atom_.type = PN_DECIMAL128; }
void scalar_base::put_(const uuid& x) { byte_copy(atom_.u.as_uuid, x); atom_.type = PN_UUID; }
void scalar_base::put_(const std::string& x) { set(binary(x), PN_STRING); }
void scalar_base::put_(const symbol& x) { set(binary(x), PN_SYMBOL); }
void scalar_base::put_(const binary& x) { set(x, PN_BINARY); }
void scalar_base::put_(const char* x) { set(binary(std::string(x)), PN_STRING); }
void scalar_base::put_(const null&) { atom_.type = PN_NULL; }

void scalar_base::ok(pn_type_t t) const {
    if (atom_.type != t) throw make_conversion_error(type_id(t), type());
}

void scalar_base::get_(bool& x) const { ok(PN_BOOL); x = atom_.u.as_bool; }
void scalar_base::get_(uint8_t& x) const { ok(PN_UBYTE); x = atom_.u.as_ubyte; }
void scalar_base::get_(int8_t& x) const { ok(PN_BYTE); x = atom_.u.as_byte; }
void scalar_base::get_(uint16_t& x) const { ok(PN_USHORT); x = atom_.u.as_ushort; }
void scalar_base::get_(int16_t& x) const { ok(PN_SHORT); x = atom_.u.as_short; }
void scalar_base::get_(uint32_t& x) const { ok(PN_UINT); x = atom_.u.as_uint; }
void scalar_base::get_(int32_t& x) const { ok(PN_INT); x = atom_.u.as_int; }
void scalar_base::get_(wchar_t& x) const { ok(PN_CHAR); x = wchar_t(atom_.u.as_char); }
void scalar_base::get_(uint64_t& x) const { ok(PN_ULONG); x = atom_.u.as_ulong; }
void scalar_base::get_(int64_t& x) const { ok(PN_LONG); x = atom_.u.as_long; }
void scalar_base::get_(timestamp& x) const { ok(PN_TIMESTAMP); x = atom_.u.as_timestamp; }
void scalar_base::get_(float& x) const { ok(PN_FLOAT); x = atom_.u.as_float; }
void scalar_base::get_(double& x) const { ok(PN_DOUBLE); x = atom_.u.as_double; }
void scalar_base::get_(decimal32& x) const { ok(PN_DECIMAL32); byte_copy(x, atom_.u.as_decimal32); }
void scalar_base::get_(decimal64& x) const { ok(PN_DECIMAL64); byte_copy(x, atom_.u.as_decimal64); }
void scalar_base::get_(decimal128& x) const { ok(PN_DECIMAL128); byte_copy(x, atom_.u.as_decimal128); }
void scalar_base::get_(uuid& x) const { ok(PN_UUID); byte_copy(x, atom_.u.as_uuid); }
void scalar_base::get_(std::string& x) const { ok(PN_STRING); x = std::string(bytes_.begin(), bytes_.end()); }
void scalar_base::get_(symbol& x) const { ok(PN_SYMBOL); x = symbol(bytes_.begin(), bytes_.end()); }
void scalar_base::get_(binary& x) const { ok(PN_BINARY); x = bytes_; }
void scalar_base::get_(null&) const { ok(PN_NULL); }

int64_t scalar_base::as_int() const { return internal::coerce<int64_t>(*this); }

uint64_t scalar_base::as_uint() const { return internal::coerce<uint64_t>(*this); }

double scalar_base::as_double() const { return internal::coerce<double>(*this); }

std::string scalar_base::as_string() const { return internal::coerce<std::string>(*this); }

namespace {

struct equal_op {
    const scalar_base& x;
    equal_op(const scalar_base& s) : x(s) {}
    template<class T> bool operator()(const T& y) { return (x.get<T>() == y); }
};

struct less_op {
    const scalar_base& x;
    less_op(const scalar_base& s) : x(s) {}
    template<class T> bool operator()(const T& y) { return (y < x.get<T>()); }
};

struct ostream_op {
    std::ostream& o;
    ostream_op(std::ostream& o_) : o(o_) {}
    template<class T> std::ostream& operator()(const T& x) { return o << x; }
};

} // namespace

bool operator==(const scalar_base& x, const scalar_base& y) {
    if (x.type() != y.type()) return false;
    if (x.type() == NULL_TYPE) return true;
    return internal::visit<bool>(x, equal_op(y));
}

bool operator<(const scalar_base& x, const scalar_base& y) {
    if (x.type() != y.type()) return x.type() < y.type();
    if (x.type() == NULL_TYPE) return false;
    return internal::visit<bool>(x, less_op(y));
}

std::ostream& operator<<(std::ostream& o, const scalar_base& s) {
    if (s.type() == NULL_TYPE) return o << "<null>";
    return internal::visit<std::ostream&>(s, ostream_op(o));
}

} // namespace proton
