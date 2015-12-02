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

class sasl {
  public:
    /** The result of the SASL negotiation */
    enum outcome_t {
        NONE = PN_SASL_NONE,   /** negotiation not completed */
        OK = PN_SASL_OK,       /** authentication succeeded */
        AUTH = PN_SASL_AUTH,   /** failed due to bad credentials */
        SYS = PN_SASL_SYS,     /** failed due to a system error */
        PERM = PN_SASL_PERM,   /** failed due to unrecoverable error */
        TEMP = PN_SASL_TEMP    /** failed due to transient error */
    };

    sasl(pn_sasl_t* s) : object_(s) {}
    PN_CPP_EXTERN static bool extended();
    PN_CPP_EXTERN void done(outcome_t);
    PN_CPP_EXTERN outcome_t outcome() const;
    PN_CPP_EXTERN std::string user() const;
    PN_CPP_EXTERN std::string mech() const;

    PN_CPP_EXTERN void allow_insecure_mechs(bool);
    PN_CPP_EXTERN bool allow_insecure_mechs();
    PN_CPP_EXTERN void allowed_mechs(const std::string &);
    PN_CPP_EXTERN void config_name(const std::string&);
    PN_CPP_EXTERN void config_path(const std::string&);
private:
    pn_sasl_t* object_;
};

}

#endif  /*!PROTON_CPP_SASL_H*/
