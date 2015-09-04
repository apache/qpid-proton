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
#include "proton/facade.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

/** delivery status of a message */
class delivery : public counted_facade<pn_delivery_t, delivery> {
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

    /** Return true if the delivery has been settled. */
    PN_CPP_EXTERN bool settled();

    /** Settle the delivery, informs the remote end. */
    PN_CPP_EXTERN void settle();

    /** Set the local state of the delivery. */
    PN_CPP_EXTERN void update(delivery::state state);

    /** update and settle a delivery with the given delivery::state */
    PN_CPP_EXTERN void settle(delivery::state s);

    /** settle with ACCEPTED state */
    PN_CPP_EXTERN void accept() { settle(ACCEPTED); }

    /** settle with REJECTED state */
    PN_CPP_EXTERN void reject() { settle(REJECTED); }

    /** settle with REJECTED state */
    PN_CPP_EXTERN void release() { settle(RELEASED); }

    /** settle with MODIFIED state */
    PN_CPP_EXTERN void modifiy() { settle(MODIFIED); }
};

}

#endif  /*!PROTON_CPP_DELIVERY_H*/
