#ifndef PROTON_CPP_EVENT_H
#define PROTON_CPP_EVENT_H

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
#include "proton/Link.hpp"
#include "proton/Connection.hpp"
#include "proton/Message.hpp"
#include <vector>


namespace proton {
namespace reactor {

class Handler;
class Container;
class Connection;

class Event
{
  public:
    virtual PN_CPP_EXTERN void dispatch(Handler &h) = 0;
    virtual PN_CPP_EXTERN Container &getContainer();
    virtual PN_CPP_EXTERN Connection &getConnection();
    virtual PN_CPP_EXTERN Sender getSender();
    virtual PN_CPP_EXTERN Receiver getReceiver();
    virtual PN_CPP_EXTERN Link getLink();
    virtual PN_CPP_EXTERN Message getMessage();
    virtual PN_CPP_EXTERN void setMessage(Message &);
    virtual PN_CPP_EXTERN ~Event();
  protected:
    PN_CPP_EXTERN PN_CPP_EXTERN Event();
  private:
    PN_CPP_EXTERN Event(const Event&);
    PN_CPP_EXTERN Event& operator=(const Event&);
};

}}

#endif  /*!PROTON_CPP_EVENT_H*/
