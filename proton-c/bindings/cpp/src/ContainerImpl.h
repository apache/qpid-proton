#ifndef PROTON_CPP_CONTAINERIMPL_H
#define PROTON_CPP_CONTAINERIMPL_H

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
#include "proton/cpp/MessagingHandler.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Link.h"

#include "proton/reactor.h"

#include <string>
namespace proton {
namespace reactor {

class DispatchHelper;
class Connection;
class Connector;
class Acceptor;

class ContainerImpl
{
  public:
    PROTON_CPP_EXTERN ContainerImpl(MessagingHandler &mhandler);
    PROTON_CPP_EXTERN ~ContainerImpl();
    PROTON_CPP_EXTERN Connection connect(std::string &host);
    PROTON_CPP_EXTERN void run();
    PROTON_CPP_EXTERN pn_reactor_t *getReactor();
    PROTON_CPP_EXTERN pn_handler_t *getGlobalHandler();
    PROTON_CPP_EXTERN Sender createSender(Connection &connection, std::string &addr);
    PROTON_CPP_EXTERN Sender createSender(std::string &url);
    PROTON_CPP_EXTERN Receiver createReceiver(Connection &connection, std::string &addr);
    PROTON_CPP_EXTERN Acceptor listen(const std::string &url);
    PROTON_CPP_EXTERN std::string getContainerId();
    static void incref(ContainerImpl *);
    static void decref(ContainerImpl *);
  private:
    void dispatch(pn_event_t *event, pn_event_type_t type);
    Acceptor acceptor(const std::string &host, const std::string &port);
    pn_reactor_t *reactor;
    pn_handler_t *globalHandler;
    MessagingHandler &messagingHandler;
    std::string containerId;
    int refCount;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_CONTAINERIMPL_H*/
