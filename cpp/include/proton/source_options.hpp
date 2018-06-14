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

#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"
#include "./duration.hpp"
#include "./source.hpp"

#include <string>

/// @file
/// @copybrief proton::source_options

namespace proton {

/// Options for creating a source node for a sender or receiver.
///
/// Options can be "chained".  For more information see @ref
/// proton::connection_options.
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

    /// Set the address for the source.  It is unset by default.  The
    /// address is ignored if dynamic() is true.
    PN_CPP_EXTERN source_options& address(const std::string&);

    /// Request that a node be dynamically created by the remote peer.
    /// The default is false.  Any specified source address() is
    /// ignored.
    PN_CPP_EXTERN source_options& dynamic(bool);

    /// Request an anonymous node on the remote peer.
    /// The default is false.  Any specified target address() is
    /// ignored if true.
    PN_CPP_EXTERN source_options& anonymous(bool);

    /// Control whether messages are browsed or consumed.  The
    /// default is source::MOVE, meaning consumed.
    PN_CPP_EXTERN source_options& distribution_mode(enum source::distribution_mode);

    /// Control the persistence of the source node.  The default is
    /// source::NONDURABLE, meaning non-persistent.
    PN_CPP_EXTERN source_options& durability_mode(enum source::durability_mode);

    /// The expiry period after which the source is discarded.  The
    /// default is no timeout.
    PN_CPP_EXTERN source_options& timeout(duration);

    /// Control when the clock for expiration begins.  The default is
    /// source::LINK_CLOSE.
    PN_CPP_EXTERN source_options& expiry_policy(enum source::expiry_policy);

    /// **Unsettled API** - Specify a filter mechanism on the source
    /// that restricts message flow to a subset of the available
    /// messages.
    PN_CPP_EXTERN source_options& filters(const source::filter_map&);

    /// Extension capabilities that are supported/requested
    PN_CPP_EXTERN source_options& capabilities(const std::vector<symbol>&);

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
