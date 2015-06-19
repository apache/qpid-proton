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
#include "proton/event.hpp"
#include "proton/link.hpp"

namespace proton {

class handler;
class container;
class connection;
class container;

class proton_event : public event
{
  public:
    virtual PN_CPP_EXTERN void dispatch(handler &h);
    virtual PN_CPP_EXTERN class container &container();
    virtual PN_CPP_EXTERN class connection &connection();
    virtual PN_CPP_EXTERN class sender sender();
    virtual PN_CPP_EXTERN class receiver receiver();
    virtual PN_CPP_EXTERN class link link();
    PN_CPP_EXTERN int type();
    PN_CPP_EXTERN pn_event_t* pn_event();
  protected:
    PN_CPP_EXTERN proton_event(pn_event_t *ce, pn_event_type_t t, class container &c);
  private:
    pn_event_t *pn_event_;
    int type_;
    class container &container_;
};

}

#endif  /*!PROTON_CPP_PROTONEVENT_H*/
