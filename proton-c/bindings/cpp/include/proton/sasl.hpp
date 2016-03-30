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
#include "proton/config.hpp"
#include "proton/sasl.h"
#include <string>

namespace proton {

/// SASL information.
class sasl {
    /// @cond INTERNAL
    sasl(pn_sasl_t* s) : object_(s) {}
    /// @endcond

  public:
#if PN_CPP_HAS_CPP11
    sasl() = delete;
    sasl(const sasl&) = delete;
    sasl(sasl&&) = default;
    sasl& operator=(const sasl&) = delete;
    sasl& operator=(sasl&&) = delete;
#endif

    /// The result of the SASL negotiation.
    enum outcome {
        NONE = PN_SASL_NONE,   ///< Negotiation not completed
        OK = PN_SASL_OK,       ///< Authentication succeeded
        AUTH = PN_SASL_AUTH,   ///< Failed due to bad credentials
        SYS = PN_SASL_SYS,     ///< Failed due to a system error
        PERM = PN_SASL_PERM,   ///< Failed due to unrecoverable error
        TEMP = PN_SASL_TEMP    ///< Failed due to transient error
    };

    /// Get the outcome.
    PN_CPP_EXTERN enum outcome outcome() const;

    /// Get the user name.
    PN_CPP_EXTERN std::string user() const;

    /// Get the mechanism.
    PN_CPP_EXTERN std::string mech() const;

    /// @cond INTERNAL
  private:
    void allow_insecure_mechs(bool);
    bool allow_insecure_mechs();
    void allowed_mechs(const std::string &);
    void config_name(const std::string&);
    void config_path(const std::string&);

    pn_sasl_t* const object_;

    friend class connection_options;
    friend class transport;
    /// @endcond
};

}

#endif // PROTON_CPP_SASL_H
