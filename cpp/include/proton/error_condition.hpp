#ifndef PROTON_ERROR_CONDITION_H
#define PROTON_ERROR_CONDITION_H

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
#include "./internal/config.hpp"
#include "./value.hpp"

#include <string>
#include <iosfwd>

/// @file
/// @copybrief proton::error_condition

struct pn_condition_t;

namespace proton {

/// Describes an endpoint error state.
class error_condition {
    /// @cond INTERNAL
    error_condition(pn_condition_t* c);
    /// @endcond

  public:
    /// Create an empty error condition.
    error_condition() {}

    /// Create an error condition with only a description. A default
    /// name will be used ("proton:io:error").
    PN_CPP_EXTERN error_condition(std::string description);

    /// Create an error condition with a name and description.
    PN_CPP_EXTERN error_condition(std::string name, std::string description);

    /// **Unsettled API** - Create an error condition with name,
    /// description, and informational properties.
    PN_CPP_EXTERN error_condition(std::string name, std::string description, proton::value properties);

#if PN_CPP_HAS_DEFAULTED_FUNCTIONS && PN_CPP_HAS_DEFAULTED_MOVE_INITIALIZERS
    /// @cond INTERNAL
    error_condition(const error_condition&) = default;
    error_condition& operator=(const error_condition&) = default;
    error_condition(error_condition&&) = default;
    error_condition& operator=(error_condition&&) = default;
    /// @endcond
#endif

#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
    /// If you are using a C++11 compiler, you may use an
    /// error_condition in boolean contexts. The expression will be
    /// true if the error_condition is set.
    PN_CPP_EXTERN explicit operator bool() const;
#endif

    /// No condition set.
    PN_CPP_EXTERN bool operator!() const;

    /// No condition has been set.
    PN_CPP_EXTERN bool empty() const;

    /// Condition name.
    PN_CPP_EXTERN std::string name() const;

    /// Descriptive string for condition.
    PN_CPP_EXTERN std::string description() const;

    /// Extra information for condition.
    PN_CPP_EXTERN value properties() const;

    /// Simple printable string for condition.
    PN_CPP_EXTERN std::string what() const;

  private:
    std::string name_;
    std::string description_;
    proton::value properties_;

    /// @cond INTERNAL
  friend class internal::factory<error_condition>;
    /// @endcond
};

/// @return true if name, description and properties are all equal
PN_CPP_EXTERN bool operator==(const error_condition& x, const error_condition& y);

/// Human readable string
PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, const error_condition& err);

} // proton

#endif // PROTON_ERROR_CONDITION_H
