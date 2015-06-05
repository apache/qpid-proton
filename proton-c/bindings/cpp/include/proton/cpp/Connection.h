#ifndef PROTON_CPP_CONNECTION_H
#define PROTON_CPP_CONNECTION_H

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
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {
namespace reactor {

class Handler;
class Transport;
class Container;
class ConnectionImpl;

class Connection : public Endpoint, public Handle<ConnectionImpl>
{
  public:
    PN_CPP_EXTERN Connection();
    PN_CPP_EXTERN Connection(ConnectionImpl *);
    PN_CPP_EXTERN Connection(const Connection& c);
    PN_CPP_EXTERN Connection& operator=(const Connection& c);
    PN_CPP_EXTERN ~Connection();

    PN_CPP_EXTERN Connection(Container &c, Handler *h = 0);
    PN_CPP_EXTERN Transport &getTransport();
    PN_CPP_EXTERN Handler *getOverride();
    PN_CPP_EXTERN void setOverride(Handler *h);
    PN_CPP_EXTERN void open();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN pn_connection_t *getPnConnection();
    PN_CPP_EXTERN Container &getContainer();
    PN_CPP_EXTERN std::string getHostname();
    virtual PN_CPP_EXTERN Connection &getConnection();
    PN_CPP_EXTERN Link getLinkHead(Endpoint::State mask);
  private:
   friend class PrivateImplRef<Connection>;
   friend class Connector;
   friend class ConnectionImpl;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_CONNECTION_H*/
