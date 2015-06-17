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
#include "proton/ImportExport.hpp"
#include "proton/ProtonHandle.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {
namespace reactor {

class Delivery : public ProtonHandle<pn_delivery_t>
{
  public:

    enum state {
        NONE = 0,
        RECEIVED = PN_RECEIVED,
        ACCEPTED = PN_ACCEPTED,
        REJECTED = PN_REJECTED,
        RELEASED = PN_RELEASED,
        MODIFIED = PN_MODIFIED
    };  // AMQP spec 3.4 Delivery State

    PN_CPP_EXTERN Delivery(pn_delivery_t *d);
    PN_CPP_EXTERN Delivery();
    PN_CPP_EXTERN ~Delivery();
    PN_CPP_EXTERN Delivery(const Delivery&);
    PN_CPP_EXTERN Delivery& operator=(const Delivery&);
    PN_CPP_EXTERN bool settled();
    PN_CPP_EXTERN void settle();
    PN_CPP_EXTERN pn_delivery_t *getPnDelivery();
  private:
    friend class ProtonImplRef<Delivery>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_DELIVERY_H*/
