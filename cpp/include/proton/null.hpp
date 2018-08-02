#ifndef PROTON_NULL_HPP
#define PROTON_NULL_HPP

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

/// @file
/// @copybrief proton::null

#include "./internal/config.hpp"
#include "./internal/comparable.hpp"
#include "./internal/export.hpp"

#include <iosfwd>

namespace proton {

/// The type of the AMQP null value
///
/// @see @ref types_page
class null : private internal::comparable<null> {
  public:
    null() {}
#if PN_CPP_HAS_NULLPTR
    /// Constructed from nullptr literal
    null(decltype(nullptr)) {}
#endif
    /// null instances are always equal
  friend bool operator==(const null&, const null&) { return true; }
    /// null instances are never unequal
  friend bool operator<(const null&, const null&) { return false; }
};

/// Print a null value
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const null&);

}

#endif // PROTON_NULL_HPP
