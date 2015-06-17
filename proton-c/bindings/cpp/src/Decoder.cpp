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

#include "proton/Decoder.hpp"
#include "proton/Value.hpp"
#include <proton/codec.h>
#include "proton_bits.hpp"
#include "Msg.hpp"

namespace proton {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the Decoder.
 *
 */
Decoder::Decoder() {}
Decoder::Decoder(const char* buffer, size_t size) { decode(buffer, size); }
Decoder::Decoder(const std::string& buffer) { decode(buffer); }
Decoder::~Decoder() {}

DecodeError::DecodeError(const std::string& msg) throw() : Error("decode: "+msg) {}

namespace {
struct SaveState {
    pn_data_t* data;
    pn_handle_t handle;
    SaveState(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~SaveState() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

struct Narrow {
    pn_data_t* data;
    Narrow(pn_data_t* d) : data(d) { pn_data_narrow(d); }
    ~Narrow() { pn_data_widen(data); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw DecodeError("" + errorStr(result));
    return result;
}

}

void Decoder::decode(const char* i, size_t size) {
    SaveState ss(data);
    const char* end = i + size;
    while (i < end) {
        i += check(pn_data_decode(data, i, end - i));
    }
}

void Decoder::decode(const std::string& buffer) {
    decode(buffer.data(), buffer.size());
}

bool Decoder::more() const {
    SaveState ss(data);
    return pn_data_next(data);
}

namespace {

void badType(TypeId want, TypeId got) {
    if (want != got)
        throw DecodeError("expected "+typeName(want)+" found "+typeName(got));
}

TypeId preGet(pn_data_t* data) {
    if (!pn_data_next(data)) throw DecodeError("no more data");
    TypeId t = TypeId(pn_data_type(data));
    if (t < 0) throw DecodeError("invalid data");
    return t;
}

// Simple extract with no type conversion.
template <class T, class U> void extract(pn_data_t* data, T& value, U (*get)(pn_data_t*)) {
    SaveState ss(data);
    badType(TypeIdOf<T>::value, preGet(data));
    value = get(data);
    ss.cancel();                // No error, no rewind
}

}

void Decoder::checkType(TypeId want) {
    TypeId got = type();
    if (want != got) badType(want, got);
}

TypeId Decoder::type() const {
    SaveState ss(data);
    return preGet(data);
}

Decoder& operator>>(Decoder& d, Start& s) {
    SaveState ss(d.data);
    s.type = preGet(d.data);
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(d.data);
        s.element = TypeId(pn_data_get_array_type(d.data));
        s.isDescribed = pn_data_is_array_described(d.data);
        break;
      case LIST:
        s.size = pn_data_get_list(d.data);
        break;
      case MAP:
        s.size = pn_data_get_map(d.data);
        break;
      case DESCRIBED:
        s.isDescribed = true;
        s.size = 1;
        break;
      default:
        throw DecodeError(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(d.data);
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Finish) { pn_data_exit(d.data); return d; }

Decoder& operator>>(Decoder& d, Skip) { pn_data_next(d.data); return d; }

Decoder& operator>>(Decoder& d, Value& v) {
    if (d.data == v.values.data) throw DecodeError("extract into self");
    pn_data_clear(v.values.data);
    {
        Narrow n(d.data);
        check(pn_data_appendn(v.values.data, d.data, 1));
    }
    if (!pn_data_next(d.data)) throw DecodeError("no more data");
    return d;
}


Decoder& operator>>(Decoder& d, Null) {
    SaveState ss(d.data);
    badType(NULL_, preGet(d.data));
    return d;
}

Decoder& operator>>(Decoder& d, Bool& value) {
    extract(d.data, value, pn_data_get_bool);
    return d;
}

Decoder& operator>>(Decoder& d, Ubyte& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case UBYTE: value = pn_data_get_ubyte(d.data); break;
      default: badType(UBYTE, TypeId(TypeId(pn_data_type(d.data))));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Byte& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case BYTE: value = pn_data_get_byte(d.data); break;
      default: badType(BYTE, TypeId(TypeId(pn_data_type(d.data))));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Ushort& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case UBYTE: value = pn_data_get_ubyte(d.data); break;
      case USHORT: value = pn_data_get_ushort(d.data); break;
      default: badType(USHORT, TypeId(TypeId(pn_data_type(d.data))));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Short& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case BYTE: value = pn_data_get_byte(d.data); break;
      case SHORT: value = pn_data_get_short(d.data); break;
      default: badType(SHORT, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Uint& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case UBYTE: value = pn_data_get_ubyte(d.data); break;
      case USHORT: value = pn_data_get_ushort(d.data); break;
      case UINT: value = pn_data_get_uint(d.data); break;
      default: badType(UINT, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Int& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case BYTE: value = pn_data_get_byte(d.data); break;
      case SHORT: value = pn_data_get_short(d.data); break;
      case INT: value = pn_data_get_int(d.data); break;
      default: badType(INT, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Ulong& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case UBYTE: value = pn_data_get_ubyte(d.data); break;
      case USHORT: value = pn_data_get_ushort(d.data); break;
      case UINT: value = pn_data_get_uint(d.data); break;
      case ULONG: value = pn_data_get_ulong(d.data); break;
      default: badType(ULONG, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Long& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case BYTE: value = pn_data_get_byte(d.data); break;
      case SHORT: value = pn_data_get_short(d.data); break;
      case INT: value = pn_data_get_int(d.data); break;
      case LONG: value = pn_data_get_long(d.data); break;
      default: badType(LONG, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Char& value) {
    extract(d.data, value, pn_data_get_char);
    return d;
}

Decoder& operator>>(Decoder& d, Timestamp& value) {
    extract(d.data, value, pn_data_get_timestamp);
    return d;
}

Decoder& operator>>(Decoder& d, Float& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case FLOAT: value = pn_data_get_float(d.data); break;
      case DOUBLE: value = pn_data_get_double(d.data); break;
      default: badType(FLOAT, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

Decoder& operator>>(Decoder& d, Double& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case FLOAT: value = pn_data_get_float(d.data); break;
      case DOUBLE: value = pn_data_get_double(d.data); break;
      default: badType(DOUBLE, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

// TODO aconway 2015-06-11: decimal conversions.
Decoder& operator>>(Decoder& d, Decimal32& value) {
    extract(d.data, value, pn_data_get_decimal32);
    return d;
}

Decoder& operator>>(Decoder& d, Decimal64& value) {
    extract(d.data, value, pn_data_get_decimal64);
    return d;
}

Decoder& operator>>(Decoder& d, Decimal128& value)  {
    extract(d.data, value, pn_data_get_decimal128);
    return d;
}

Decoder& operator>>(Decoder& d, Uuid& value)  {
    extract(d.data, value, pn_data_get_uuid);
    return d;
}

Decoder& operator>>(Decoder& d, std::string& value) {
    SaveState ss(d.data);
    switch (preGet(d.data)) {
      case STRING: value = str(pn_data_get_string(d.data)); break;
      case BINARY: value = str(pn_data_get_binary(d.data)); break;
      case SYMBOL: value = str(pn_data_get_symbol(d.data)); break;
      default: badType(STRING, TypeId(pn_data_type(d.data)));
    }
    ss.cancel();
    return d;
}

}
