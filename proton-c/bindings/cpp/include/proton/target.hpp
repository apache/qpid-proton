#ifndef PROTON_CPP_TARGET_H
#define PROTON_CPP_TARGET_H

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

#include <string>

namespace proton {

class sender;
class receiver;

///
/// The target is the destination node of a sent or received message.
///
/// @see proton::sender proton::receiver proton::target
class target : public internal::terminus {
  public:
    target() : internal::terminus() {}
    PN_CPP_EXTERN std::string address() const;
    /// @cond INTERNAL
  private:
    target(pn_terminus_t* t);
    target(const sender&);
    target(const receiver&);
  friend class sender;
  friend class receiver;
  friend class sender_options;
  friend class receiver_options;
    /// @endcond

};

}

#endif // PROTON_CPP_TARGET_H
