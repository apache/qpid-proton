#ifndef PROTON_TRACKER_HPP
#define PROTON_TRACKER_HPP

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

#include "./binary.hpp"
#include "./internal/export.hpp"
#include "./transfer.hpp"
#include "./messaging_handler.hpp"
#include "./transaction.hpp"

/// @file
/// @copybrief proton::tracker

struct pn_delivery_t;

namespace proton {

/// A tracker for a sent message. Every tracker exists within the
/// context of a sender.
///
/// A delivery attempt can fail. As a result, a particular message may
/// correspond to multiple trackers.
class tracker : public transfer {
    /// @cond INTERNAL
    tracker(pn_delivery_t* d);
    /// @endcond

    Transaction *transaction;
  public:
    /// Create an empty tracker.
    tracker() = default;

    /// Get the sender for this tracker.
    PN_CPP_EXTERN class sender sender() const;

    /// Get the tag for this tracker.
    PN_CPP_EXTERN binary tag() const;

   // set_transaction here is a problem. As evry time we call it will change
   // the pointer in current object and update won' be reflected in any copies of this tracker.
    PN_CPP_EXTERN void set_transaction(Transaction *t);

    PN_CPP_EXTERN Transaction* get_transaction() const;

    /// @cond INTERNAL
  friend class internal::factory<tracker>;
    /// @endcond
};

} // proton

#endif // PROTON_TRACKER_HPP
