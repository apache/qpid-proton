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

#include "proton/cpp/Encoder.h"
#include <proton/codec.h>
#include "proton_bits.h"

namespace proton {
namespace reactor {

Encoder::Encoder() {}
Encoder::~Encoder() {}

namespace {
struct SaveState {
    pn_data_t* data;
    pn_handle_t handle;
    SaveState(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~SaveState() { if (data) pn_data_restore(data, handle); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw Encoder::Error("encode: " + errorStr(result));
    return result;
}
}

bool Encoder::encode(char* buffer, size_t& size) {
    SaveState ss(data);               // In case of error
    pn_data_rewind(data);
    ssize_t result = pn_data_encode(data, buffer, size);
    if (result == PN_OVERFLOW) {
        size = pn_data_encoded_size(data);
        return false;
    }
    check(result);
    size = result;
    ss.data = 0;                // Don't restore state, all is well.
    pn_data_clear(data);
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

namespace {
template <class T, class U>
Encoder& insert(Encoder& e, pn_data_t* data, T& value, int (*put)(pn_data_t*, U)) {
    SaveState ss(data);         // Save state in case of error.
    check(put(data, value));
    ss.data = 0;                // Don't restore state, all is good.
    return e;
}
}

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

}} // namespace proton::reactor
