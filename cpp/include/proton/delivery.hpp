#ifndef PROTON_DELIVERY_HPP
#define PROTON_DELIVERY_HPP

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
#include "./transfer.hpp"

/// @file
/// @copybrief proton::delivery

namespace proton {

/// A received message.
/// 
/// A delivery attempt can fail. As a result, a particular message may
/// correspond to multiple deliveries.
class delivery : public transfer {
    /// @cond INTERNAL
    delivery(pn_delivery_t* d);
    /// @endcond

  public:
    delivery() {}

    PN_CPP_EXTERN ~delivery();

    /// Return the receiver for this delivery.
    PN_CPP_EXTERN class receiver receiver() const;

    // XXX ATM the following don't reflect the differing behaviors we
    // get from the different delivery modes. - Deferred

    /// Settle with ACCEPTED state.
    PN_CPP_EXTERN void accept();

    /// Settle with REJECTED state.
    PN_CPP_EXTERN void reject();

    /// Settle with RELEASED state.
    PN_CPP_EXTERN void release();

    /// Settle with MODIFIED state.
    PN_CPP_EXTERN void modify();

    /// @cond INTERNAL
  friend class internal::factory<delivery>;
    /// @endcond
};

} // proton

#endif // PROTON_DELIVERY_HPP
