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
#include "proton/decimal.hpp"
#include "proton/value.hpp"
#include "proton/message_id.hpp"
#include "proton/annotation_key.hpp"
#include "proton/amqp.hpp"
#include "proton_bits.hpp"

#include "types_internal.hpp"
#include "msg.hpp"

#include <proton/codec.h>

namespace proton {
namespace internal {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the decoder.
 *
 */
namespace {
struct save_state {
    pn_data_t* data;
    pn_handle_t handle;
    save_state(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~save_state() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

struct narrow {
    data data_;
    narrow(data d) : data_(d) { data_.narrow(); }
    ~narrow() { data_.widen(); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw conversion_error("" + error_str(result));
    return result;
}

}

void decoder::decode(const char* i, size_t size) {
    save_state ss(pn_object());
    const char* end = i + size;
    while (i < end) {
        i += check(pn_data_decode(pn_object(), i, size_t(end - i)));
    }
}

void decoder::decode(const std::string& buffer) {
    decode(buffer.data(), buffer.size());
}

bool decoder::more() const {
    save_state ss(pn_object());
    return pn_data_next(pn_object());
}

void decoder::rewind() { ::pn_data_rewind(pn_object()); }

void decoder::backup() { ::pn_data_prev(pn_object()); }

void decoder::skip() { ::pn_data_next(pn_object()); }

namespace {

void bad_type(type_id want, type_id got) {
    if (want != got) throw make_conversion_error(want, got);
}

type_id pre_get(pn_data_t* data) {
    if (!pn_data_next(data)) throw conversion_error("no more data");
    type_id t = type_id(pn_data_type(data));
    if (t < 0) throw conversion_error("invalid data");
    return t;
}

template <class T, class U> void assign(T& x, const U& y) { x = y; }
void assign(uuid& x, const pn_uuid_t y) { byte_copy(x, y); }
void assign(decimal32& x, const pn_decimal32_t y) { byte_copy(x, y); }
void assign(decimal64& x, const pn_decimal64_t y)  { byte_copy(x, y); }
void assign(decimal128& x, const pn_decimal128_t y) { byte_copy(x, y); }

// Simple extract with no type conversion.
template <class T, class U> void extract(pn_data_t* data, T& x, U (*get)(pn_data_t*)) {
    save_state ss(data);
    bad_type(type_id_of<T>::value, pre_get(data));
    assign(x, get(data));
    ss.cancel();                // No error, no rewind
}

}

void decoder::check_type(type_id want) {
    type_id got = type();
    if (want != got) bad_type(want, got);
}

type_id decoder::type() const {
    save_state ss(pn_object());
    return pre_get(pn_object());
}

decoder decoder::operator>>(start& s) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    s.type = pre_get(d);
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(d);
        s.element = type_id(pn_data_get_array_type(d)); s.is_described = pn_data_is_array_described(d);
        break;
      case LIST:
        s.size = pn_data_get_list(d);
        break;
      case MAP:
        s.size = pn_data_get_map(d);
        break;
      case DESCRIBED:
        s.is_described = true;
        s.size = 1;
        break;
      default:
        throw conversion_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(d);
    ss.cancel();
    return *this;
}

// FIXME aconway 2016-03-08: remove
decoder decoder::operator>>(struct finish) { pn_data_exit(pn_object()); return *this; }

decoder decoder::operator>>(struct skip) { pn_data_next(pn_object()); return *this; }

decoder decoder::operator>>(struct assert_type a) { bad_type(a.type, type()); return *this; }

decoder decoder::operator>>(struct rewind) { rewind(); return *this; }

decoder decoder::operator>>(null&) {
    save_state ss(pn_object());
    bad_type(NULL_TYPE, pre_get(pn_object()));
    return *this;
}

decoder decoder::operator>>(value& v) {
    data mine(pn_object());
    if (mine == v.data_)
        throw conversion_error("extract into self");
    v.clear();
    {
        narrow n(pn_object());
        check(v.data().appendn(mine, 1));
    }
    if (!mine.next()) throw conversion_error("no more data");
    return *this;
}

decoder decoder::operator>>(message_id& x) {
    switch (type()) {
      case ULONG:
      case UUID:
      case BINARY:
      case STRING:
        return *this >> x.scalar_;
      default:
        throw conversion_error(
            msg() << "expected one of ulong, uuid, binary or string but found " << type());
    };
}

decoder decoder::operator>>(annotation_key& x) {
    switch (type()) {
      case ULONG:
      case SYMBOL:
        return *this >> x.scalar_;
      default:
        throw conversion_error("expected one of ulong or symbol but found " + type_name(type()));
    };
}

decoder decoder::operator>>(scalar& x) {
    save_state ss(pn_object());
    type_id got = pre_get(pn_object());
    if (!type_id_is_scalar(got))
        throw conversion_error("expected scalar, found "+type_name(got));
    x.set(pn_data_get_atom(pn_object()));
    ss.cancel();                // No error, no rewind
    return *this;
}

decoder decoder::operator>>(amqp::boolean_type &x) {
    extract(pn_object(), x, pn_data_get_bool);
    return *this;
}

decoder decoder::operator>>(amqp::ubyte_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: x = pn_data_get_ubyte(d); break;
      default: bad_type(UBYTE, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::byte_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: x = pn_data_get_byte(d); break;
      default: bad_type(BYTE, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::ushort_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: x = pn_data_get_ubyte(d); break;
      case USHORT: x = pn_data_get_ushort(d); break;
      default: bad_type(USHORT, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::short_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: x = pn_data_get_byte(d); break;
      case SHORT: x = pn_data_get_short(d); break;
      default: bad_type(SHORT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::uint_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: x = pn_data_get_ubyte(d); break;
      case USHORT: x = pn_data_get_ushort(d); break;
      case UINT: x = pn_data_get_uint(d); break;
      default: bad_type(UINT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::int_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: x = pn_data_get_byte(d); break;
      case SHORT: x = pn_data_get_short(d); break;
      case INT: x = pn_data_get_int(d); break;
      default: bad_type(INT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::ulong_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: x = pn_data_get_ubyte(d); break;
      case USHORT: x = pn_data_get_ushort(d); break;
      case UINT: x = pn_data_get_uint(d); break;
      case ULONG: x = pn_data_get_ulong(d); break;
      default: bad_type(ULONG, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::long_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: x = pn_data_get_byte(d); break;
      case SHORT: x = pn_data_get_short(d); break;
      case INT: x = pn_data_get_int(d); break;
      case LONG: x = pn_data_get_long(d); break;
      default: bad_type(LONG, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::char_type &x) {
    extract(pn_object(), x, pn_data_get_char);
    return *this;
}

decoder decoder::operator>>(timestamp &x) {
    extract(pn_object(), x, pn_data_get_timestamp);
    return *this;
}

decoder decoder::operator>>(amqp::float_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case FLOAT: x = pn_data_get_float(d); break;
      case DOUBLE: x = float(pn_data_get_double(d)); break;
      default: bad_type(FLOAT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(amqp::double_type &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case FLOAT: x = pn_data_get_float(d); break;
      case DOUBLE: x = pn_data_get_double(d); break;
      default: bad_type(DOUBLE, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

decoder decoder::operator>>(decimal32 &x) {
    extract(pn_object(), x, pn_data_get_decimal32);
    return *this;
}

decoder decoder::operator>>(decimal64 &x) {
    extract(pn_object(), x, pn_data_get_decimal64);
    return *this;
}

decoder decoder::operator>>(decimal128 &x)  {
    extract(pn_object(), x, pn_data_get_decimal128);
    return *this;
}

decoder decoder::operator>>(uuid &x)  {
    extract(pn_object(), x, pn_data_get_uuid);
    return *this;
}

decoder decoder::operator>>(std::string &x) {
    pn_data_t* d = pn_object();
    save_state ss(d);
    switch (pre_get(d)) {
      case STRING: x = str(pn_data_get_string(d)); break;
      case BINARY: x = str(pn_data_get_binary(d)); break;
      case SYMBOL: x = str(pn_data_get_symbol(d)); break;
      default: bad_type(STRING, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return *this;
}

void assert_map_scope(const scope& s) {
    if (s.type != MAP)
        throw conversion_error("cannot decode "+type_name(s.type)+" as map");
    if (s.size % 2 != 0)
        throw conversion_error("odd number of elements in map");
}


} // internal
} // proton
