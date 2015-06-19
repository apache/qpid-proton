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
#include "proton/export.hpp"
#include "proton/handle.hpp"
#include "proton/endpoint.hpp"
#include "proton/container.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

class handler;
class transport;
class connection_impl;

class connection : public endpoint, public handle<connection_impl>
{
  public:
    PN_CPP_EXTERN connection();
    PN_CPP_EXTERN connection(connection_impl *);
    PN_CPP_EXTERN connection(const connection& c);
    PN_CPP_EXTERN connection(class container &c, handler *h = 0);
    PN_CPP_EXTERN ~connection();

    PN_CPP_EXTERN connection& operator=(const connection& c);

    PN_CPP_EXTERN class transport& transport();
    PN_CPP_EXTERN handler *override();
    PN_CPP_EXTERN void override(handler *h);
    PN_CPP_EXTERN void open();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN pn_connection_t *pn_connection();
    PN_CPP_EXTERN class container &container();
    PN_CPP_EXTERN std::string hostname();
    PN_CPP_EXTERN link link_head(endpoint::State mask);
  private:
   friend class private_impl_ref<connection>;
   friend class Connector;
   friend class connection_impl;
};

}

#endif  /*!PROTON_CPP_CONNECTION_H*/
