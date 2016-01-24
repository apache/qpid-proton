#ifndef PROTON_CPP_TERMINUS_H
#define PROTON_CPP_TERMINUS_H

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

#include "proton/link.h"
#include "proton/object.hpp"
#include "proton/value.hpp"
#include <string>

namespace proton {

class link;

/** A terminus represents one end of a link.
 * The source terminus is where messages originate, the target terminus is where they go.
 */
class terminus
{
  public:
    terminus(pn_terminus_t* t);

    /// Type of terminus
    enum type{
        TYPE_UNSPECIFIED = PN_UNSPECIFIED,
        SOURCE = PN_SOURCE,
        TARGET = PN_TARGET,
        COORDINATOR = PN_COORDINATOR ///< Transaction co-ordinator
    };

    /// Durability
    enum durability {
        NONDURABLE = PN_NONDURABLE,
        CONFIGURATION = PN_CONFIGURATION,
        DELIVERIES = PN_DELIVERIES
    };

    /// Expiry policy
    enum expiry_policy {
        EXPIRE_WITH_LINK = PN_EXPIRE_WITH_LINK,
        EXPIRE_WITH_SESSION = PN_EXPIRE_WITH_SESSION,
        EXPIRE_WITH_CONNECTION = PN_EXPIRE_WITH_CONNECTION,
        EXPIRE_NEVER = PN_EXPIRE_NEVER
    };

    /// Distribution mode
    enum distribution_mode {
        MODE_UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED,
        COPY = PN_DIST_MODE_COPY,
        MOVE = PN_DIST_MODE_MOVE
    };

    PN_CPP_EXTERN enum type type() const;
    PN_CPP_EXTERN void type(enum type);
    PN_CPP_EXTERN enum expiry_policy expiry_policy() const;
    PN_CPP_EXTERN void expiry_policy(enum expiry_policy);
    PN_CPP_EXTERN uint32_t timeout() const;
    PN_CPP_EXTERN void timeout(uint32_t seconds);
    PN_CPP_EXTERN enum distribution_mode distribution_mode() const;
    PN_CPP_EXTERN void distribution_mode(enum distribution_mode);
    PN_CPP_EXTERN enum durability durability();
    PN_CPP_EXTERN void durability(enum durability);
    PN_CPP_EXTERN std::string address() const;
    PN_CPP_EXTERN void address(const std::string &);
    PN_CPP_EXTERN bool dynamic() const;
    PN_CPP_EXTERN void dynamic(bool);

    /** Obtain a reference to the AMQP dynamic node properties for the terminus.
     * See also link_options::lifetime_policy. */
    PN_CPP_EXTERN value& node_properties();
    PN_CPP_EXTERN const value& node_properties() const;

    /** Obtain a reference to the AMQP filter set for the terminus.
     * See also link_options::selector. */
    PN_CPP_EXTERN value& filter();
    PN_CPP_EXTERN const value& filter() const;

private:
    pn_terminus_t* object_;
    value properties_, filter_;
};


}

#endif  /*!PROTON_CPP_TERMINUS_H*/
