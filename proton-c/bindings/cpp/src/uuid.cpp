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

#include "uuid.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace proton {

uuid::uuid() {
    static bool seeded = false;
    if (!seeded) {
        std::srand(std::time(0)); // use current time as seed for random generator
        seeded = true;
    }
    int r = std::rand();
    for (size_t i = 0; i < sizeof(bytes); ++i ) {
        bytes[i] = r & 0xFF;
        r >>= 8;
        if (!r) r = std::rand();
    }

    // From RFC4122, the version bits are set to 0100
    bytes[6] = (bytes[6] & 0x0F) | 0x40;

    // From RFC4122, the top two bits of byte 8 get set to 01
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
}

std::string uuid::str() {
    // UUID standard format: 8-4-4-4-12 (36 chars, 32 alphanumeric and 4 hypens)
    std::ostringstream s;
    s << std::hex << std::setw(2) << std::setfill('0');
    s << b(0) << b(1) << b(2) << b(3);
    s << '-' << b(4) << b(5) << '-' << b(6) << b(7) << '-' << b(8) << b(9);
    s << '-' << b(10) << b(11) << b(12) << b(13) << b(14) << b(15);
    return s.str();
}

}
