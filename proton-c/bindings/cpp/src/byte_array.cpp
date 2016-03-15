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

#include <proton/byte_array.hpp>

#include <ostream>
#include <iomanip>

namespace proton {
namespace internal {

void print_hex(std::ostream& o, const uint8_t* p, size_t n) {
    ios_guard restore_flags(o);
    o << "0x" << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) {
        o << std::setw(2) << printable_byte(p[i]);
    }
}

}}
