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

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

/// A message transfer.  Every delivery exists within the context of a
/// proton::link.  A delivery attempt can fail. As a result, a
/// particular message may correspond to multiple deliveries.
class delivery : public internal::object<pn_delivery_t> {
    /// @cond INTERNAL
    delivery(pn_delivery_t* d) : internal::object<pn_delivery_t>(d) {}
    /// @endcond

  public:
    delivery() : internal::object<pn_delivery_t>(0) {}

    /// Return the link for this delivery
    PN_CPP_EXTERN class link link() const;

    /// Return the session for this delivery
    PN_CPP_EXTERN class session session() const;

    /// Return the connection for this delivery
    PN_CPP_EXTERN class connection connection() const;

    /// Return the container for this delivery
    PN_CPP_EXTERN class container &container() const;

    /// Delivery state values.
    enum state {
        NONE = 0,               ///< Unknown state
        RECEIVED = PN_RECEIVED, ///< Received but not yet settled
        ACCEPTED = PN_ACCEPTED, ///< Settled as accepted
        REJECTED = PN_REJECTED, ///< Settled as rejected
        RELEASED = PN_RELEASED, ///< Settled as released
        MODIFIED = PN_MODIFIED  ///< Settled as modified
    }; // AMQP spec 3.4 delivery State

    /// @cond INTERNAL
    /// XXX settle how much of this we need to expose
    
    /// Return true if the delivery has been settled.
    PN_CPP_EXTERN bool settled() const;

    /// Settle the delivery; informs the remote end.
    PN_CPP_EXTERN void settle();

    /// Set the local state of the delivery.
    PN_CPP_EXTERN void update(delivery::state state);

    /// Update and settle a delivery with the given delivery::state
    PN_CPP_EXTERN void settle(delivery::state s);

    /// @endcond

    /// Settle with ACCEPTED state
    PN_CPP_EXTERN void accept() { settle(ACCEPTED); }

    /// Settle with REJECTED state
    PN_CPP_EXTERN void reject() { settle(REJECTED); }

    /// Settle with RELEASED state
    PN_CPP_EXTERN void release() { settle(RELEASED); }

    /// Settle with MODIFIED state
    PN_CPP_EXTERN void modify() { settle(MODIFIED); }

    /// @cond INTERNAL
    /// XXX who needs this?
    
    /// Check if a delivery is readable.
    ///
    /// A delivery is considered readable if it is the current delivery on
    /// an incoming link.
    PN_CPP_EXTERN bool partial() const;

    /// Check if a delivery is writable.
    ///
    /// A delivery is considered writable if it is the current delivery on
    /// an outgoing link, and the link has positive credit.
    PN_CPP_EXTERN bool writable() const;

    /// Check if a delivery is readable.
    ///
    /// A delivery is considered readable if it is the current
    /// delivery on an incoming link.
    PN_CPP_EXTERN bool readable() const;

    /// Check if a delivery is updated.
    ///
    /// A delivery is considered updated whenever the peer
    /// communicates a new disposition for the delivery. Once a
    /// delivery becomes updated, it will remain so until clear() is
    /// called.
    PN_CPP_EXTERN bool updated() const;

    /// Clear the updated flag for a delivery.
    PN_CPP_EXTERN void clear();

    /// Get the size of the current delivery.
    PN_CPP_EXTERN size_t pending() const;

    /// @endcond

    /// Get the remote state for a delivery.
    PN_CPP_EXTERN state remote_state() const;

    /// @cond INTERNAL
    friend class proton_event;
    friend class sender;
    /// @endcond
};

}

#endif // PROTON_CPP_DELIVERY_H
