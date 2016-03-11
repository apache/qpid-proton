#ifndef CODEC_HPP
#define CODEC_HPP
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

#include <proton/type_traits.hpp>
#include <proton/error.hpp>
#include <proton/binary.hpp>
#include <sstream>

///@file
/// Internal helpers for encode/decode/type conversion.

namespace proton {

/// Byte copy between two objects, only enabled if their sizes are equal.
template <class T, class U>
typename codec::enable_if<sizeof(T) == sizeof(U)>::type byte_copy(T &to, const U &from) {
    const char *p = reinterpret_cast<const char*>(&from);
    std::copy(p, p + sizeof(T), reinterpret_cast<char*>(&to));
}

inline conversion_error
make_conversion_error(type_id want, type_id got, const std::string& msg=std::string()) {
    std::ostringstream s;
    s << "unexpected type, want: " << want << " got: " << got;
    if (!msg.empty()) s << ": " << msg;
    return conversion_error(s.str());
}

/// Convert std::string to pn_bytes_t
inline pn_bytes_t pn_bytes(const std::string& s) {
    pn_bytes_t b = { s.size(), const_cast<char*>(&s[0]) };
    return b;
}

inline pn_bytes_t pn_bytes(const binary& s) {
    pn_bytes_t b = { s.size(), const_cast<char*>(&s[0]) };
    return b;
}

inline std::string str(const pn_bytes_t& b) { return std::string(b.start, b.size); }
inline binary bin(const pn_bytes_t& b) { return binary(b.start, b.start+b.size); }

inline bool type_id_is_signed_int(type_id t) { return t == BYTE || t == SHORT || t == INT || t == LONG; }
inline bool type_id_is_unsigned_int(type_id t) { return t == UBYTE || t == USHORT || t == UINT || t == ULONG; }
inline bool type_id_is_integral(type_id t) { return t == BOOLEAN || t == CHAR || t == TIMESTAMP || type_id_is_unsigned_int(t) || type_id_is_signed_int(t); }
inline bool type_id_is_floating_point(type_id t) { return t == FLOAT || t == DOUBLE; }
inline bool type_id_is_decimal(type_id t) { return t == DECIMAL32 || t == DECIMAL64 || t == DECIMAL128; }
inline bool type_id_is_signed(type_id t) { return type_id_is_signed_int(t) || type_id_is_floating_point(t) || type_id_is_decimal(t); }
inline bool type_id_is_string_like(type_id t) { return t == BINARY || t == STRING || t == SYMBOL; }
inline bool type_id_is_container(type_id t) { return t == LIST || t == MAP || t == ARRAY || t == DESCRIBED; }
inline bool type_id_is_scalar(type_id t) { return type_id_is_integral(t) || type_id_is_floating_point(t) || type_id_is_decimal(t) || type_id_is_string_like(t) || t == TIMESTAMP || t == UUID; }

}

#endif // CODEC_HPP
