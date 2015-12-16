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

#include "proton/atom.hpp"
#include "proton/type_traits.hpp"

#include <ostream>

namespace proton {

atom::atom() { atom_.type = PN_NULL; }

type_id atom::type() const { return type_id(atom_.type); }
bool atom::empty() const { return type() == NULL_TYPE; }

atom::atom(bool x) { atom_.u.as_bool = x; atom_.type = PN_BOOL; }
atom::atom(uint8_t x) { atom_.u.as_ubyte = x; atom_.type = PN_UBYTE; }
atom::atom(int8_t x) { atom_.u.as_byte = x; atom_.type = PN_BYTE; }
atom::atom(uint16_t x) { atom_.u.as_ushort = x; atom_.type = PN_USHORT; }
atom::atom(int16_t x) { atom_.u.as_short = x; atom_.type = PN_SHORT; }
atom::atom(uint32_t x) { atom_.u.as_uint = x; atom_.type = PN_UINT; }
atom::atom(int32_t x) { atom_.u.as_int = x; atom_.type = PN_INT; }
atom::atom(uint64_t x) { atom_.u.as_ulong = x; atom_.type = PN_ULONG; }
atom::atom(int64_t x) { atom_.u.as_long = x; atom_.type = PN_LONG; }
atom::atom(wchar_t x) { atom_.u.as_char = x; atom_.type = PN_CHAR; }
atom::atom(float x) { atom_.u.as_float = x; atom_.type = PN_FLOAT; }
atom::atom(double x) { atom_.u.as_double = x; atom_.type = PN_DOUBLE; }
atom::atom(amqp_timestamp x) { atom_.u.as_timestamp = x; atom_.type = PN_TIMESTAMP; }
atom::atom(const amqp_decimal32& x) { atom_.u.as_decimal32 = x; atom_.type = PN_DECIMAL32; }
atom::atom(const amqp_decimal64& x) { atom_.u.as_decimal64 = x; atom_.type = PN_DECIMAL64; }
atom::atom(const amqp_decimal128& x) { atom_.u.as_decimal128 = x; atom_.type = PN_DECIMAL128; }
atom::atom(const amqp_uuid& x) { atom_.u.as_uuid = x; atom_.type = PN_UUID; }

void atom::set(const std::string& x) { str_ = x; atom_.u.as_bytes = pn_bytes(str_); }
atom::atom(const amqp_string& x) { set(x); atom_.type = PN_STRING; }
atom::atom(const amqp_symbol& x) { set(x); atom_.type = PN_SYMBOL; }
atom::atom(const amqp_binary& x) { set(x); atom_.type = PN_BINARY; }
atom::atom(const std::string& x) { *this = amqp_string(x); }
atom::atom(const char* x) { *this = amqp_string(x); }

void atom::ok(pn_type_t t) const {
    if (atom_.type != t) throw type_mismatch(type_id(t), type());
}

void atom::get(bool& x) const { ok(PN_BOOL); x = atom_.u.as_bool; }
void atom::get(uint8_t& x) const { ok(PN_UBYTE); x = atom_.u.as_ubyte; }
void atom::get(int8_t& x) const { ok(PN_BYTE); x = atom_.u.as_byte; }
void atom::get(uint16_t& x) const { ok(PN_USHORT); x = atom_.u.as_ushort; }
void atom::get(int16_t& x) const { ok(PN_SHORT); x = atom_.u.as_short; }
void atom::get(uint32_t& x) const { ok(PN_UINT); x = atom_.u.as_uint; }
void atom::get(int32_t& x) const { ok(PN_INT); x = atom_.u.as_int; }
void atom::get(wchar_t& x) const { ok(PN_CHAR); x = atom_.u.as_char; }
void atom::get(uint64_t& x) const { ok(PN_ULONG); x = atom_.u.as_ulong; }
void atom::get(int64_t& x) const { ok(PN_LONG); x = atom_.u.as_long; }
void atom::get(amqp_timestamp& x) const { ok(PN_TIMESTAMP); x = atom_.u.as_timestamp; }
void atom::get(float& x) const { ok(PN_FLOAT); x = atom_.u.as_float; }
void atom::get(double& x) const { ok(PN_DOUBLE); x = atom_.u.as_double; }
void atom::get(amqp_decimal32& x) const { ok(PN_DECIMAL32); x = atom_.u.as_decimal32; }
void atom::get(amqp_decimal64& x) const { ok(PN_DECIMAL64); x = atom_.u.as_decimal64; }
void atom::get(amqp_decimal128& x) const { ok(PN_DECIMAL128); x = atom_.u.as_decimal128; }
void atom::get(amqp_uuid& x) const { ok(PN_UUID); x = atom_.u.as_uuid; }
void atom::get(amqp_string& x) const { ok(PN_STRING); x = str_; }
void atom::get(amqp_symbol& x) const { ok(PN_SYMBOL); x = str_; }
void atom::get(amqp_binary& x) const { ok(PN_BINARY); x = str_; }
void atom::get(std::string& x) const { x = get<amqp_string>(); }

int64_t atom::as_int() const {
    if (type_id_floating_point(type())) return as_double();
    switch (atom_.type) {
      case PN_BOOL: return atom_.u.as_bool;
      case PN_UBYTE: return atom_.u.as_ubyte;
      case PN_BYTE: return atom_.u.as_byte;
      case PN_USHORT: return atom_.u.as_ushort;
      case PN_SHORT: return atom_.u.as_short;
      case PN_UINT: return atom_.u.as_uint;
      case PN_INT: return atom_.u.as_int;
      case PN_CHAR: return atom_.u.as_char;
      case PN_ULONG: return atom_.u.as_ulong;
      case PN_LONG: return atom_.u.as_long;
      default: throw type_mismatch(LONG, type(), "cannot convert");
    }
}

uint64_t atom::as_uint() const {
    if  (!type_id_integral(type()))
        throw type_mismatch(ULONG, type(), "cannot convert");
    return uint64_t(as_int());
}

double atom::as_double() const {
    if (type_id_integral(type())) {
        return type_id_signed(type()) ? double(as_int()) : double(as_uint());
    }
    switch (atom_.type) {
      case PN_DOUBLE: return atom_.u.as_double;
      case PN_FLOAT: return atom_.u.as_float;
      default: throw type_mismatch(DOUBLE, type(), "cannot convert");
    }
}

std::string atom::as_string() const {
    if (type_id_string_like(type()))
        return str_;
    throw type_mismatch(DOUBLE, type(), "cannot convert");
}


namespace {
template <class T, class F> T type_switch(const atom& a, F f) {
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
        throw error("bad atom type");
    }
}

struct equal_op {
    const atom& a;
    equal_op(const atom& a_) : a(a_) {}
    template<class T> bool operator()(T x) { return x == a.get<T>(); }
};

struct less_op {
    const atom& a;
    less_op(const atom& a_) : a(a_) {}
    template<class T> bool operator()(T x) { return x < a.get<T>(); }
};

struct ostream_op {
    std::ostream& o;
    ostream_op(std::ostream& o_) : o(o_) {}
    template<class T> std::ostream& operator()(T x) { return o << x; }
};

} // namespace

bool atom::operator==(const atom& x) const {
    return type_switch<bool>(*this, equal_op(x));
}

bool atom::operator<(const atom& x) const {
    return type_switch<bool>(*this, less_op(x));
}

std::ostream& operator<<(std::ostream& o, const atom& a) {
    return type_switch<std::ostream&>(a, ostream_op(o));
}

}
