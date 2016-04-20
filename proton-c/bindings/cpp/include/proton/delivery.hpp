#ifndef PROTON_CPP_DELIVERY_H
#define PROTON_CPP_DELIVERY_H

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
#include "proton/transfer.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

class receiver;

/// A message transfer.  Every delivery exists within the context of a
/// proton::receiver.  A delivery attempt can fail. As a result, a
/// particular message may correspond to multiple deliveries.
class delivery : public transfer {
    /// @cond INTERNAL
    delivery(pn_delivery_t* d);
    /// @endcond

  public:
    delivery() {}

    // Return the receiver for this delivery
    PN_CPP_EXTERN class receiver receiver() const;

    /// Settle with ACCEPTED state
    PN_CPP_EXTERN void accept() { settle(ACCEPTED); }

    /// Settle with REJECTED state
    PN_CPP_EXTERN void reject() { settle(REJECTED); }

    /// Settle with RELEASED state
    PN_CPP_EXTERN void release() { settle(RELEASED); }

    /// Settle with MODIFIED state
    PN_CPP_EXTERN void modify() { settle(MODIFIED); }

    /// @cond INTERNAL
  private:
    /// Get the size of the current delivery.
    size_t pending() const;

    /// Check if a delivery is complete.
    bool partial() const;

    /// Check if a delivery is readable.
    ///
    /// A delivery is considered readable if it is the current
    /// delivery on a receiver.
    bool readable() const;

  friend class internal::factory<delivery>;
  friend class message;
  friend class messaging_adapter;
    /// @endcond
};

}

#endif // PROTON_CPP_DELIVERY_H
