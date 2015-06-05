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
#include "proton/cpp/ImportExport.h"
#include "proton/cpp/Handle.h"
#include "proton/cpp/Endpoint.h"
#include "proton/cpp/Container.h"
#include "proton/cpp/Duration.h"
#include "proton/cpp/MessagingHandler.h"
#include "proton/cpp/BlockingLink.h"
#include "proton/types.h"
#include "proton/delivery.h"
#include <string>

namespace proton {
namespace reactor {

class BlockingConnection;
class BlockingLink;

class BlockingSender : public BlockingLink
{
  public:
    PN_CPP_EXTERN Delivery send(Message &msg);
    PN_CPP_EXTERN Delivery send(Message &msg, Duration timeout);
  private:
    PN_CPP_EXTERN BlockingSender(BlockingConnection &c, Sender &l);
    friend class BlockingConnection;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_BLOCKINGSENDER_H*/
