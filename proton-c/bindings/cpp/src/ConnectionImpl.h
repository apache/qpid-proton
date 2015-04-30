#ifndef PROTON_CPP_CONNECTIONIMPL_H
#define PROTON_CPP_CONNECTIONIMPL_H

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

class ConnectionImpl : public Endpoint
{
  public:
    PROTON_CPP_EXTERN ConnectionImpl(Container &c);
    PROTON_CPP_EXTERN ~ConnectionImpl();
    PROTON_CPP_EXTERN Transport &getTransport();
    PROTON_CPP_EXTERN Handler *getOverride();
    PROTON_CPP_EXTERN void setOverride(Handler *h);
    PROTON_CPP_EXTERN void open();
    PROTON_CPP_EXTERN void close();
    PROTON_CPP_EXTERN pn_connection_t *getPnConnection();
    PROTON_CPP_EXTERN Container &getContainer();
    PROTON_CPP_EXTERN std::string getHostname();
    virtual PROTON_CPP_EXTERN Connection &getConnection();
    static Connection &getReactorReference(pn_connection_t *);
    static ConnectionImpl *getImpl(const Connection &c) { return c.impl; }
    void reactorDetach();
    static void incref(ConnectionImpl *);
    static void decref(ConnectionImpl *);
  private:
    friend class Connector;
    friend class ContainerImpl;
    Container container;
    int refCount;
    Handler *override;
    Transport *transport;
    pn_session_t *defaultSession;  // Temporary, for SessionPerConnection style policy.
    pn_connection_t *pnConnection;
    Connection reactorReference;   // Keep-alive reference, until PN_CONNECTION_FINAL.
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_CONNECTIONIMPL_H*/
