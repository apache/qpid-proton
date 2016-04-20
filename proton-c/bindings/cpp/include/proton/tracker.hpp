#ifndef PROTON_CPP_TRACKER_H
#define PROTON_CPP_TRACKER_H

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
#include "proton/transfer.hpp"

struct pn_delivery_t;

namespace proton {

/// A message transfer.  Every delivery exists within the context of a
/// proton::link.  A delivery attempt can fail. As a result, a
/// particular message may correspond to multiple deliveries.
class tracker : public transfer {
    /// @cond INTERNAL
    tracker(pn_delivery_t* d);
    /// @endcond

  public:
    tracker() {}

    // Return the sender for this tracker
    PN_CPP_EXTERN class sender sender() const;

  friend class internal::factory<tracker>;
};

}

#endif // PROTON_CPP_TRACKER_H
