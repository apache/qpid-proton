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

#include "proton/Encoder.hpp"
#include "proton/Value.hpp"
#include <proton/codec.h>
#include "proton_bits.hpp"
#include "Msg.hpp"

namespace proton {

Encoder::Encoder() {}
Encoder::~Encoder() {}

static const std::string prefix("encode: ");
EncodeError::EncodeError(const std::string& msg) throw() : Error(prefix+msg) {}

namespace {
struct SaveState {
    pn_data_t* data;
    pn_handle_t handle;
    SaveState(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~SaveState() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

void check(int result, pn_data_t* data) {
    if (result < 0)
        throw EncodeError(errorStr(pn_data_error(data), result));
}
}

bool Encoder::encode(char* buffer, size_t& size) {
    SaveState ss(data);               // In case of error
    ssize_t result = pn_data_encode(data, buffer, size);
    if (result == PN_OVERFLOW) {
        result = pn_data_encoded_size(data);
        if (result >= 0) {
            size = result;
            return false;
        }
    }
    check(result, data);
    size = result;
    ss.cancel();                // Don't restore state, all is well.
    pn_data_clear(data);
    return true;
}

void Encoder::encode(std::string& s) {
    size_t size = s.size();
    if (!encode(&s[0], size)) {
        s.resize(size);
        encode(&s[0], size);
    }
}

std::string Encoder::encode() {
    std::string s;
    encode(s);
    return s;
}

Encoder& operator<<(Encoder& e, const Start& s) {
    switch (s.type) {
      case ARRAY: pn_data_put_array(e.data, s.isDescribed, pn_type_t(s.element)); break;
      case MAP: pn_data_put_map(e.data); break;
      case LIST: pn_data_put_list(e.data); break;
      case DESCRIBED: pn_data_put_described(e.data); break;
      default:
        throw EncodeError(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(e.data);
    return e;
}

Encoder& operator<<(Encoder& e, Finish) {
    pn_data_exit(e.data);
    return e;
}

namespace {
template <class T, class U>
Encoder& insert(Encoder& e, pn_data_t* data, T& value, int (*put)(pn_data_t*, U)) {
    SaveState ss(data);         // Save state in case of error.
    check(put(data, value), data);
    ss.cancel();                // Don't restore state, all is good.
    return e;
}
}

Encoder& operator<<(Encoder& e, Null) { pn_data_put_null(e.data); return e; }
Encoder& operator<<(Encoder& e, Bool value) { return insert(e, e.data, value, pn_data_put_bool); }
Encoder& operator<<(Encoder& e, Ubyte value) { return insert(e, e.data, value, pn_data_put_ubyte); }
Encoder& operator<<(Encoder& e, Byte value) { return insert(e, e.data, value, pn_data_put_byte); }
Encoder& operator<<(Encoder& e, Ushort value) { return insert(e, e.data, value, pn_data_put_ushort); }
Encoder& operator<<(Encoder& e, Short value) { return insert(e, e.data, value, pn_data_put_short); }
Encoder& operator<<(Encoder& e, Uint value) { return insert(e, e.data, value, pn_data_put_uint); }
Encoder& operator<<(Encoder& e, Int value) { return insert(e, e.data, value, pn_data_put_int); }
Encoder& operator<<(Encoder& e, Char value) { return insert(e, e.data, value, pn_data_put_char); }
Encoder& operator<<(Encoder& e, Ulong value) { return insert(e, e.data, value, pn_data_put_ulong); }
Encoder& operator<<(Encoder& e, Long value) { return insert(e, e.data, value, pn_data_put_long); }
Encoder& operator<<(Encoder& e, Timestamp value) { return insert(e, e.data, value, pn_data_put_timestamp); }
Encoder& operator<<(Encoder& e, Float value) { return insert(e, e.data, value, pn_data_put_float); }
Encoder& operator<<(Encoder& e, Double value) { return insert(e, e.data, value, pn_data_put_double); }
Encoder& operator<<(Encoder& e, Decimal32 value) { return insert(e, e.data, value, pn_data_put_decimal32); }
Encoder& operator<<(Encoder& e, Decimal64 value) { return insert(e, e.data, value, pn_data_put_decimal64); }
Encoder& operator<<(Encoder& e, Decimal128 value) { return insert(e, e.data, value, pn_data_put_decimal128); }
Encoder& operator<<(Encoder& e, Uuid value) { return insert(e, e.data, value, pn_data_put_uuid); }
Encoder& operator<<(Encoder& e, String value) { return insert(e, e.data, value, pn_data_put_string); }
Encoder& operator<<(Encoder& e, Symbol value) { return insert(e, e.data, value, pn_data_put_symbol); }
Encoder& operator<<(Encoder& e, Binary value) { return insert(e, e.data, value, pn_data_put_binary); }

// Meta-function to get the class from the type ID.
template <TypeId A> struct ClassOf {};
template<> struct ClassOf<NULL_> { typedef Null ValueType; };
template<> struct ClassOf<BOOL> { typedef Bool ValueType; };
template<> struct ClassOf<UBYTE> { typedef Ubyte ValueType; };
template<> struct ClassOf<BYTE> { typedef Byte ValueType; };
template<> struct ClassOf<USHORT> { typedef Ushort ValueType; };
template<> struct ClassOf<SHORT> { typedef Short ValueType; };
template<> struct ClassOf<UINT> { typedef Uint ValueType; };
template<> struct ClassOf<INT> { typedef Int ValueType; };
template<> struct ClassOf<CHAR> { typedef Char ValueType; };
template<> struct ClassOf<ULONG> { typedef Ulong ValueType; };
template<> struct ClassOf<LONG> { typedef Long ValueType; };
template<> struct ClassOf<TIMESTAMP> { typedef Timestamp ValueType; };
template<> struct ClassOf<FLOAT> { typedef Float ValueType; };
template<> struct ClassOf<DOUBLE> { typedef Double ValueType; };
template<> struct ClassOf<DECIMAL32> { typedef Decimal32 ValueType; };
template<> struct ClassOf<DECIMAL64> { typedef Decimal64 ValueType; };
template<> struct ClassOf<DECIMAL128> { typedef Decimal128 ValueType; };
template<> struct ClassOf<UUID> { typedef Uuid ValueType; };
template<> struct ClassOf<BINARY> { typedef Binary ValueType; };
template<> struct ClassOf<STRING> { typedef String ValueType; };
template<> struct ClassOf<SYMBOL> { typedef Symbol ValueType; };

Encoder& operator<<(Encoder& e, const Value& v) {
    if (e.data == v.values.data) throw EncodeError("cannot insert into self");
    check(pn_data_appendn(e.data, v.values.data, 1), e.data);
    return e;
}

}
