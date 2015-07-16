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

#include "proton/value.hpp"
#include "proton_bits.hpp"
#include <proton/codec.h>
#include <ostream>

namespace proton {

values::values() {}
values::values(const values& v) { *this = v; }
values::values(pn_data_t* d) : data(d) {}

values::~values() {}
values& values::operator=(const values& v) { data::operator=(v); return *this; }

std::ostream& operator<<(std::ostream& o, const values& v) {
    return o << static_cast<const encoder&>(v);
}

}
