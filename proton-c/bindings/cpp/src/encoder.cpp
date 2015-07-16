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

#include "proton/encoder.hpp"
#include "proton/value.hpp"
#include <proton/codec.h>
#include "proton_bits.hpp"
#include "msg.hpp"

namespace proton {

encoder::encoder() {}
encoder::~encoder() {}

static const std::string prefix("encode: ");
encode_error::encode_error(const std::string& msg) throw() : error(prefix+msg) {}

namespace {
struct save_state {
    pn_data_t* data;
    pn_handle_t handle;
    save_state(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~save_state() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

void check(int result, pn_data_t* data) {
    if (result < 0)
        throw encode_error(error_str(pn_data_error(data), result));
}
}

bool encoder::encode(char* buffer, size_t& size) {
    save_state ss(data_);               // In case of error
    ssize_t result = pn_data_encode(data_, buffer, size);
    if (result == PN_OVERFLOW) {
        result = pn_data_encoded_size(data_);
        if (result >= 0) {
            size = result;
            return false;
        }
    }
    check(result, data_);
    size = result;
    ss.cancel();                // Don't restore state, all is well.
    pn_data_clear(data_);
    return true;
}

void encoder::encode(std::string& s) {
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

encoder& operator<<(encoder& e, const start& s) {
    switch (s.type) {
      case ARRAY: pn_data_put_array(e.data_, s.is_described, pn_type_t(s.element)); break;
      case MAP: pn_data_put_map(e.data_); break;
      case LIST: pn_data_put_list(e.data_); break;
      case DESCRIBED: pn_data_put_described(e.data_); break;
      default:
        throw encode_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(e.data_);
    return e;
}

encoder& operator<<(encoder& e, finish) {
    pn_data_exit(e.data_);
    return e;
}

namespace {
template <class T, class U>
encoder& insert(encoder& e, pn_data_t* data, T& value, int (*put)(pn_data_t*, U)) {
    save_state ss(data);         // Save state in case of error.
    check(put(data, value), data);
    ss.cancel();                // Don't restore state, all is good.
    return e;
}
}

encoder& operator<<(encoder& e, amqp_null) { pn_data_put_null(e.data_); return e; }
encoder& operator<<(encoder& e, amqp_bool value) { return insert(e, e.data_, value, pn_data_put_bool); }
encoder& operator<<(encoder& e, amqp_ubyte value) { return insert(e, e.data_, value, pn_data_put_ubyte); }
encoder& operator<<(encoder& e, amqp_byte value) { return insert(e, e.data_, value, pn_data_put_byte); }
encoder& operator<<(encoder& e, amqp_ushort value) { return insert(e, e.data_, value, pn_data_put_ushort); }
encoder& operator<<(encoder& e, amqp_short value) { return insert(e, e.data_, value, pn_data_put_short); }
encoder& operator<<(encoder& e, amqp_uint value) { return insert(e, e.data_, value, pn_data_put_uint); }
encoder& operator<<(encoder& e, amqp_int value) { return insert(e, e.data_, value, pn_data_put_int); }
encoder& operator<<(encoder& e, amqp_char value) { return insert(e, e.data_, value, pn_data_put_char); }
encoder& operator<<(encoder& e, amqp_ulong value) { return insert(e, e.data_, value, pn_data_put_ulong); }
encoder& operator<<(encoder& e, amqp_long value) { return insert(e, e.data_, value, pn_data_put_long); }
encoder& operator<<(encoder& e, amqp_timestamp value) { return insert(e, e.data_, value, pn_data_put_timestamp); }
encoder& operator<<(encoder& e, amqp_float value) { return insert(e, e.data_, value, pn_data_put_float); }
encoder& operator<<(encoder& e, amqp_double value) { return insert(e, e.data_, value, pn_data_put_double); }
encoder& operator<<(encoder& e, amqp_decimal32 value) { return insert(e, e.data_, value, pn_data_put_decimal32); }
encoder& operator<<(encoder& e, amqp_decimal64 value) { return insert(e, e.data_, value, pn_data_put_decimal64); }
encoder& operator<<(encoder& e, amqp_decimal128 value) { return insert(e, e.data_, value, pn_data_put_decimal128); }
encoder& operator<<(encoder& e, amqp_uuid value) { return insert(e, e.data_, value, pn_data_put_uuid); }
encoder& operator<<(encoder& e, amqp_string value) { return insert(e, e.data_, value, pn_data_put_string); }
encoder& operator<<(encoder& e, amqp_symbol value) { return insert(e, e.data_, value, pn_data_put_symbol); }
encoder& operator<<(encoder& e, amqp_binary value) { return insert(e, e.data_, value, pn_data_put_binary); }

encoder& operator<<(encoder& e, const value& v) {
    if (e.data_ == v.values_.data_) throw encode_error("cannot insert into self");
    check(pn_data_appendn(e.data_, v.values_.data_, 1), e.data_);
    return e;
}

encoder& operator<<(encoder& e, const values& v) {
    if (e.data_ == v.data_) throw encode_error("cannot insert into self");
    check(pn_data_append(e.data_, v.data_), e.data_);
    return e;
}

}
