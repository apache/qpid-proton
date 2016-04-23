#ifndef PROTON_CPP_ERROR_CONDITION_H
#define PROTON_CPP_ERROR_CONDITION_H

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

#include "proton/export.hpp"
#include "proton/value.hpp"

#include "proton/config.hpp"

#include <string>
#include <iosfwd>

struct pn_condition_t;

namespace proton {

namespace internal {
class link;
}

/// Describes an endpoint error state.
class error_condition {
    /// @cond INTERNAL
    error_condition(pn_condition_t* c);
    /// @endcond

  public:
    error_condition() {}
    PN_CPP_EXTERN error_condition(std::string description);
    PN_CPP_EXTERN error_condition(std::string name, std::string description);
    PN_CPP_EXTERN error_condition(std::string name, std::string description, proton::value properties);

#if PN_CPP_HAS_CPP11
    error_condition(const error_condition&) = default;
    error_condition(error_condition&&) = default;
    error_condition& operator=(const error_condition&) = default;
    error_condition& operator=(error_condition&&) = default;
#endif

    /// No condition set.
    PN_CPP_EXTERN bool operator!() const;

    /// XXX add C++11 explicit bool conversion with a note about
    /// C++11-only usage

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

    /// @cond INTERNAL
  private:
    std::string name_;
    std::string description_;
    proton::value properties_;

    friend class transport;
    friend class connection;
    friend class session;
    friend class internal::link;
    /// @endcond
};

PN_CPP_EXTERN bool operator==(const error_condition& x, const error_condition& y);

PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, const error_condition& err);

}

#endif // PROTON_CPP_ERROR_CONDITION_H
