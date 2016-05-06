#ifndef PROTON_CPP_SOURCE_H
#define PROTON_CPP_SOURCE_H

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
#include "proton/terminus.hpp"
#include <proton/map.hpp>

#include <string>

namespace proton {

class sender;
class receiver;

///
/// The source node is where messages originate.
///
/// @see proton::sender proton::receiver proton::target
class source : public terminus {
  public:
    /// A map of AMQP symbol keys and filter specifiers.
    typedef std::map<symbol, value> filter_map;

    source() : terminus() {}

    enum distribution_mode {
      UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED,
      COPY = PN_DIST_MODE_COPY,
      MOVE = PN_DIST_MODE_MOVE
    };

    using terminus::durability_mode;
    using terminus::expiry_policy;

    /// The address of the source.
    PN_CPP_EXTERN std::string address() const;

    /// Get the distribution mode.
    PN_CPP_EXTERN enum distribution_mode distribution_mode() const;

    /// Obtain the set of message filters.
    PN_CPP_EXTERN filter_map filters() const;
    /// @cond INTERNAL
  private:
    source(pn_terminus_t* t);
    source(const sender&);
    source(const receiver&);
  friend class proton::internal::factory<source>;
  friend class sender;
  friend class receiver;
    /// @endcond
};

}

#endif // PROTON_CPP_SOURCE_H
