#ifndef PROTON_CPP_CONDITION_H
#define PROTON_CPP_CONDITION_H

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

#include <string>

struct pn_condition_t;

namespace proton {

/// Describes an endpoint error state.
class condition {
  public:
    /// @cond INTERNAL
    condition(pn_condition_t* c) : condition_(c) {}
    /// @endcond

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
    PN_CPP_EXTERN value info() const;

    /// @cond INTERNAL
    /// Simple printable string for condition.
    /// XXX not sure about this as a convention 
    PN_CPP_EXTERN std::string str() const;
    /// @endcond

  private:
    pn_condition_t* condition_;
};

}

#endif // PROTON_CPP_CONDITION_H
