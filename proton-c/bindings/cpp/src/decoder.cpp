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

#include "proton/decoder.hpp"
#include "proton/value.hpp"
#include <proton/codec.h>
#include "proton_bits.hpp"
#include "msg.hpp"

namespace proton {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the decoder.
 *
 */
decoder::decoder() {}
decoder::decoder(const char* buffer, size_t size) { decode(buffer, size); }
decoder::decoder(const std::string& buffer) { decode(buffer); }
decoder::~decoder() {}

static const std::string prefix("decode: ");
decode_error::decode_error(const std::string& msg) throw() : error(prefix+msg) {}

namespace {
struct save_state {
    pn_data_t* data;
    pn_handle_t handle;
    save_state(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~save_state() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

struct Narrow {
    pn_data_t* data;
    Narrow(pn_data_t* d) : data(d) { pn_data_narrow(d); }
    ~Narrow() { pn_data_widen(data); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw decode_error("" + error_str(result));
    return result;
}

}

void decoder::decode(const char* i, size_t size) {
    save_state ss(data_);
    const char* end = i + size;
    while (i < end) {
        i += check(pn_data_decode(data_, i, end - i));
    }
}

void decoder::decode(const std::string& buffer) {
    decode(buffer.data(), buffer.size());
}

bool decoder::more() const {
    save_state ss(data_);
    return pn_data_next(data_);
}

namespace {

void bad_type(type_id want, type_id got) {
    if (want != got)
        throw decode_error("expected "+type_name(want)+" found "+type_name(got));
}

type_id pre_get(pn_data_t* data) {
    if (!pn_data_next(data)) throw decode_error("no more data");
    type_id t = type_id(pn_data_type(data));
    if (t < 0) throw decode_error("invalid data");
    return t;
}

// Simple extract with no type conversion.
template <class T, class U> void extract(pn_data_t* data, T& value, U (*get)(pn_data_t*)) {
    save_state ss(data);
    bad_type(type_id_of<T>::value, pre_get(data));
    value = get(data);
    ss.cancel();                // No error, no rewind
}

}

void decoder::check_type(type_id want) {
    type_id got = type();
    if (want != got) bad_type(want, got);
}

type_id decoder::type() const {
    save_state ss(data_);
    return pre_get(data_);
}

decoder& operator>>(decoder& d, start& s) {
    save_state ss(d.data_);
    s.type = pre_get(d.data_);
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(d.data_);
        s.element = type_id(pn_data_get_array_type(d.data_));
        s.is_described = pn_data_is_array_described(d.data_);
        break;
      case LIST:
        s.size = pn_data_get_list(d.data_);
        break;
      case MAP:
        s.size = pn_data_get_map(d.data_);
        break;
      case DESCRIBED:
        s.is_described = true;
        s.size = 1;
        break;
      default:
        throw decode_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(d.data_);
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, finish) { pn_data_exit(d.data_); return d; }

decoder& operator>>(decoder& d, skip) { pn_data_next(d.data_); return d; }

decoder& operator>>(decoder& d, rewind) { d.rewind(); return d; }

decoder& operator>>(decoder& d, value& v) {
    if (d.data_ == v.values_.data_) throw decode_error("extract into self");
    pn_data_clear(v.values_.data_);
    {
        Narrow n(d.data_);
        check(pn_data_appendn(v.values_.data_, d.data_, 1));
    }
    if (!pn_data_next(d.data_)) throw decode_error("no more data");
    return d;
}


decoder& operator>>(decoder& d, amqp_null) {
    save_state ss(d.data_);
    bad_type(NULl_, pre_get(d.data_));
    return d;
}

decoder& operator>>(decoder& d, amqp_bool& value) {
    extract(d.data_, value, pn_data_get_bool);
    return d;
}

decoder& operator>>(decoder& d, amqp_ubyte& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case UBYTE: value = pn_data_get_ubyte(d.data_); break;
      default: bad_type(UBYTE, type_id(type_id(pn_data_type(d.data_))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_byte& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case BYTE: value = pn_data_get_byte(d.data_); break;
      default: bad_type(BYTE, type_id(type_id(pn_data_type(d.data_))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_ushort& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case UBYTE: value = pn_data_get_ubyte(d.data_); break;
      case USHORT: value = pn_data_get_ushort(d.data_); break;
      default: bad_type(USHORT, type_id(type_id(pn_data_type(d.data_))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_short& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case BYTE: value = pn_data_get_byte(d.data_); break;
      case SHORT: value = pn_data_get_short(d.data_); break;
      default: bad_type(SHORT, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_uint& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case UBYTE: value = pn_data_get_ubyte(d.data_); break;
      case USHORT: value = pn_data_get_ushort(d.data_); break;
      case UINT: value = pn_data_get_uint(d.data_); break;
      default: bad_type(UINT, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_int& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case BYTE: value = pn_data_get_byte(d.data_); break;
      case SHORT: value = pn_data_get_short(d.data_); break;
      case INT: value = pn_data_get_int(d.data_); break;
      default: bad_type(INT, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_ulong& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case UBYTE: value = pn_data_get_ubyte(d.data_); break;
      case USHORT: value = pn_data_get_ushort(d.data_); break;
      case UINT: value = pn_data_get_uint(d.data_); break;
      case ULONG: value = pn_data_get_ulong(d.data_); break;
      default: bad_type(ULONG, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_long& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case BYTE: value = pn_data_get_byte(d.data_); break;
      case SHORT: value = pn_data_get_short(d.data_); break;
      case INT: value = pn_data_get_int(d.data_); break;
      case LONG: value = pn_data_get_long(d.data_); break;
      default: bad_type(LONG, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_char& value) {
    extract(d.data_, value, pn_data_get_char);
    return d;
}

decoder& operator>>(decoder& d, amqp_timestamp& value) {
    extract(d.data_, value, pn_data_get_timestamp);
    return d;
}

decoder& operator>>(decoder& d, amqp_float& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case FLOAT: value = pn_data_get_float(d.data_); break;
      case DOUBLE: value = pn_data_get_double(d.data_); break;
      default: bad_type(FLOAT, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_double& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case FLOAT: value = pn_data_get_float(d.data_); break;
      case DOUBLE: value = pn_data_get_double(d.data_); break;
      default: bad_type(DOUBLE, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

// TODO aconway 2015-06-11: decimal conversions.
decoder& operator>>(decoder& d, amqp_decimal32& value) {
    extract(d.data_, value, pn_data_get_decimal32);
    return d;
}

decoder& operator>>(decoder& d, amqp_decimal64& value) {
    extract(d.data_, value, pn_data_get_decimal64);
    return d;
}

decoder& operator>>(decoder& d, amqp_decimal128& value)  {
    extract(d.data_, value, pn_data_get_decimal128);
    return d;
}

decoder& operator>>(decoder& d, amqp_uuid& value)  {
    extract(d.data_, value, pn_data_get_uuid);
    return d;
}

decoder& operator>>(decoder& d, std::string& value) {
    save_state ss(d.data_);
    switch (pre_get(d.data_)) {
      case STRING: value = str(pn_data_get_string(d.data_)); break;
      case BINARY: value = str(pn_data_get_binary(d.data_)); break;
      case SYMBOL: value = str(pn_data_get_symbol(d.data_)); break;
      default: bad_type(STRING, type_id(pn_data_type(d.data_)));
    }
    ss.cancel();
    return d;
}

}
