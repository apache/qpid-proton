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
#include "proton/scalar.hpp"
#include "proton/symbol.hpp"
#include "proton/timestamp.hpp"
#include "proton/type_traits.hpp"
#include "proton/uuid.hpp"

#include <ostream>

namespace proton {

scalar::scalar() { atom_.type = PN_NULL; }
scalar::scalar(const pn_atom_t& a) { set(a); }
scalar::scalar(const scalar& x) { set(x.atom_); }

scalar& scalar::operator=(const scalar& x) {
    if (this != &x)
        set(x.atom_);
    return *this;
}

type_id scalar::type() const { return type_id(atom_.type); }

bool scalar::empty() const { return type() == NULL_TYPE; }

void scalar::set(const binary& x, pn_type_t t) {
    atom_.type = t;
    bytes_ = x;
    atom_.u.as_bytes = pn_bytes(bytes_);
}

void scalar::set(const pn_atom_t& atom) {
    if (type_id_is_string_like(type_id(atom.type)))
        set(bin(atom.u.as_bytes), atom.type);
    else
        atom_ = atom;
}

scalar::scalar(bool x) { *this = x; }
scalar::scalar(uint8_t x) { *this = x; }
scalar::scalar(int8_t x) { *this = x; }
scalar::scalar(uint16_t x) { *this = x; }
scalar::scalar(int16_t x) { *this = x; }
scalar::scalar(uint32_t x) { *this = x; }
scalar::scalar(int32_t x) { *this = x; }
scalar::scalar(uint64_t x) { *this = x; }
scalar::scalar(int64_t x) { *this = x; }
scalar::scalar(wchar_t x) { *this = x; }
scalar::scalar(float x) { *this = x; }
scalar::scalar(double x) { *this = x; }
scalar::scalar(timestamp x) { *this = x; }
scalar::scalar(const decimal32& x) { *this = x; }
scalar::scalar(const decimal64& x) { *this = x; }
scalar::scalar(const decimal128& x) { *this = x; }
scalar::scalar(const uuid& x) { *this = x; }
scalar::scalar(const std::string& x) { *this = x; }
scalar::scalar(const symbol& x) { *this = x; }
scalar::scalar(const binary& x) { *this = x; }
scalar::scalar(const char* x) { *this = x; }

scalar& scalar::operator=(bool x) { atom_.u.as_bool = x; atom_.type = PN_BOOL; return *this; }
scalar& scalar::operator=(uint8_t x) { atom_.u.as_ubyte = x; atom_.type = PN_UBYTE; return *this; }
scalar& scalar::operator=(int8_t x) { atom_.u.as_byte = x; atom_.type = PN_BYTE; return *this; }
scalar& scalar::operator=(uint16_t x) { atom_.u.as_ushort = x; atom_.type = PN_USHORT; return *this; }
scalar& scalar::operator=(int16_t x) { atom_.u.as_short = x; atom_.type = PN_SHORT; return *this; }
scalar& scalar::operator=(uint32_t x) { atom_.u.as_uint = x; atom_.type = PN_UINT; return *this; }
scalar& scalar::operator=(int32_t x) { atom_.u.as_int = x; atom_.type = PN_INT; return *this; }
scalar& scalar::operator=(uint64_t x) { atom_.u.as_ulong = x; atom_.type = PN_ULONG; return *this; }
scalar& scalar::operator=(int64_t x) { atom_.u.as_long = x; atom_.type = PN_LONG; return *this; }
scalar& scalar::operator=(wchar_t x) { atom_.u.as_char = x; atom_.type = PN_CHAR; return *this; }
scalar& scalar::operator=(float x) { atom_.u.as_float = x; atom_.type = PN_FLOAT; return *this; }
scalar& scalar::operator=(double x) { atom_.u.as_double = x; atom_.type = PN_DOUBLE; return *this; }
scalar& scalar::operator=(timestamp x) { atom_.u.as_timestamp = x.ms(); atom_.type = PN_TIMESTAMP; return *this; }

scalar& scalar::operator=(const decimal32& x) {
    byte_copy(atom_.u.as_decimal32, x);
    atom_.type = PN_DECIMAL32;
    return *this;
}

scalar& scalar::operator=(const decimal64& x) {
    byte_copy(atom_.u.as_decimal64, x);
    atom_.type = PN_DECIMAL64;
    return *this;
}

scalar& scalar::operator=(const decimal128& x) {
    byte_copy(atom_.u.as_decimal128, x);
    atom_.type = PN_DECIMAL128;
    return *this;
}

scalar& scalar::operator=(const uuid& x) {
    byte_copy(atom_.u.as_uuid, x);
    atom_.type = PN_UUID;
    return *this;
}

scalar& scalar::operator=(const std::string& x) { set(binary(x), PN_STRING); return *this; }
scalar& scalar::operator=(const symbol& x) { set(binary(x), PN_SYMBOL); return *this; }
scalar& scalar::operator=(const binary& x) { set(x, PN_BINARY); return *this; }
scalar& scalar::operator=(const char* x) { set(binary(std::string(x)), PN_STRING); return *this; }

void scalar::ok(pn_type_t t) const {
    if (atom_.type != t) throw make_conversion_error(type_id(t), type());
}

void scalar::get(bool& x) const { ok(PN_BOOL); x = atom_.u.as_bool; }
void scalar::get(uint8_t& x) const { ok(PN_UBYTE); x = atom_.u.as_ubyte; }
void scalar::get(int8_t& x) const { ok(PN_BYTE); x = atom_.u.as_byte; }
void scalar::get(uint16_t& x) const { ok(PN_USHORT); x = atom_.u.as_ushort; }
void scalar::get(int16_t& x) const { ok(PN_SHORT); x = atom_.u.as_short; }
void scalar::get(uint32_t& x) const { ok(PN_UINT); x = atom_.u.as_uint; }
void scalar::get(int32_t& x) const { ok(PN_INT); x = atom_.u.as_int; }
void scalar::get(wchar_t& x) const { ok(PN_CHAR); x = wchar_t(atom_.u.as_char); }
void scalar::get(uint64_t& x) const { ok(PN_ULONG); x = atom_.u.as_ulong; }
void scalar::get(int64_t& x) const { ok(PN_LONG); x = atom_.u.as_long; }
void scalar::get(timestamp& x) const { ok(PN_TIMESTAMP); x = atom_.u.as_timestamp; }
void scalar::get(float& x) const { ok(PN_FLOAT); x = atom_.u.as_float; }
void scalar::get(double& x) const { ok(PN_DOUBLE); x = atom_.u.as_double; }
void scalar::get(decimal32& x) const { ok(PN_DECIMAL32); byte_copy(x, atom_.u.as_decimal32); }
void scalar::get(decimal64& x) const { ok(PN_DECIMAL64); byte_copy(x, atom_.u.as_decimal64); }
void scalar::get(decimal128& x) const { ok(PN_DECIMAL128); byte_copy(x, atom_.u.as_decimal128); }
void scalar::get(uuid& x) const { ok(PN_UUID); byte_copy(x, atom_.u.as_uuid); }
void scalar::get(std::string& x) const {
    ok(PN_STRING);
    x = bytes_.str();
}
void scalar::get(symbol& x) const { ok(PN_SYMBOL); x = symbol(bytes_.str()); }
void scalar::get(binary& x) const { ok(PN_BINARY); x = bytes_; }

int64_t scalar::as_int() const {
    if (type_id_is_floating_point(type()))
        return int64_t(as_double());
    switch (atom_.type) {
      case PN_BOOL: return atom_.u.as_bool;
      case PN_UBYTE: return atom_.u.as_ubyte;
      case PN_BYTE: return atom_.u.as_byte;
      case PN_USHORT: return atom_.u.as_ushort;
      case PN_SHORT: return atom_.u.as_short;
      case PN_UINT: return atom_.u.as_uint;
      case PN_INT: return atom_.u.as_int;
      case PN_CHAR: return atom_.u.as_char;
      case PN_ULONG: return int64_t(atom_.u.as_ulong);
      case PN_LONG: return atom_.u.as_long;
      case PN_TIMESTAMP: return atom_.u.as_timestamp;
      default: throw make_conversion_error(LONG, type());
    }
}

uint64_t scalar::as_uint() const {
    if  (!type_id_is_integral(type()))
        throw make_conversion_error(ULONG, type());
    return uint64_t(as_int());
}

double scalar::as_double() const {
    if (type_id_is_integral(type())) {
        return type_id_is_signed(type()) ? double(as_int()) : double(as_uint());
    }
    switch (atom_.type) {
      case PN_DOUBLE: return atom_.u.as_double;
      case PN_FLOAT: return atom_.u.as_float;
      default: throw make_conversion_error(DOUBLE, type());
    }
}

std::string scalar::as_string() const {
    if (type_id_is_string_like(type()))
        return bytes_.str();
    throw make_conversion_error(STRING, type());
}

namespace {

template <class T, class F> T type_switch(const scalar& a, F f) {
    switch(a.type()) {
      case BOOLEAN: return f(a.get<bool>());
      case UBYTE: return f(a.get<uint8_t>());
      case BYTE: return f(a.get<int8_t>());
      case USHORT: return f(a.get<uint16_t>());
      case SHORT: return f(a.get<int16_t>());
      case UINT: return f(a.get<uint32_t>());
      case INT: return f(a.get<int32_t>());
      case CHAR: return f(a.get<wchar_t>());
      case ULONG: return f(a.get<uint64_t>());
      case LONG: return f(a.get<int64_t>());
      case TIMESTAMP: return f(a.get<timestamp>());
      case FLOAT: return f(a.get<float>());
      case DOUBLE: return f(a.get<double>());
      case DECIMAL32: return f(a.get<decimal32>());
      case DECIMAL64: return f(a.get<decimal64>());
      case DECIMAL128: return f(a.get<decimal128>());
      case UUID: return f(a.get<uuid>());
      case BINARY: return f(a.get<binary>());
      case STRING: return f(a.get<std::string>());
      case SYMBOL: return f(a.get<symbol>());
      default:
        throw std::logic_error("bad proton::scalar type");
    }
}

struct equal_op {
    const scalar& a;
    equal_op(const scalar& a_) : a(a_) {}
    template<class T> bool operator()(T x) { return x == a.get<T>(); }
};

struct less_op {
    const scalar& a;
    less_op(const scalar& a_) : a(a_) {}
    template<class T> bool operator()(T x) { return x < a.get<T>(); }
};

struct ostream_op {
    std::ostream& o;
    ostream_op(std::ostream& o_) : o(o_) {}
    template<class T> std::ostream& operator()(T x) { return o << x; }
};

} // namespace

bool operator==(const scalar& x, const scalar& y) {
    if (x.type() != y.type()) return false;
    if (x.empty()) return true;
    return type_switch<bool>(x, equal_op(y));
}

bool operator<(const scalar& x, const scalar& y) {
    if (x.type() != y.type()) return x.type() < y.type();
    if (x.empty()) return false;
    return type_switch<bool>(x, less_op(y));
}

std::ostream& operator<<(std::ostream& o, const scalar& a) {
    if (a.empty()) return o << "<null>";
    return type_switch<std::ostream&>(a, ostream_op(o));
}

} // namespace proton
