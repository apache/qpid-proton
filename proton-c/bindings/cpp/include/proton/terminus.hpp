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
#include "proton/object.hpp"
#include "proton/value.hpp"

#include "proton/link.h"
#include <string>

namespace proton {

class link;

/// One end of a link, either a source or a target.
///
/// The source terminus is where messages originate; the target
/// terminus is where they go.
///
/// @see proton::link
class terminus {
    /// @cond INTERNAL
    terminus(pn_terminus_t* t);
    /// @endcond

  public:
    terminus() : object_(0) {}

    /// Type of terminus
    enum type {
        TYPE_UNSPECIFIED = PN_UNSPECIFIED,
        SOURCE = PN_SOURCE,
        TARGET = PN_TARGET,
        COORDINATOR = PN_COORDINATOR ///< Transaction coordinator
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

    /// Get the terminus type.
    PN_CPP_EXTERN enum type type() const;

    /// Set the terminus type.
    PN_CPP_EXTERN void type(enum type);

    /// Get the expiration policy.
    PN_CPP_EXTERN enum expiry_policy expiry_policy() const;

    /// Set the expiration policy.
    PN_CPP_EXTERN void expiry_policy(enum expiry_policy);

    /// @cond INTERNAL
    /// XXX use duration
    PN_CPP_EXTERN uint32_t timeout() const;
    PN_CPP_EXTERN void timeout(uint32_t seconds);
    /// @endcond

    /// Get the distribution mode.
    PN_CPP_EXTERN enum distribution_mode distribution_mode() const;

    /// Set the distribution mode.
    PN_CPP_EXTERN void distribution_mode(enum distribution_mode);

    /// Get the durability flag.
    PN_CPP_EXTERN enum durability durability();

    /// Set the durability flag.
    PN_CPP_EXTERN void durability(enum durability);

    /// Get the source or target address.
    PN_CPP_EXTERN std::string address() const;

    /// Set the source or target address.
    PN_CPP_EXTERN void address(const std::string &);

    /// True if the remote node is created dynamically.
    PN_CPP_EXTERN bool dynamic() const;

    /// Enable or disable dynamic creation of the remote node.
    PN_CPP_EXTERN void dynamic(bool);

    /// Obtain a reference to the AMQP dynamic node properties for the
    /// terminus.  See also link_options::lifetime_policy.
    PN_CPP_EXTERN value& node_properties();

    /// Obtain a reference to the AMQP dynamic node properties for the
    /// terminus.  See also link_options::lifetime_policy.
    PN_CPP_EXTERN const value& node_properties() const;

    /// Obtain a reference to the AMQP filter set for the terminus.
    /// See also link_options::selector.
    PN_CPP_EXTERN value& filter();

    /// Obtain a reference to the AMQP filter set for the terminus.
    /// See also link_options::selector.
    PN_CPP_EXTERN const value& filter() const;

    /// @cond INTERNAL
  private:
    pn_terminus_t* object_;
    value properties_, filter_;

    friend class link;
    /// @endcond
};

}

#endif // PROTON_CPP_TERMINUS_H
