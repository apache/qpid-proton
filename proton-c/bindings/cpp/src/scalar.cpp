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

#include "proton/scalar.hpp"
#include "proton/type_traits.hpp"

#include <ostream>

namespace proton {

scalar::scalar() { atom_.type = PN_NULL; }

type_id scalar::type() const { return type_id(atom_.type); }
bool scalar::empty() const { return type() == NULL_TYPE; }

scalar::scalar(bool x) { atom_.u.as_bool = x; atom_.type = PN_BOOL; }
scalar::scalar(uint8_t x) { atom_.u.as_ubyte = x; atom_.type = PN_UBYTE; }
scalar::scalar(int8_t x) { atom_.u.as_byte = x; atom_.type = PN_BYTE; }
scalar::scalar(uint16_t x) { atom_.u.as_ushort = x; atom_.type = PN_USHORT; }
scalar::scalar(int16_t x) { atom_.u.as_short = x; atom_.type = PN_SHORT; }
scalar::scalar(uint32_t x) { atom_.u.as_uint = x; atom_.type = PN_UINT; }
scalar::scalar(int32_t x) { atom_.u.as_int = x; atom_.type = PN_INT; }
scalar::scalar(uint64_t x) { atom_.u.as_ulong = x; atom_.type = PN_ULONG; }
scalar::scalar(int64_t x) { atom_.u.as_long = x; atom_.type = PN_LONG; }
scalar::scalar(wchar_t x) { atom_.u.as_char = pn_char_t(x); atom_.type = PN_CHAR; }
scalar::scalar(float x) { atom_.u.as_float = x; atom_.type = PN_FLOAT; }
scalar::scalar(double x) { atom_.u.as_double = x; atom_.type = PN_DOUBLE; }
scalar::scalar(amqp_timestamp x) { atom_.u.as_timestamp = x; atom_.type = PN_TIMESTAMP; }
scalar::scalar(const amqp_decimal32& x) { atom_.u.as_decimal32 = x; atom_.type = PN_DECIMAL32; }
scalar::scalar(const amqp_decimal64& x) { atom_.u.as_decimal64 = x; atom_.type = PN_DECIMAL64; }
scalar::scalar(const amqp_decimal128& x) { atom_.u.as_decimal128 = x; atom_.type = PN_DECIMAL128; }
scalar::scalar(const amqp_uuid& x) { atom_.u.as_uuid = x; atom_.type = PN_UUID; }

void scalar::set(const std::string& x) { str_ = x; atom_.u.as_bytes = pn_bytes(str_); }
scalar::scalar(const amqp_string& x) { set(x); atom_.type = PN_STRING; }
scalar::scalar(const amqp_symbol& x) { set(x); atom_.type = PN_SYMBOL; }
scalar::scalar(const amqp_binary& x) { set(x); atom_.type = PN_BINARY; }
scalar::scalar(const std::string& x) { *this = amqp_string(x); }
scalar::scalar(const char* x) { *this = amqp_string(x); }

void scalar::ok(pn_type_t t) const {
    if (atom_.type != t) throw type_mismatch(type_id(t), type());
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
void scalar::get(amqp_timestamp& x) const { ok(PN_TIMESTAMP); x = atom_.u.as_timestamp; }
void scalar::get(float& x) const { ok(PN_FLOAT); x = atom_.u.as_float; }
void scalar::get(double& x) const { ok(PN_DOUBLE); x = atom_.u.as_double; }
void scalar::get(amqp_decimal32& x) const { ok(PN_DECIMAL32); x = atom_.u.as_decimal32; }
void scalar::get(amqp_decimal64& x) const { ok(PN_DECIMAL64); x = atom_.u.as_decimal64; }
void scalar::get(amqp_decimal128& x) const { ok(PN_DECIMAL128); x = atom_.u.as_decimal128; }
void scalar::get(amqp_uuid& x) const { ok(PN_UUID); x = atom_.u.as_uuid; }
void scalar::get(amqp_string& x) const { ok(PN_STRING); x = str_; }
void scalar::get(amqp_symbol& x) const { ok(PN_SYMBOL); x = str_; }
void scalar::get(amqp_binary& x) const { ok(PN_BINARY); x = str_; }
void scalar::get(std::string& x) const { x = get<amqp_string>(); }

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
      default: throw type_mismatch(LONG, type(), "cannot convert");
    }
}

uint64_t scalar::as_uint() const {
    if  (!type_id_is_integral(type()))
        throw type_mismatch(ULONG, type(), "cannot convert");
    return uint64_t(as_int());
}

double scalar::as_double() const {
    if (type_id_is_integral(type())) {
        return type_id_is_signed(type()) ? double(as_int()) : double(as_uint());
    }
    switch (atom_.type) {
      case PN_DOUBLE: return atom_.u.as_double;
      case PN_FLOAT: return atom_.u.as_float;
      default: throw type_mismatch(DOUBLE, type(), "cannot convert");
    }
}

std::string scalar::as_string() const {
    if (type_id_is_string_like(type()))
        return str_;
    throw type_mismatch(DOUBLE, type(), "cannot convert");
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
      case TIMESTAMP: return f(a.get<amqp_timestamp>());
      case FLOAT: return f(a.get<float>());
      case DOUBLE: return f(a.get<double>());
      case DECIMAL32: return f(a.get<amqp_decimal32>());
      case DECIMAL64: return f(a.get<amqp_decimal64>());
      case DECIMAL128: return f(a.get<amqp_decimal128>());
      case UUID: return f(a.get<amqp_uuid>());
      case BINARY: return f(a.get<amqp_binary>());
      case STRING: return f(a.get<amqp_string>());
      case SYMBOL: return f(a.get<amqp_symbol>());
      default:
        throw error("bad scalar type");
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

bool scalar::operator==(const scalar& x) const {
    return type_switch<bool>(*this, equal_op(x));
}

bool scalar::operator<(const scalar& x) const {
    return type_switch<bool>(*this, less_op(x));
}

std::ostream& operator<<(std::ostream& o, const scalar& a) {
    return type_switch<std::ostream&>(a, ostream_op(o));
}

}
