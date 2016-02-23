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

#include "proton/type_traits.hpp"
#include "proton/error.hpp"
#include <sstream>

///@file
/// Internal helpers for encode/decode/type conversion.

namespace proton {

/// Byte copy between two objects, only enabled if their sizes are equal.
template <class T, class U>
typename enable_if<sizeof(T) == sizeof(U)>::type byte_copy(T &to, const U &from) {
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

}

#endif // CODEC_HPP
