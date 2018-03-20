#ifndef PROTON_UUID_HPP
#define PROTON_UUID_HPP

/*
 *
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
 *
 */

#include "./internal/export.hpp"
#include "./byte_array.hpp"

#include <string>
#include <iosfwd>

/// @file
/// @copybrief proton::uuid

namespace proton {

/// A 16-byte universally unique identifier.
class uuid : public byte_array<16> {
  public:
    /// Make a copy.
    PN_CPP_EXTERN static uuid copy();

    /// Return a uuid copied from bytes.  Bytes must point to at least
    /// 16 bytes.  If `bytes == 0` the UUID is zero-initialized.
    PN_CPP_EXTERN static uuid copy(const char* bytes);

    /// Return a simple randomly-generated UUID.  This is used by the
    /// Proton library to generate default UUIDs.
    ///
    /// For specific security, performance, or uniqueness
    /// requirements, you may want to use a better UUID generator or
    /// some other form of identifier entirely.
    PN_CPP_EXTERN static uuid random();

    /// UUID standard string format: 8-4-4-4-12 (36 chars, 32
    /// alphanumeric chars and 4 hyphens).
    PN_CPP_EXTERN std::string str()  const;
};

/// UUID standard format: 8-4-4-4-12 (36 chars, 32 alphanumeric chars
/// and 4 hyphens).
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const uuid&);

} // proton

#endif // PROTON_UUID_HPP
