#ifndef PROTON_CPP_ENDPOINT_H
#define PROTON_CPP_ENDPOINT_H

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
#include "proton/connection.h"

namespace proton {
namespace reactor {

class Handler;
class Connection;
class Transport;

class Endpoint
{
  public:
    enum {
        LOCAL_UNINIT = PN_LOCAL_UNINIT,
        REMOTE_UNINIT = PN_REMOTE_UNINIT,
        LOCAL_ACTIVE = PN_LOCAL_ACTIVE,
        REMOTE_ACTIVE = PN_REMOTE_ACTIVE,
        LOCAL_CLOSED = PN_LOCAL_CLOSED,
        REMOTE_CLOSED  = PN_REMOTE_CLOSED
    };
    typedef int State;

    // TODO: getCondition, getRemoteCondition, updateCondition, get/setHandler
    virtual PN_CPP_EXTERN Connection &getConnection() = 0;
    Transport PN_CPP_EXTERN &getTransport();
  protected:
    Endpoint();
    ~Endpoint();
};


}}

#endif  /*!PROTON_CPP_ENDPOINT_H*/
