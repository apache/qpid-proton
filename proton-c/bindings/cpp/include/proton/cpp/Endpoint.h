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
#include "proton/cpp/ImportExport.h"

namespace proton {
namespace reactor {

class Handler;
class Connection;
class Transport;

class Endpoint
{
  public:
    // TODO: getCondition, getRemoteCondition, updateCondition, get/setHandler
    virtual PROTON_CPP_EXTERN Connection &getConnection() = 0;
    Transport PROTON_CPP_EXTERN &getTransport();
  protected:
    Endpoint();
    ~Endpoint();
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_ENDPOINT_H*/
