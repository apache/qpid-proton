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
#include "proton/proton_handle.hpp"

#include "proton/delivery.h"
#include "proton/disposition.h"

namespace proton {

class delivery : public proton_handle<pn_delivery_t>
{
  public:

    enum state {
        NONE = 0,
        RECEIVED = PN_RECEIVED,
        ACCEPTED = PN_ACCEPTED,
        REJECTED = PN_REJECTED,
        RELEASED = PN_RELEASED,
        MODIFIED = PN_MODIFIED
    };  // AMQP spec 3.4 delivery State

    PN_CPP_EXTERN delivery(pn_delivery_t *d);
    PN_CPP_EXTERN delivery();
    PN_CPP_EXTERN ~delivery();
    PN_CPP_EXTERN delivery(const delivery&);
    PN_CPP_EXTERN delivery& operator=(const delivery&);
    PN_CPP_EXTERN bool settled();
    PN_CPP_EXTERN void settle();
    PN_CPP_EXTERN pn_delivery_t *pn_delivery();
  private:
    friend class proton_impl_ref<delivery>;
};

}

#endif  /*!PROTON_CPP_DELIVERY_H*/
