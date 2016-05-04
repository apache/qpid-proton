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

    enum durability_mode {
      NONDURABLE = PN_NONDURABLE,
      CONFIGURATION = PN_CONFIGURATION,
      UNSETTLED_STATE = PN_DELIVERIES
    };

    enum expiry_policy {
      LINK_CLOSE = PN_EXPIRE_WITH_LINK,
      SESSION_CLOSE = PN_EXPIRE_WITH_SESSION,
      CONNECTION_CLOSE = PN_EXPIRE_WITH_CONNECTION,
      NEVER = PN_EXPIRE_NEVER
    };

    /// Control when the clock for expiration begins.
    PN_CPP_EXTERN enum expiry_policy expiry_policy() const;

    /// The period after which the source is discarded on expiry. The
    /// duration is rounded to the nearest second.
    PN_CPP_EXTERN duration timeout() const;

    /// Get the durability flag.
    PN_CPP_EXTERN enum durability_mode durability_mode();

    /// True if the remote node is created dynamically.
    PN_CPP_EXTERN bool dynamic() const;

    /// Obtain a reference to the AMQP dynamic node properties for the
    /// terminus.  See also lifetime_policy.
    PN_CPP_EXTERN value node_properties() const;

    /// @cond INTERNAL
  protected:
    pn_terminus_t *pn_object() { return object_; }
  private:
    pn_terminus_t* object_;
    pn_link_t* parent_;

  friend class internal::factory<terminus>;
  friend class source;
  friend class target;
    /// @endcond
};

}

#endif // PROTON_CPP_TERMINUS_H
