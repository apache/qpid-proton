#ifndef PROTON_CPP_PROTONEVENT_H
#define PROTON_CPP_PROTONEVENT_H

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
#include "proton/cpp/Event.h"
#include "proton/cpp/Link.h"

namespace proton {
namespace cpp {
namespace reactor {

class Handler;
class Container;
class Connection;

class ProtonEvent : public Event
{
  public:
    virtual PROTON_CPP_EXTERN void dispatch(Handler &h);
    virtual PROTON_CPP_EXTERN Container &getContainer();
    virtual PROTON_CPP_EXTERN Connection &getConnection();
    virtual PROTON_CPP_EXTERN Sender getSender();
    virtual PROTON_CPP_EXTERN Receiver getReceiver();
    virtual PROTON_CPP_EXTERN Link getLink();
    PROTON_CPP_EXTERN int getType();
    PROTON_CPP_EXTERN pn_event_t* getPnEvent();
  protected:
    PROTON_CPP_EXTERN ProtonEvent(pn_event_t *ce, pn_event_type_t t, Container &c);
  private:
    int type;
    pn_event_t *pnEvent;
    Container &container;
};


}}} // namespace proton::cpp::reactor

#endif  /*!PROTON_CPP_PROTONEVENT_H*/
