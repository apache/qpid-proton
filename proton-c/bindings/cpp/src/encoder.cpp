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
#include "proton/encoder.hpp"
#include "proton/message_id.hpp"
#include "proton/annotation_key.hpp"
#include "proton/value.hpp"

#include "proton_bits.hpp"
#include "msg.hpp"

#include <proton/codec.h>

#include <algorithm>

namespace proton {

namespace {
struct save_state {
    pn_data_t* data;
    pn_handle_t handle;
    save_state(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~save_state() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

void check(long result, pn_data_t* data) {
    if (result < 0)
        throw encode_error(error_str(pn_data_error(data), result));
}
}

bool encoder::encode(char* buffer, size_t& size) {
    save_state ss(pn_object()); // In case of error
    ssize_t result = pn_data_encode(pn_object(), buffer, size);
    if (result == PN_OVERFLOW) {
        result = pn_data_encoded_size(pn_object());
        if (result >= 0) {
            size = size_t(result);
            return false;
        }
    }
    check(result, pn_object());
    size = size_t(result);
    ss.cancel();                // Don't restore state, all is well.
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

data encoder::data() { return proton::data(pn_object()); }

encoder operator<<(encoder e, const start& s) {
    switch (s.type) {
      case ARRAY: pn_data_put_array(e.pn_object(), s.is_described, pn_type_t(s.element)); break;
      case MAP: pn_data_put_map(e.pn_object()); break;
      case LIST: pn_data_put_list(e.pn_object()); break;
      case DESCRIBED: pn_data_put_described(e.pn_object()); break;
      default:
        throw encode_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(e.pn_object());
    return e;
}

encoder operator<<(encoder e, finish) {
    pn_data_exit(e.pn_object());
    return e;
}

namespace {
template <class T, class U>
encoder insert(encoder e, pn_data_t* data, T& x, int (*put)(pn_data_t*, U)) {
    save_state ss(data);         // Save state in case of error.
    check(put(data, x), data);
    ss.cancel();                // Don't restore state, all is good.
    return e;
}

int pn_data_put_amqp_string(pn_data_t *d, const amqp_string& x) { return pn_data_put_string(d, pn_bytes(x)); }
int pn_data_put_amqp_binary(pn_data_t *d, const amqp_binary& x) { return pn_data_put_binary(d, pn_bytes(x)); }
int pn_data_put_amqp_symbol(pn_data_t *d, const amqp_symbol& x) { return pn_data_put_symbol(d, pn_bytes(x)); }
}

encoder operator<<(encoder e, amqp_null) { pn_data_put_null(e.pn_object()); return e; }
encoder operator<<(encoder e, amqp_boolean x) { return insert(e, e.pn_object(), x, pn_data_put_bool); }
encoder operator<<(encoder e, amqp_ubyte x) { return insert(e, e.pn_object(), x, pn_data_put_ubyte); }
encoder operator<<(encoder e, amqp_byte x) { return insert(e, e.pn_object(), x, pn_data_put_byte); }
encoder operator<<(encoder e, amqp_ushort x) { return insert(e, e.pn_object(), x, pn_data_put_ushort); }
encoder operator<<(encoder e, amqp_short x) { return insert(e, e.pn_object(), x, pn_data_put_short); }
encoder operator<<(encoder e, amqp_uint x) { return insert(e, e.pn_object(), x, pn_data_put_uint); }
encoder operator<<(encoder e, amqp_int x) { return insert(e, e.pn_object(), x, pn_data_put_int); }
encoder operator<<(encoder e, amqp_char x) { return insert(e, e.pn_object(), x, pn_data_put_char); }
encoder operator<<(encoder e, amqp_ulong x) { return insert(e, e.pn_object(), x, pn_data_put_ulong); }
encoder operator<<(encoder e, amqp_long x) { return insert(e, e.pn_object(), x, pn_data_put_long); }
encoder operator<<(encoder e, amqp_timestamp x) { return insert(e, e.pn_object(), x, pn_data_put_timestamp); }
encoder operator<<(encoder e, amqp_float x) { return insert(e, e.pn_object(), x, pn_data_put_float); }
encoder operator<<(encoder e, amqp_double x) { return insert(e, e.pn_object(), x, pn_data_put_double); }
encoder operator<<(encoder e, amqp_decimal32 x) { return insert(e, e.pn_object(), x, pn_data_put_decimal32); }
encoder operator<<(encoder e, amqp_decimal64 x) { return insert(e, e.pn_object(), x, pn_data_put_decimal64); }
encoder operator<<(encoder e, amqp_decimal128 x) { return insert(e, e.pn_object(), x, pn_data_put_decimal128); }
encoder operator<<(encoder e, amqp_uuid x) { return insert(e, e.pn_object(), x, pn_data_put_uuid); }
encoder operator<<(encoder e, amqp_string x) { return insert(e, e.pn_object(), x, pn_data_put_amqp_string); }
encoder operator<<(encoder e, amqp_symbol x) { return insert(e, e.pn_object(), x, pn_data_put_amqp_symbol); }
encoder operator<<(encoder e, amqp_binary x) { return insert(e, e.pn_object(), x, pn_data_put_amqp_binary); }

encoder operator<<(encoder e, const value& v) {
    data edata = e.data();
    if (edata == v.data_) throw encode_error("cannot insert into self");
    data vdata = v.decode().data();
    check(edata.append(vdata), e.pn_object());
    return e;
}

encoder operator<<(encoder e, const scalar& x) {
    return insert(e, e.pn_object(), x.atom_, pn_data_put_atom);
}

encoder operator<<(encoder e, const message_id& x) { return e << x.scalar_; }
encoder operator<<(encoder e, const annotation_key& x) { return e << x.scalar_; }

}
