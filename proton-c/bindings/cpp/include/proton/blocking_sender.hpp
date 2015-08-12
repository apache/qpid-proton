#ifndef PROTON_CPP_BLOCKINGSENDER_H
#define PROTON_CPP_BLOCKINGSENDER_H

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
#include "proton/handle.hpp"
#include "proton/endpoint.hpp"
#include "proton/container.hpp"
#include "proton/duration.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/blocking_link.hpp"
#include "proton/types.h"
#include "proton/delivery.h"
#include <string>

namespace proton {

class blocking_connection;
class blocking_link;

// TODO documentation
class blocking_sender : public blocking_link
{
  public:
    PN_CPP_EXTERN delivery send(message &msg);
    PN_CPP_EXTERN delivery send(message &msg, duration timeout);
  private:
    PN_CPP_EXTERN blocking_sender(blocking_connection &c, sender l);
    friend class blocking_connection;
};

}

#endif  /*!PROTON_CPP_BLOCKINGSENDER_H*/
