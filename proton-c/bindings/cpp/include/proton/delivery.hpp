#ifndef PROTON_CPP_DELIVERY_H
#define PROTON_CPP_DELIVERY_H

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
#include "proton/wrapper.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

/** delivery status of a message */
class delivery : public wrapper<pn_delivery_t>
{
  public:

    /** Delivery state of a message */
    enum state {
        NONE = 0, ///< Unknown state
        RECEIVED = PN_RECEIVED, ///< Received but not yet settled
        ACCEPTED = PN_ACCEPTED, ///< Settled as accepted
        REJECTED = PN_REJECTED, ///< Settled as rejected
        RELEASED = PN_RELEASED, ///< Settled as released
        MODIFIED = PN_MODIFIED  ///< Settled as modified
    };  // AMQP spec 3.4 delivery State

    PN_CPP_EXTERN delivery(pn_delivery_t * = 0);

    /** Return true if the delivery has been settled. */
    PN_CPP_EXTERN bool settled();

    /** Settle the delivery, informs the remote end. */
    PN_CPP_EXTERN void settle();

    /** Set the local state of the delivery. */
    PN_CPP_EXTERN void update(delivery::state state);
};

}

#endif  /*!PROTON_CPP_DELIVERY_H*/
