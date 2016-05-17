#ifndef PROTON_TRANSFER_HPP
#define PROTON_TRANSFER_HPP

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
#include "proton/internal/object.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

/// The base class for delivery and tracker.
class transfer : public internal::object<pn_delivery_t> {
    /// @cond INTERNAL
    transfer(pn_delivery_t* d) : internal::object<pn_delivery_t>(d) {}
    /// @endcond

  public:
    /// Create an empty transfer.
    transfer() : internal::object<pn_delivery_t>(0) {}

    /// Return the session for this transfer.
    PN_CPP_EXTERN class session session() const;

    /// Return the connection for this transfer.
    PN_CPP_EXTERN class connection connection() const;

    /// Return the container for this transfer.
    PN_CPP_EXTERN class container &container() const;

    /// Settle the delivery; informs the remote end.
    PN_CPP_EXTERN void settle();

    /// Return true if the transfer has been settled.
    PN_CPP_EXTERN bool settled() const;

  protected:
    /// Delivery state values.
    enum state {
        NONE = 0,               ///< Unknown state
        RECEIVED = PN_RECEIVED, ///< Received but not yet settled
        ACCEPTED = PN_ACCEPTED, ///< Settled as accepted
        REJECTED = PN_REJECTED, ///< Settled as rejected
        RELEASED = PN_RELEASED, ///< Settled as released
        MODIFIED = PN_MODIFIED  ///< Settled as modified
    }; // AMQP spec 3.4 delivery State

    /// Set the local state of the delivery.
    void update(enum state state);

    /// Update and settle a delivery with the given delivery::state
    void settle(enum state s);

    /// Get the remote state for a delivery.
    enum state state() const;

    /// @cond INTERNAL
  friend class internal::factory<transfer>;
    /// @endcond
};

} // proton

#endif // PROTON_TRANSFER_HPP
