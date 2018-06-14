#ifndef PROTON_TERMINUS_HPP
#define PROTON_TERMINUS_HPP

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

#include "./types_fwd.hpp"
#include "./internal/export.hpp"

#include <proton/terminus.h>

#include <string>
#include <vector>

/// @file
/// @copybrief proton::terminus

struct pn_link_t;
struct pn_terminus_t;

namespace proton {

namespace internal {
template <class T> class factory;
}

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
    terminus() : object_(0), parent_(0) {}

    /// The persistence mode of the source or target.
    enum durability_mode {
        /// No persistence.
        NONDURABLE = PN_NONDURABLE,
        /// Only configuration is persisted.
        CONFIGURATION = PN_CONFIGURATION,
        /// Configuration and unsettled state are persisted.
        UNSETTLED_STATE = PN_DELIVERIES
    };

    /// When expiration of the source or target begins.
    enum expiry_policy {
        /// When the link is closed.
        LINK_CLOSE = PN_EXPIRE_WITH_LINK,
        /// When the containing session is closed.
        SESSION_CLOSE = PN_EXPIRE_WITH_SESSION,
        /// When the containing connection is closed.
        CONNECTION_CLOSE = PN_EXPIRE_WITH_CONNECTION,
        /// The terminus never expires.
        NEVER = PN_EXPIRE_NEVER
    };

    // XXX This should have address?

    /// Get the policy for when expiration begins.
    PN_CPP_EXTERN enum expiry_policy expiry_policy() const;

    /// The period after which the source is discarded on expiry. The
    /// duration is rounded to the nearest second.
    PN_CPP_EXTERN duration timeout() const;

    /// Get the durability flag.
    PN_CPP_EXTERN enum durability_mode durability_mode();

    /// True if the remote node is created dynamically.
    PN_CPP_EXTERN bool dynamic() const;

    /// True if the remote node is an anonymous-relay
    PN_CPP_EXTERN bool anonymous() const;

    /// Obtain a reference to the AMQP dynamic node properties for the
    /// terminus.  See also lifetime_policy.
    PN_CPP_EXTERN value node_properties() const;

    /// Extension capabilities that are supported/requested
    PN_CPP_EXTERN std::vector<symbol> capabilities() const;

  protected:
    pn_terminus_t *pn_object() const { return object_; }
  private:
    pn_terminus_t* object_;
    pn_link_t* parent_;

    /// @cond INTERNAL
  friend class internal::factory<terminus>;
  friend class source;
  friend class target;
    /// @endcond
};

} // proton

#endif // PROTON_TERMINUS_HPP
