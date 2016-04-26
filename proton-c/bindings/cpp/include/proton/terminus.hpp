#ifndef PROTON_CPP_TERMINUS_H
#define PROTON_CPP_TERMINUS_H

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
#include "proton/object.hpp"
#include "proton/value.hpp"
#include "proton/settings.hpp"

#include "proton/link.h"
#include <string>

namespace proton {

class source;
class target;
class receiver_options;
class source_options;
class target_options;


namespace internal {

/// One end of a link, either a source or a target.
///
/// The source terminus is where messages originate; the target
/// terminus is where they go.
///
/// @see proton::link
class terminus {
    /// @cond INTERNAL
    terminus(pn_terminus_t* t);
    /// @endcond

  public:
    terminus() : object_(0), parent_(0) {}

    /// Control when the clock for expiration begins.
    PN_CPP_EXTERN enum expiry_policy expiry_policy() const;

    /// The period after which the source is discarded on expiry. The
    /// duration is rounded to the nearest second.
    PN_CPP_EXTERN duration timeout() const;

    /// Get the distribution mode.
    PN_CPP_EXTERN enum distribution_mode distribution_mode() const;

    /// Get the durability flag.
    PN_CPP_EXTERN enum durability_mode durability_mode();

    /// Get the source or target node's address.
    PN_CPP_EXTERN std::string address() const;

    /// True if the remote node is created dynamically.
    PN_CPP_EXTERN bool dynamic() const;

    /// Obtain a reference to the AMQP dynamic node properties for the
    /// terminus.  See also lifetime_policy.
    PN_CPP_EXTERN const value& node_properties() const;

    /// Obtain a reference to the AMQP filter set for the terminus.
    /// See also selector.
    PN_CPP_EXTERN const value& filter() const;

    /// @cond INTERNAL
  private:
    void address(const std::string &);
    void expiry_policy(enum expiry_policy);
    void timeout(duration);
    void distribution_mode(enum distribution_mode);
    void durability_mode(enum durability_mode);
    void dynamic(bool);
    value& node_properties();
    value& filter();

    pn_terminus_t* object_;
    value properties_, filter_;
    pn_link_t* parent_;

    friend class link;
    friend class noderef;
    friend class proton::source;
    friend class proton::target;
    friend class proton::receiver_options;
    friend class proton::source_options;
    friend class proton::target_options;
    /// @endcond
};

}}

#endif // PROTON_CPP_TERMINUS_H
