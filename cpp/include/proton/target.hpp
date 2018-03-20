#ifndef PROTON_TARGET_HPP
#define PROTON_TARGET_HPP

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
#include "./terminus.hpp"

#include <string>

/// @file
/// @copybrief proton::target

struct pn_terminus_t;

namespace proton {

namespace internal {
template <class T> class factory;
}

/// A destination for messages.
///
/// @see proton::sender, proton::receiver, proton::target
class target : public terminus {
  public:
    /// Create an empty target.
    target() : terminus() {}

    using terminus::durability_mode;
    using terminus::expiry_policy;

    /// The address of the target.
    PN_CPP_EXTERN std::string address() const;

  private:
    target(pn_terminus_t* t);
    target(const sender&);
    target(const receiver&);

    /// @cond INTERNAL
  friend class internal::factory<target>;
  friend class sender;
  friend class receiver;
    /// @endcond
};

} // proton

#endif // PROTON_TARGET_HPP
