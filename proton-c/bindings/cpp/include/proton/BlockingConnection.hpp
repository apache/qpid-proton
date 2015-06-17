#ifndef PROTON_CPP_BLOCKINGCONNECTION_H
#define PROTON_CPP_BLOCKINGCONNECTION_H

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
#include "proton/Handle.hpp"
#include "proton/Endpoint.hpp"
#include "proton/Container.hpp"
#include "proton/Duration.hpp"
#include "proton/MessagingHandler.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {
namespace reactor {

class Container;
class BlockingConnectionImpl;
class SslDomain;
class BlockingSender;
class WaitCondition;

class BlockingConnection : public Handle<BlockingConnectionImpl>
{
  public:
    PN_CPP_EXTERN BlockingConnection();
    PN_CPP_EXTERN BlockingConnection(const BlockingConnection& c);
    PN_CPP_EXTERN BlockingConnection& operator=(const BlockingConnection& c);
    PN_CPP_EXTERN ~BlockingConnection();

    PN_CPP_EXTERN BlockingConnection(std::string &url, Duration = Duration::FOREVER,
                                         SslDomain *ssld=0, Container *c=0);
    PN_CPP_EXTERN void close();

    PN_CPP_EXTERN BlockingSender createSender(std::string &address, Handler *h=0);
    PN_CPP_EXTERN void wait(WaitCondition &condition);
    PN_CPP_EXTERN void wait(WaitCondition &condition, std::string &msg, Duration timeout=Duration::FOREVER);
    PN_CPP_EXTERN Duration getTimeout();
  private:
    friend class PrivateImplRef<BlockingConnection>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_BLOCKINGCONNECTION_H*/
