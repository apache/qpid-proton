#ifndef PROTON_CPP_SASL_H
#define PROTON_CPP_SASL_H

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
#include "proton/sasl.h"
#include <string>

namespace proton {

/// SASL information.
class sasl {
    /// @cond INTERNAL
    sasl(pn_sasl_t* s) : object_(s) {}
    /// @endcond

  public:
    sasl() : object_(0) {}

    /// The result of the SASL negotiation.
    enum outcome {
        NONE = PN_SASL_NONE,   ///< Negotiation not completed
        OK = PN_SASL_OK,       ///< Authentication succeeded
        AUTH = PN_SASL_AUTH,   ///< Failed due to bad credentials
        SYS = PN_SASL_SYS,     ///< Failed due to a system error
        PERM = PN_SASL_PERM,   ///< Failed due to unrecoverable error
        TEMP = PN_SASL_TEMP    ///< Failed due to transient error
    };

    /// @cond INTERNAL
    /// XXX need to discuss
    PN_CPP_EXTERN static bool extended();
    PN_CPP_EXTERN void done(enum outcome);
    /// @endcond

    /// Get the outcome.
    PN_CPP_EXTERN enum outcome outcome() const;

    /// Get the user name.
    PN_CPP_EXTERN std::string user() const;

    /// Get the mechanism.
    PN_CPP_EXTERN std::string mech() const;

    /// @cond INTERNAL
    PN_CPP_EXTERN void allow_insecure_mechs(bool);
    /// @endcond

    /// True if insecure mechanisms are permitted.
    PN_CPP_EXTERN bool allow_insecure_mechs();

    /// @cond INTERNAL
    /// XXX setters? versus connection options
    PN_CPP_EXTERN void allowed_mechs(const std::string &);
    PN_CPP_EXTERN void config_name(const std::string&);
    PN_CPP_EXTERN void config_path(const std::string&);
    /// @endcond

    /// @cond INTERNAL
  private:
    pn_sasl_t* object_;

    friend class transport;
    /// @endcond
};

}

#endif // PROTON_CPP_SASL_H
