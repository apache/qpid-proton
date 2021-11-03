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

#include "types_internal.hpp"

#include "proton/uuid.hpp"
#include "proton/types_fwd.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iomanip>
#include <random>
#include <sstream>

#if defined(_WIN32)
#include <process.h>
#define GETPID _getpid
#else
#include <unistd.h>
#define GETPID getpid
#endif

namespace proton {

namespace {

thread_local std::independent_bits_engine<std::mt19937, CHAR_BIT, unsigned int> engine = [] {
  // A hash of time, PID and random_device to produce an unsigned integer
  // for seeding the engine.
  std::seed_seq seq{
      (unsigned long)std::chrono::system_clock::now().time_since_epoch().count(),
      (unsigned long)GETPID(),
      (unsigned long)std::random_device{}()};

  return
      std::independent_bits_engine<std::mt19937, CHAR_BIT, unsigned int>(seq);
}();

} // namespace

uuid uuid::copy() {
    uuid u;
    std::fill(u.begin(), u.end(), 0);
    return u;
}

uuid uuid::copy(const char* bytes) {
    uuid u;
    if (bytes)
        std::copy(bytes, bytes + u.size(), u.begin());
    else
        std::fill(u.begin(), u.end(), 0);
    return u;
}

uuid uuid::random() {
    uuid bytes;
    std::generate(bytes.begin(), bytes.end(), std::ref(engine));

    // From RFC4122, the version bits are set to 0100
    bytes[6] = (bytes[6] & 0x0F) | 0x40;

    // From RFC4122, the top two bits of byte 8 get set to 01
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    return bytes;
}

/// UUID standard format: 8-4-4-4-12 (36 chars, 32 alphanumeric and 4 hyphens)
std::ostream& operator<<(std::ostream& o, const uuid& u) {
    ios_guard restore_flags(o);
    o << std::hex << std::setfill('0');
    static const int segments[] = {4,2,2,2,6}; // 1 byte is 2 hex chars.
    const uint8_t *p = reinterpret_cast<const uint8_t*>(u.begin());
    for (size_t i = 0; i < sizeof(segments)/sizeof(segments[0]); ++i) {
        if (i > 0)
            o << '-';
        for (int j = 0; j < segments[i]; ++j) {
            o << std::setw(2) << printable_byte(*(p++));
        }
    }
    return o;
}

std::string uuid::str() const {
    std::ostringstream s;
    s << *this;
    return s.str();
}

}
