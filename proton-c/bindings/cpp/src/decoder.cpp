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

#include <proton/annotation_key.hpp>
#include <proton/binary.hpp>
#include <proton/data.hpp>
#include <proton/decimal.hpp>
#include <proton/encoder.hpp>
#include <proton/message_id.hpp>
#include <proton/scalar.hpp>
#include <proton/symbol.hpp>
#include <proton/timestamp.hpp>
#include <proton/value.hpp>

#include "proton_bits.hpp"
#include "types_internal.hpp"
#include "msg.hpp"

#include <proton/codec.h>

namespace proton {
namespace codec {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the decoder.
 */

decoder::decoder(const value_base& v, bool exact) : data(v.data()), exact_(exact) { rewind(); }

namespace {
template <class T> T check(T result) {
    if (result < 0)
        throw conversion_error(error_str(result));
    return result;
}
}

void decoder::decode(const char* i, size_t size) {
    state_guard sg(*this);
    const char* end = i + size;
    while (i < end)
        i += check(pn_data_decode(pn_object(), i, size_t(end - i)));
}

void decoder::decode(const std::string& s) { decode(s.data(), s.size()); }

bool decoder::more() {
    state_guard sg(*this);
    return next();
}

type_id decoder::pre_get() {
    if (!next()) throw conversion_error("no more data");
    type_id t = type_id(pn_data_type(pn_object()));
    if (t < 0) throw conversion_error("invalid data");
    return t;
}

namespace {

template <class T, class U> void assign(T& x, const U& y) { x = y; }
void assign(uuid& x, const pn_uuid_t y) { byte_copy(x, y); }
void assign(decimal32& x, const pn_decimal32_t y) { byte_copy(x, y); }
void assign(decimal64& x, const pn_decimal64_t y)  { byte_copy(x, y); }
void assign(decimal128& x, const pn_decimal128_t y) { byte_copy(x, y); }
void assign(symbol& x, const pn_bytes_t y) { x = str(y); }
void assign(binary& x, const pn_bytes_t y) { x = bin(y); }

} // namespace


// Simple extract with no type conversion.
template <class T, class U> decoder& decoder::extract(T& x, U (*get)(pn_data_t*)) {
    state_guard sg(*this);
    assert_type_equal(internal::type_id_of<T>::value, pre_get());
    assign(x, get(pn_object()));
    sg.cancel();                // No error, cancel the reset.
    return *this;
}

type_id decoder::next_type() {
    state_guard sg(*this);
    return pre_get();
}

decoder& decoder::operator>>(start& s) {
    state_guard sg(*this);
    s.type = pre_get();
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(pn_object());
        s.element = type_id(pn_data_get_array_type(pn_object())); s.is_described = pn_data_is_array_described(pn_object());
        break;
      case LIST:
        s.size = pn_data_get_list(pn_object());
        break;
      case MAP:
        s.size = pn_data_get_map(pn_object());
        break;
      case DESCRIBED:
        s.is_described = true;
        s.size = 1;
        break;
      default:
        throw conversion_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(pn_object());
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(const finish&) {
    pn_data_exit(pn_object());
    return *this;
}

decoder& decoder::operator>>(null&) {
    state_guard sg(*this);
    assert_type_equal(NULL_TYPE, pre_get());
    return *this;
}

decoder& decoder::operator>>(value_base& x) {
    if (*this == x.data_)
        throw conversion_error("extract into self");
    data d = x.data();
    d.clear();
    narrow();
    try {
        check(d.appendn(*this, 1));
        widen();
    } catch(...) {
        widen();
        throw;
    }
    next();
    return *this;
}

decoder& decoder::operator>>(message_id& x) {
    state_guard sg(*this);
    type_id got = pre_get();
    if (got != ULONG && got != UUID && got != BINARY && got != STRING)
        throw conversion_error(
            msg() << "expected one of ulong, uuid, binary or string but found " << got);
    x.set(pn_data_get_atom(pn_object()));
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(annotation_key& x) {
    state_guard sg(*this);
    type_id got = pre_get();
    if (got != ULONG && got != SYMBOL)
        throw conversion_error(msg() << "expected one of ulong or symbol but found " << got);
    x.set(pn_data_get_atom(pn_object()));
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(scalar& x) {
    state_guard sg(*this);
    type_id got = pre_get();
    if (!type_id_is_scalar(got))
        throw conversion_error("expected scalar, found "+type_name(got));
    x.set(pn_data_get_atom(pn_object()));
    sg.cancel();                // No error, no rewind
    return *this;
}

decoder& decoder::operator>>(bool &x) { return extract(x, pn_data_get_bool); }

decoder& decoder::operator>>(uint8_t &x)  { return extract(x, pn_data_get_ubyte); }

decoder& decoder::operator>>(int8_t &x) { return extract(x, pn_data_get_byte); }

decoder& decoder::operator>>(uint16_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(USHORT, tid);
    switch (tid) {
      case UBYTE: x = pn_data_get_ubyte(pn_object()); break;
      case USHORT: x = pn_data_get_ushort(pn_object()); break;
      default: assert_type_equal(USHORT, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(int16_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(SHORT, tid);
    switch (tid) {
      case BYTE: x = pn_data_get_byte(pn_object()); break;
      case SHORT: x = pn_data_get_short(pn_object()); break;
      default: assert_type_equal(SHORT, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(uint32_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(UINT, tid);
    switch (tid) {
      case UBYTE: x = pn_data_get_ubyte(pn_object()); break;
      case USHORT: x = pn_data_get_ushort(pn_object()); break;
      case UINT: x = pn_data_get_uint(pn_object()); break;
      default: assert_type_equal(UINT, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(int32_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(INT, tid);
    switch (tid) {
      case BYTE: x = pn_data_get_byte(pn_object()); break;
      case SHORT: x = pn_data_get_short(pn_object()); break;
      case INT: x = pn_data_get_int(pn_object()); break;
      default: assert_type_equal(INT, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(uint64_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(ULONG, tid);
    switch (tid) {
      case UBYTE: x = pn_data_get_ubyte(pn_object()); break;
      case USHORT: x = pn_data_get_ushort(pn_object()); break;
      case UINT: x = pn_data_get_uint(pn_object()); break;
      case ULONG: x = pn_data_get_ulong(pn_object()); break;
      default: assert_type_equal(ULONG, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(int64_t &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(LONG, tid);
    switch (tid) {
      case BYTE: x = pn_data_get_byte(pn_object()); break;
      case SHORT: x = pn_data_get_short(pn_object()); break;
      case INT: x = pn_data_get_int(pn_object()); break;
      case LONG: x = pn_data_get_long(pn_object()); break;
      default: assert_type_equal(LONG, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(wchar_t &x) { return extract(x, pn_data_get_char); }

decoder& decoder::operator>>(timestamp &x) { return extract(x, pn_data_get_timestamp); }

decoder& decoder::operator>>(float &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(FLOAT, tid);
    switch (tid) {
      case FLOAT: x = pn_data_get_float(pn_object()); break;
      case DOUBLE: x = float(pn_data_get_double(pn_object())); break;
      default: assert_type_equal(FLOAT, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(double &x) {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(DOUBLE, tid);
    switch (tid) {
      case FLOAT: x = pn_data_get_float(pn_object()); break;
      case DOUBLE: x = pn_data_get_double(pn_object()); break;
      default: assert_type_equal(DOUBLE, tid);
    }
    sg.cancel();
    return *this;
}

decoder& decoder::operator>>(decimal32 &x) { return extract(x, pn_data_get_decimal32); }
decoder& decoder::operator>>(decimal64 &x) { return extract(x, pn_data_get_decimal64); }
decoder& decoder::operator>>(decimal128 &x)  { return extract(x, pn_data_get_decimal128); }

decoder& decoder::operator>>(uuid &x)  { return extract(x, pn_data_get_uuid); }
decoder& decoder::operator>>(binary &x)  { return extract(x, pn_data_get_binary); }
decoder& decoder::operator>>(symbol &x)  { return extract(x, pn_data_get_symbol); }

decoder& decoder::operator>>(std::string &x)  {
    state_guard sg(*this);
    type_id tid = pre_get();
    if (exact_) assert_type_equal(STRING, tid);
    switch (tid) {
      case STRING: x = str(pn_data_get_string(pn_object())); break;
      case SYMBOL: x = str(pn_data_get_symbol(pn_object())); break;
      default: assert_type_equal(STRING, tid);
    }
    sg.cancel();
    return *this;
}

} // codec
} // proton
