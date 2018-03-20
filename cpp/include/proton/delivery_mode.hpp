#ifndef PROTON_DELIVERY_MODE_H
#define PROTON_DELIVERY_MODE_H

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

/// @file
/// @copybrief proton::delivery_mode

namespace proton {

/// The message delivery policy to establish when opening a link.
/// This structure imitates the newer C++11 "enum class" so that
/// The enumeration constants are in the delivery_mode namespace.
struct delivery_mode {
    /// Delivery modes
    enum modes {
        /// No set policy.  The application must settle messages
        /// itself according to its own policy.
        NONE = 0,
        /// Outgoing messages are settled immediately by the link.
        /// There are no duplicates.
        AT_MOST_ONCE,
        /// The receiver settles the delivery first with an
        /// accept/reject/release disposition.  The sender waits to
        /// settle until after the disposition notification is
        /// received.
        AT_LEAST_ONCE
    };

    /// @cond INTERNAL
    
    delivery_mode() : modes_(NONE) {}
    delivery_mode(modes m) : modes_(m) {}
    operator modes() { return modes_; }

    /// @endcond

  private:
    modes modes_;
};

} // proton

#endif // PROTON_DELIVERY_MODE_H
