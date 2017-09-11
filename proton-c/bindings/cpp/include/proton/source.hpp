#ifndef PROTON_SOURCE_HPP
#define PROTON_SOURCE_HPP

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
#include "./map.hpp"
#include "./symbol.hpp"
#include "./terminus.hpp"
#include "./value.hpp"

#include <string>

/// @file
/// @copybrief proton::source

struct pn_terminus_t;

namespace proton {

/// A point of origin for messages.
///
/// @see proton::sender, proton::receiver, proton::target
class source : public terminus {
  public:
    /// **Unsettled API** - A map of AMQP symbol keys and filter
    /// specifiers.
    typedef map<symbol, value> filter_map;

    /// Create an empty source.
    source() : terminus() {}

    /// The policy for distributing messages.
    enum distribution_mode {
        // XXX Why is unspecified needed?  The protocol doesn't have
        // it.
        /// Unspecified
        UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED,
        /// Once transferred, the message remains available to other links
        COPY = PN_DIST_MODE_COPY,
        /// Once transferred, the message is unavailable to other links
        MOVE = PN_DIST_MODE_MOVE
    };

    using terminus::durability_mode;
    using terminus::expiry_policy;

    /// The address of the source.
    PN_CPP_EXTERN std::string address() const;

    /// Get the distribution mode.
    PN_CPP_EXTERN enum distribution_mode distribution_mode() const;

    /// **Unsettled API** - Obtain the set of message filters.
    PN_CPP_EXTERN const filter_map& filters() const;

  private:
    source(pn_terminus_t* t);
    source(const sender&);
    source(const receiver&);

    filter_map filters_;

    /// @cond INTERNAL
  friend class proton::internal::factory<source>;
  friend class sender;
  friend class receiver;
    /// @endcond
};

} // proton

#endif // PROTON_SOURCE_HPP
