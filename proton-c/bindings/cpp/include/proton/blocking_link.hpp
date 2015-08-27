#ifndef PROTON_CPP_BLOCKINGLINK_H
#define PROTON_CPP_BLOCKINGLINK_H

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
#include "proton/endpoint.hpp"
#include "proton/container.hpp"
#include "proton/duration.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/types.h"
#include <string>

namespace proton {

class blocking_connection;

// TODO documentation
class blocking_link
{
  public:
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN ~blocking_link();
  protected:
    PN_CPP_EXTERN blocking_link(blocking_connection *c, pn_link_t *l);
    PN_CPP_EXTERN void wait_for_closed(duration timeout=duration::SECOND);
  private:
    blocking_connection connection_;
    link link_;
    PN_CPP_EXTERN void check_closed();
    friend class blocking_connection;
    friend class blocking_sender;
    friend class blocking_receiver;
};

}

#endif  /*!PROTON_CPP_BLOCKINGLINK_H*/
