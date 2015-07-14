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
#include "proton/link.hpp"

#include "proton/link.h"
#include <string>

namespace proton {

class link;

/** A terminus represents one end of a link.
 * The source terminus is where originate, the target terminus is where they go.
 */
class terminus : public proton_handle<pn_terminus_t>
{
  public:
    /// Type of terminus
    enum type_t {
        TYPE_UNSPECIFIED = PN_UNSPECIFIED,
        SOURCE = PN_SOURCE,
        TARGET = PN_TARGET,
        COORDINATOR = PN_COORDINATOR ///< Transaction co-ordinator
    };

    /// Expiry policy
    enum expiry_policy_t {
        NONDURABLE = PN_NONDURABLE,
        CONFIGURATION = PN_CONFIGURATION,
        DELIVERIES = PN_DELIVERIES
    };

    /// Distribution mode
    enum distribution_mode_t {
        MODE_UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED,
        COPY = PN_DIST_MODE_COPY,
        MOVE = PN_DIST_MODE_MOVE
    };


    PN_CPP_EXTERN terminus();
    PN_CPP_EXTERN ~terminus();
    PN_CPP_EXTERN terminus(const terminus&);
    PN_CPP_EXTERN terminus& operator=(const terminus&);
    PN_CPP_EXTERN pn_terminus_t *pn_terminus();
    PN_CPP_EXTERN type_t type();
    PN_CPP_EXTERN void type(type_t);
    PN_CPP_EXTERN expiry_policy_t expiry_policy();
    PN_CPP_EXTERN void expiry_policy(expiry_policy_t);
    PN_CPP_EXTERN distribution_mode_t distribution_mode();
    PN_CPP_EXTERN void distribution_mode(distribution_mode_t);
    PN_CPP_EXTERN std::string address();
    PN_CPP_EXTERN void address(std::string &);
    PN_CPP_EXTERN bool is_dynamic();
    PN_CPP_EXTERN void dynamic(bool);

  private:
    link *link_;
    PN_CPP_EXTERN terminus(pn_terminus_t *, link *);
  friend class link;
  friend class proton_impl_ref<terminus>;
};


}

#endif  /*!PROTON_CPP_TERMINUS_H*/
