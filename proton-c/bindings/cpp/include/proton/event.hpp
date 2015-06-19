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
#include "proton/export.hpp"
#include "proton/link.hpp"
#include "proton/connection.hpp"
#include "proton/message.hpp"
#include <vector>


namespace proton {

class handler;
class container;
class connection;

class event
{
  public:
    virtual PN_CPP_EXTERN void dispatch(handler &h) = 0;
    virtual PN_CPP_EXTERN class container &container();
    virtual PN_CPP_EXTERN class connection &connection();
    virtual PN_CPP_EXTERN class sender sender();
    virtual PN_CPP_EXTERN class receiver receiver();
    virtual PN_CPP_EXTERN class link link();
    virtual PN_CPP_EXTERN class message message();
    virtual PN_CPP_EXTERN void message(class message &);
    virtual PN_CPP_EXTERN ~event();
  protected:
    PN_CPP_EXTERN event();
  private:
    event(const event&);
    event& operator=(const event&);
};

}

#endif  /*!PROTON_CPP_EVENT_H*/
