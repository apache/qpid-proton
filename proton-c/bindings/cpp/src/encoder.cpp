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
#include "types_internal.hpp"
#include "msg.hpp"

#include <proton/annotation_key.hpp>
#include <proton/binary.hpp>
#include <proton/data.hpp>
#include <proton/decimal.hpp>
#include <proton/encoder.hpp>
#include <proton/message_id.hpp>
#include <proton/symbol.hpp>
#include <proton/timestamp.hpp>
#include <proton/value.hpp>

#include <proton/codec.h>

#include <algorithm>

namespace proton {
namespace codec {

void encoder::check(long result) {
    if (result < 0)
        throw conversion_error(error_str(pn_data_error(pn_object()), result));
}


encoder::encoder(value& v) : data(v.data()) {
    clear();
}

bool encoder::encode(char* buffer, size_t& size) {
    state_guard sg(*this); // In case of error
    ssize_t result = pn_data_encode(pn_object(), buffer, size);
    if (result == PN_OVERFLOW) {
        result = pn_data_encoded_size(pn_object());
        if (result >= 0) {
            size = size_t(result);
            return false;
        }
    }
    check(result);
    size = size_t(result);
    sg.cancel();                // Don't restore state, all is well.
    pn_data_clear(pn_object());
    return true;
}

void encoder::encode(std::string& s) {
    s.resize(std::max(s.capacity(), size_t(1))); // Use full capacity, ensure not empty
	size_t size = s.size();
    if (!encode(&s[0], size)) {
        s.resize(size);
        encode(&s[0], size);
    }
}

std::string encoder::encode() {
    std::string s;
    encode(s);
    return s;
}

encoder& encoder::operator<<(const start& s) {
    switch (s.type) {
      case ARRAY: pn_data_put_array(pn_object(), s.is_described, pn_type_t(s.element)); break;
      case MAP: pn_data_put_map(pn_object()); break;
      case LIST: pn_data_put_list(pn_object()); break;
      case DESCRIBED: pn_data_put_described(pn_object()); break;
      default:
        throw conversion_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(pn_object());
    return *this;
}

encoder& encoder::operator<<(const finish&) {
    pn_data_exit(pn_object());
    return *this;
}

namespace {

template <class T, class U> T convert(const U &x) { return x; }
template <> pn_uuid_t convert(const uuid& x) { pn_uuid_t y; byte_copy(y, x); return  y; }
template <> pn_decimal32_t convert(const decimal32 &x) { pn_decimal32_t y; byte_copy(y, x); return  y; }
template <> pn_decimal64_t convert(const decimal64 &x) { pn_decimal64_t y; byte_copy(y, x); return  y; }
template <> pn_decimal128_t convert(const decimal128 &x) { pn_decimal128_t y; byte_copy(y, x); return  y; }

int pn_data_put_amqp_string(pn_data_t *d, const std::string& x) { return pn_data_put_string(d, pn_bytes(x)); }
int pn_data_put_amqp_binary(pn_data_t *d, const binary& x) { return pn_data_put_binary(d, pn_bytes(x)); }
int pn_data_put_amqp_symbol(pn_data_t *d, const symbol& x) { return pn_data_put_symbol(d, pn_bytes(x)); }
} // namespace

template <class T, class U>
encoder& encoder::insert(const T& x, int (*put)(pn_data_t*, U)) {
    state_guard sg(*this);         // Save state in case of error.
    check(put(pn_object(), convert<U>(x)));
    sg.cancel();                // Don't restore state, all is good.
    return *this;
}

encoder& encoder::operator<<(bool x) { return insert(x, pn_data_put_bool); }
encoder& encoder::operator<<(uint8_t x) { return insert(x, pn_data_put_ubyte); }
encoder& encoder::operator<<(int8_t x) { return insert(x, pn_data_put_byte); }
encoder& encoder::operator<<(uint16_t x) { return insert(x, pn_data_put_ushort); }
encoder& encoder::operator<<(int16_t x) { return insert(x, pn_data_put_short); }
encoder& encoder::operator<<(uint32_t x) { return insert(x, pn_data_put_uint); }
encoder& encoder::operator<<(int32_t x) { return insert(x, pn_data_put_int); }
encoder& encoder::operator<<(wchar_t x) { return insert(x, pn_data_put_char); }
encoder& encoder::operator<<(uint64_t x) { return insert(x, pn_data_put_ulong); }
encoder& encoder::operator<<(int64_t x) { return insert(x, pn_data_put_long); }
encoder& encoder::operator<<(timestamp x) { return insert(x.ms(), pn_data_put_timestamp); }
encoder& encoder::operator<<(float x) { return insert(x, pn_data_put_float); }
encoder& encoder::operator<<(double x) { return insert(x, pn_data_put_double); }
encoder& encoder::operator<<(decimal32 x) { return insert(x, pn_data_put_decimal32); }
encoder& encoder::operator<<(decimal64 x) { return insert(x, pn_data_put_decimal64); }
encoder& encoder::operator<<(decimal128 x) { return insert(x, pn_data_put_decimal128); }
encoder& encoder::operator<<(const uuid& x) { return insert(x, pn_data_put_uuid); }
encoder& encoder::operator<<(const std::string& x) { return insert(x, pn_data_put_amqp_string); }
encoder& encoder::operator<<(const symbol& x) { return insert(x, pn_data_put_amqp_symbol); }
encoder& encoder::operator<<(const binary& x) { return insert(x, pn_data_put_amqp_binary); }

encoder& encoder::operator<<(const scalar& x) { return insert(x.atom_, pn_data_put_atom); }

encoder& encoder::operator<<(exact_cref<value> x) {
    if (*this == x.ref.data_)
        throw conversion_error("cannot insert into self");
    if (x.ref.empty())
        pn_data_put_null(pn_object());
    decoder d(x.ref);                 // Rewind
    check(append(d));
    return *this;
}

} // codec
} // proton
