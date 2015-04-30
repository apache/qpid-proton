#ifndef PROTON_CPP_SESSION_H
#define PROTON_CPP_SESSION_H

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
#include "proton/cpp/Link.h"

#include "proton/types.h"
#include "proton/link.h"
#include <string>

struct pn_connection_t;

namespace proton {
namespace reactor {

class Container;
class Handler;
class Transport;

class Session : public Endpoint
{
  public:
    PROTON_CPP_EXTERN Session(pn_session_t *s);
    PROTON_CPP_EXTERN ~Session();
    PROTON_CPP_EXTERN void open();
    PROTON_CPP_EXTERN void close();
    PROTON_CPP_EXTERN pn_session_t *getPnSession();
    virtual PROTON_CPP_EXTERN Connection &getConnection();
    Receiver createReceiver(std::string name);
    Sender createSender(std::string name);
  private:
    pn_session_t *pnSession;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_SESSION_H*/
