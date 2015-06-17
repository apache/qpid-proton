#ifndef PROTON_CPP_CONTAINER_H
#define PROTON_CPP_CONTAINER_H

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
#include "proton/Acceptor.hpp"
#include "proton/Duration.hpp"
#include <proton/reactor.h>
#include <string>

namespace proton {
namespace reactor {

class DispatchHelper;
class Connection;
class Connector;
class Acceptor;
class ContainerImpl;
class MessagingHandler;
class Sender;
class Receiver;
class Link;
 class Handler;

class Container : public Handle<ContainerImpl>
{
  public:
    PN_CPP_EXTERN Container(ContainerImpl *);
    PN_CPP_EXTERN Container(const Container& c);
    PN_CPP_EXTERN Container& operator=(const Container& c);
    PN_CPP_EXTERN ~Container();

    PN_CPP_EXTERN Container();
    PN_CPP_EXTERN Container(MessagingHandler &mhandler);
    PN_CPP_EXTERN Connection connect(std::string &host, Handler *h=0);
    PN_CPP_EXTERN void run();
    PN_CPP_EXTERN void start();
    PN_CPP_EXTERN bool process();
    PN_CPP_EXTERN void stop();
    PN_CPP_EXTERN void wakeup();
    PN_CPP_EXTERN bool isQuiesced();
    PN_CPP_EXTERN pn_reactor_t *getReactor();
    PN_CPP_EXTERN Sender createSender(Connection &connection, std::string &addr, Handler *h=0);
    PN_CPP_EXTERN Sender createSender(std::string &url);
    PN_CPP_EXTERN Receiver createReceiver(Connection &connection, std::string &addr);
    PN_CPP_EXTERN Receiver createReceiver(const std::string &url);
    PN_CPP_EXTERN Acceptor listen(const std::string &url);
    PN_CPP_EXTERN std::string getContainerId();
    PN_CPP_EXTERN Duration getTimeout();
    PN_CPP_EXTERN void setTimeout(Duration timeout);
  private:
   friend class PrivateImplRef<Container>;
};

}}

#endif  /*!PROTON_CPP_CONTAINER_H*/
