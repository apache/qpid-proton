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

#include "proton/data.hpp"
#include "proton/decoder.hpp"
#include "proton/value.hpp"
#include "proton/message_id.hpp"
#include "proton_bits.hpp"
#include "msg.hpp"

#include <proton/codec.h>

namespace proton {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the decoder.
 *
 */
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

struct narrow {
    pn_data_t* data;
    narrow(pn_data_t* d) : data(d) { pn_data_narrow(d); }
    ~narrow() { pn_data_widen(data); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw decode_error("" + error_str(result));
    return result;
}

}

void decoder::decode(const char* i, size_t size) {
    save_state ss(pn_cast(this));
    const char* end = i + size;
    while (i < end) {
        i += check(pn_data_decode(pn_cast(this), i, end - i));
    }
}

void decoder::decode(const std::string& buffer) {
    decode(buffer.data(), buffer.size());
}

bool decoder::more() const {
    save_state ss(pn_cast(this));
    return pn_data_next(pn_cast(this));
}

void decoder::rewind() { pn_data_rewind(pn_cast(this)); }

void decoder::backup() { pn_data_prev(pn_cast(this)); }

void decoder::skip() { pn_data_next(pn_cast(this)); }

data& decoder::data() { return *data::cast(pn_cast(this)); }

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
    save_state ss(pn_cast(this));
    return pre_get(pn_cast(this));
}

decoder& operator>>(decoder& d, start& s) {
    save_state ss(pn_cast(&d));
    s.type = pre_get(pn_cast(&d));
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(pn_cast(&d));
        s.element = type_id(pn_data_get_array_type(pn_cast(&d))); s.is_described = pn_data_is_array_described(pn_cast(&d));
        break;
      case LIST:
        s.size = pn_data_get_list(pn_cast(&d));
        break;
      case MAP:
        s.size = pn_data_get_map(pn_cast(&d));
        break;
      case DESCRIBED:
        s.is_described = true;
        s.size = 1;
        break;
      default:
        throw decode_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(pn_cast(&d));
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, finish) { pn_data_exit(pn_cast(&d)); return d; }

decoder& operator>>(decoder& d, skip) { pn_data_next(pn_cast(&d)); return d; }

decoder& operator>>(decoder& d, assert_type a) { bad_type(a.type, d.type()); return d; }

decoder& operator>>(decoder& d, rewind) { d.rewind(); return d; }

decoder& operator>>(decoder& d, value& v) {
    pn_data_t *ddata = pn_cast(&d);
    pn_data_t *vdata = pn_cast(&v.encoder());
    if (ddata == vdata) throw decode_error("extract into self");
    {
        narrow n(ddata);
        check(pn_data_appendn(vdata, ddata, 1));
    }
    if (!pn_data_next(ddata)) throw decode_error("no more data");
    return d;
}

decoder& operator>>(decoder& d, message_id& id) {
    switch (d.type()) {
      case ULONG:
      case UUID:
      case BINARY:
      case STRING:
        return d >> id.value_;
      default:
        throw decode_error("expected one of ulong, uuid, binary or string but found " +
                           type_name(d.type()));
    };
}

decoder& operator>>(decoder& d, amqp_null) {
    save_state ss(pn_cast(&d));
    bad_type(NULL_, pre_get(pn_cast(&d)));
    return d;
}

decoder& operator>>(decoder& d, amqp_boolean& value) {
    extract(pn_cast(&d), value, pn_data_get_bool);
    return d;
}

