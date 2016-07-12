#ifndef PROTON_TARGET_OPTIONS_HPP
#define PROTON_TARGET_OPTIONS_HPP

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

#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"
#include "./types.hpp"
#include "./delivery_mode.hpp"
#include "./target.hpp"
#include "./terminus.hpp"

#include <vector>
#include <string>

namespace proton {

class proton_handler;
class target;

/// Options for creating a target node for a sender or receiver.
///
/// Options can be "chained" (see proton::connection_options).
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class target_options {
  public:
    /// Create an empty set of options.
    PN_CPP_EXTERN target_options();

    /// Copy options.
    PN_CPP_EXTERN target_options(const target_options&);

    PN_CPP_EXTERN ~target_options();

    /// Copy options.
    PN_CPP_EXTERN target_options& operator=(const target_options&);

    /// Set the address for the target.  Ignored if dynamic is true.
    PN_CPP_EXTERN target_options& address(const std::string& addr);

    /// Request a dynamically created node to be created by the peer.
    /// Any specified target address is ignored.
    PN_CPP_EXTERN target_options& dynamic(bool);

    /// Control the persistence of target state.
    PN_CPP_EXTERN target_options& durability_mode(enum target::durability_mode);

    /// The expiry period after which the target is discarded.
    PN_CPP_EXTERN target_options& timeout(duration);

    /// Control when the clock for expiration begins.
    PN_CPP_EXTERN target_options& expiry_policy(enum target::expiry_policy);

  private:
    void apply(target&) const;

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class target;
  friend class sender_options;
  friend class receiver_options;
    /// @endcond
};

} // proton

#endif // PROTON_TARGET_OPTIONS_HPP
