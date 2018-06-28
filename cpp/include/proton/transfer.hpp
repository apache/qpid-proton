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

#include "./fwd.hpp"
#include "./internal/export.hpp"
#include "./internal/object.hpp"

#include <proton/disposition.h>
#include <iosfwd>

/// @file
/// @copybrief proton::transfer

struct pn_delivery_t;

namespace proton {

/// The base class for delivery and tracker.
class transfer : public internal::object<pn_delivery_t> {
    /// @cond INTERNAL
    transfer(pn_delivery_t* d) : internal::object<pn_delivery_t>(d) {}
    /// @endcond

  public:
    /// Create an empty transfer.
    transfer() : internal::object<pn_delivery_t>(0) {}

    /// Delivery state values.
    enum state {
        NONE = 0,               ///< Unknown state
        RECEIVED = PN_RECEIVED, ///< Received but not yet settled
        ACCEPTED = PN_ACCEPTED, ///< Settled as accepted
        REJECTED = PN_REJECTED, ///< Settled as rejected
        RELEASED = PN_RELEASED, ///< Settled as released
        MODIFIED = PN_MODIFIED  ///< Settled as modified
    }; // AMQP spec 3.4 delivery State

    /// Get the remote state for a delivery.
    PN_CPP_EXTERN enum state state() const;

    /// Return the session for this transfer.
    PN_CPP_EXTERN class session session() const;

    /// Return the connection for this transfer.
    PN_CPP_EXTERN class connection connection() const;

    /// Get the work_queue for the transfer.
    PN_CPP_EXTERN class work_queue& work_queue() const;

    /// Return the container for this transfer.
    PN_CPP_EXTERN class container &container() const;

    /// Settle the delivery; informs the remote end.
    PN_CPP_EXTERN void settle();

    /// Return true if the transfer has been settled.
    PN_CPP_EXTERN bool settled() const;

    /// @cond INTERNAL
  friend class internal::factory<transfer>;
    /// @endcond
};

/// Human-readalbe name of the transfer::state
PN_CPP_EXTERN std::string to_string(enum transfer::state);
/// Human-readalbe name of the transfer::state
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const enum transfer::state);

} // proton

#endif // PROTON_TRANSFER_HPP
