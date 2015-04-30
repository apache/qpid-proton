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
#include "proton/cpp/ImportExport.h"
#include "proton/cpp/Link.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Message.h"
#include <vector>


namespace proton {
namespace reactor {

class Handler;
class Container;
class Connection;

class Event
{
  public:
    virtual PROTON_CPP_EXTERN void dispatch(Handler &h) = 0;
    virtual PROTON_CPP_EXTERN Container &getContainer();
    virtual PROTON_CPP_EXTERN Connection &getConnection();
    virtual PROTON_CPP_EXTERN Sender getSender();
    virtual PROTON_CPP_EXTERN Receiver getReceiver();
    virtual PROTON_CPP_EXTERN Link getLink();
    virtual PROTON_CPP_EXTERN Message getMessage();
    virtual PROTON_CPP_EXTERN void setMessage(Message &);
    virtual PROTON_CPP_EXTERN ~Event();
  protected:
    PROTON_CPP_EXTERN PROTON_CPP_EXTERN Event();
  private:
    PROTON_CPP_EXTERN Event(const Event&);
    PROTON_CPP_EXTERN Event& operator=(const Event&);
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_EVENT_H*/
