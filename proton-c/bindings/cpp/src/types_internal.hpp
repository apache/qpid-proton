#ifndef TYPES_INTERNAL_HPP
#define TYPES_INTERNAL_HPP
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

#include "proton/internal/type_traits.hpp"
#include "proton/error.hpp"
#include "proton/binary.hpp"
#include <sstream>

///@file
/// Inline helpers for encode/decode/type conversion/ostream operators.

namespace proton {

/// Byte copy between two objects, only enabled if their sizes are equal.
template <class T, class U>
typename internal::enable_if<sizeof(T) == sizeof(U)>::type byte_copy(T &to, const U &from) {
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
    pn_bytes_t b = { s.size(), s.empty() ? 0 : const_cast<char*>(&s[0]) };
    return b;
}

inline pn_bytes_t pn_bytes(const binary& s) {
    pn_bytes_t b = { s.size(), s.empty() ? 0 : reinterpret_cast<const char*>(&s[0]) };
    return b;
}

inline std::string str(const pn_bytes_t& b) { return std::string(b.start, b.size); }
inline binary bin(const pn_bytes_t& b) { return binary(b.start, b.start+b.size); }

// Save all stream format state, restore in destructor.
struct ios_guard {
    std::ios &guarded;
    std::ios old;
    ios_guard(std::ios& x) : guarded(x), old(0) { old.copyfmt(guarded); }
    ~ios_guard() { guarded.copyfmt(old); }
};

// Convert a char (signed or unsigned) into an unsigned 1 byte integer that will ostream 
// as a numeric byte value, not a character and will not get sign-extended.
inline unsigned int printable_byte(uint8_t byte) { return byte; }

}
#endif // TYPES_INTERNAL_HPP
