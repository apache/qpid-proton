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
#include "proton/Handle.hpp"
#include "proton/Endpoint.hpp"
#include "proton/Container.hpp"
#include "proton/Duration.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/BlockingConnection.hpp"
#include "proton/types.h"
#include <string>

namespace proton {
namespace reactor {

class BlockingConnection;

class BlockingLink
{
  public:
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN ~BlockingLink();
  protected:
    PN_CPP_EXTERN BlockingLink(BlockingConnection *c, pn_link_t *l);
    PN_CPP_EXTERN void waitForClosed(Duration timeout=Duration::SECOND);
  private:
    BlockingConnection connection;
    Link link;
    PN_CPP_EXTERN void checkClosed();
    friend class BlockingConnection;
    friend class BlockingSender;
    friend class BlockingReceiver;
};

}}

#endif  /*!PROTON_CPP_BLOCKINGLINK_H*/
