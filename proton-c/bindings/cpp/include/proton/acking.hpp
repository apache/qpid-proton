#ifndef PROTON_CPP_ACKING_H
#define PROTON_CPP_ACKING_H

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
#include "proton/delivery.hpp"

namespace proton {


/** acking provides simple functions to acknowledge, or settle, a delivery */
class acking
{
  public:
    /** accept a delivery */
    PN_CPP_EXTERN virtual void accept(delivery &d);
    /** reject a delivery */
    PN_CPP_EXTERN virtual void reject(delivery &d);
    /** release a delivery. Mark it delivery::MODIFIED if it has already been delivered,
     * delivery::RELEASED otherwise.
     */
    PN_CPP_EXTERN virtual void release(delivery &d, bool delivered=true);
    /** settle a delivery with the given delivery::state */
    PN_CPP_EXTERN virtual void settle(delivery &d, delivery::state s = delivery::REJECTED);
};


}

#endif  /*!PROTON_CPP_ACKING_H*/