decoder& operator>>(decoder& d, amqp_ubyte& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case UBYTE: value = pn_data_get_ubyte(pn_cast(&d)); break;
      default: bad_type(UBYTE, type_id(type_id(pn_data_type(pn_cast(&d)))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_byte& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case BYTE: value = pn_data_get_byte(pn_cast(&d)); break;
      default: bad_type(BYTE, type_id(type_id(pn_data_type(pn_cast(&d)))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_ushort& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case UBYTE: value = pn_data_get_ubyte(pn_cast(&d)); break;
      case USHORT: value = pn_data_get_ushort(pn_cast(&d)); break;
      default: bad_type(USHORT, type_id(type_id(pn_data_type(pn_cast(&d)))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_short& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case BYTE: value = pn_data_get_byte(pn_cast(&d)); break;
      case SHORT: value = pn_data_get_short(pn_cast(&d)); break;
      default: bad_type(SHORT, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_uint& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case UBYTE: value = pn_data_get_ubyte(pn_cast(&d)); break;
      case USHORT: value = pn_data_get_ushort(pn_cast(&d)); break;
      case UINT: value = pn_data_get_uint(pn_cast(&d)); break;
      default: bad_type(UINT, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_int& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case BYTE: value = pn_data_get_byte(pn_cast(&d)); break;
      case SHORT: value = pn_data_get_short(pn_cast(&d)); break;
      case INT: value = pn_data_get_int(pn_cast(&d)); break;
      default: bad_type(INT, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_ulong& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case UBYTE: value = pn_data_get_ubyte(pn_cast(&d)); break;
      case USHORT: value = pn_data_get_ushort(pn_cast(&d)); break;
      case UINT: value = pn_data_get_uint(pn_cast(&d)); break;
      case ULONG: value = pn_data_get_ulong(pn_cast(&d)); break;
      default: bad_type(ULONG, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_long& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case BYTE: value = pn_data_get_byte(pn_cast(&d)); break;
      case SHORT: value = pn_data_get_short(pn_cast(&d)); break;
      case INT: value = pn_data_get_int(pn_cast(&d)); break;
      case LONG: value = pn_data_get_long(pn_cast(&d)); break;
      default: bad_type(LONG, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_char& value) {
    extract(pn_cast(&d), value, pn_data_get_char);
    return d;
}

decoder& operator>>(decoder& d, amqp_timestamp& value) {
    extract(pn_cast(&d), value, pn_data_get_timestamp);
    return d;
}

decoder& operator>>(decoder& d, amqp_float& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case FLOAT: value = pn_data_get_float(pn_cast(&d)); break;
      case DOUBLE: value = pn_data_get_double(pn_cast(&d)); break;
      default: bad_type(FLOAT, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_double& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case FLOAT: value = pn_data_get_float(pn_cast(&d)); break;
      case DOUBLE: value = pn_data_get_double(pn_cast(&d)); break;
      default: bad_type(DOUBLE, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

decoder& operator>>(decoder& d, amqp_decimal32& value) {
    extract(pn_cast(&d), value, pn_data_get_decimal32);
    return d;
}

decoder& operator>>(decoder& d, amqp_decimal64& value) {
    extract(pn_cast(&d), value, pn_data_get_decimal64);
    return d;
}

decoder& operator>>(decoder& d, amqp_decimal128& value)  {
    extract(pn_cast(&d), value, pn_data_get_decimal128);
    return d;
}

decoder& operator>>(decoder& d, amqp_uuid& value)  {
    extract(pn_cast(&d), value, pn_data_get_uuid);
    return d;
}

decoder& operator>>(decoder& d, std::string& value) {
    save_state ss(pn_cast(&d));
    switch (pre_get(pn_cast(&d))) {
      case STRING: value = str(pn_data_get_string(pn_cast(&d))); break;
      case BINARY: value = str(pn_data_get_binary(pn_cast(&d))); break;
      case SYMBOL: value = str(pn_data_get_symbol(pn_cast(&d))); break;
      default: bad_type(STRING, type_id(pn_data_type(pn_cast(&d))));
    }
    ss.cancel();
    return d;
}

void assert_map_scope(const decoder::scope& s) {
    if (s.type != MAP)
        throw decode_error("cannot decode "+type_name(s.type)+" as map");
    if (s.size % 2 != 0)
        throw decode_error("odd number of elements in map");
}


}
