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
#include "./endpoint.hpp"
#include "./internal/export.hpp"
#include "./transfer.hpp"

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

  public:
    /// Create an empty tracker.
    tracker() = default;

    /// Get the sender for this tracker.
    PN_CPP_EXTERN class sender sender() const;

    /// Get the tag for this tracker.
    PN_CPP_EXTERN binary tag() const;

    /// @cond INTERNAL
  friend class internal::factory<tracker>;
  friend class sender;
    /// @endcond
};

/// @cond INTERNAL

/// An iterator of unsettled trackers on a sender.
class tracker_iterator : public internal::iter_base<tracker, tracker_iterator> {
  tracker_iterator(tracker t) :
      internal::iter_base<tracker, tracker_iterator>(t) {}

public:
  explicit tracker_iterator() :
      internal::iter_base<tracker, tracker_iterator>(tracker()) {}
  /// Advance to the next unsettled tracker.
  PN_CPP_EXTERN tracker_iterator operator++();

  friend class sender;
};

/// A range of unsettled trackers.
typedef internal::iter_range<tracker_iterator> tracker_range;

/// @endcond

} // proton

#endif // PROTON_TRACKER_HPP
