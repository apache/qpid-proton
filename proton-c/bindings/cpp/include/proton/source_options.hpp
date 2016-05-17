#ifndef PROTON_SOURCE_OPTIONS_HPP
#define PROTON_SOURCE_OPTIONS_HPP

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
#include "./source.hpp"

#include <vector>
#include <string>

namespace proton {

class proton_handler;

/// Options for creating a source node for a sender or receiver.
///
/// Options can be "chained" (@see proton::connection_options).
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class source_options {
  public:

    /// Create an empty set of options.
    PN_CPP_EXTERN source_options();

    /// Copy options.
    PN_CPP_EXTERN source_options(const source_options&);

    PN_CPP_EXTERN ~source_options();

    /// Copy options.
    PN_CPP_EXTERN source_options& operator=(const source_options&);

    /// Set the address for the source.  Ignored if dynamic is true.
    PN_CPP_EXTERN source_options& address(const std::string&);

    /// Request a dynamically created node to be created by the remote peer.
    /// Any specified source address is ignored.
    PN_CPP_EXTERN source_options& dynamic(bool);

    /// Control whether messsages are browsed or consumed.
    PN_CPP_EXTERN source_options& distribution_mode(enum source::distribution_mode);

    /// Control the persistence of source state.
    PN_CPP_EXTERN source_options& durability_mode(enum source::durability_mode);

    /// The expiry period after which the source is discarded.
    PN_CPP_EXTERN source_options& timeout(duration);

    /// Control when the clock for expiration begins.
    PN_CPP_EXTERN source_options& expiry_policy(enum source::expiry_policy);

    /// **Experimental** - Specify a filter mechanism on the source
    /// that restricts message flow to a subset of the available
    /// messages.
    PN_CPP_EXTERN source_options& filters(const source::filter_map&);

  private:
    void apply(source&) const;

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class source;
  friend class sender_options;
  friend class receiver_options;
    /// @endcond
};

} // proton

#endif // PROTON_SOURCE_OPTIONS_HPP
