#ifndef PROTON_CPP_CONNECTOR_HANDLER_H
#define PROTON_CPP_CONNECTOR_HANDLER_H

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

#include "proton/proton_handler.hpp"
#include "proton/event.h"
#include "proton/reactor.h"
#include "proton/url.h"
#include <string>


namespace proton {

class event;
class connection;
class transport;

class connector : public proton_handler
{
  public:
    connector(connection &c);
    ~connector();
    void address(const url&);
    void connect();
    virtual void on_connection_local_open(event &e);
    virtual void on_connection_remote_open(event &e);
    virtual void on_connection_init(event &e);
    virtual void on_transport_closed(event &e);

  private:
    connection connection_;
    url address_;
};


}

#endif  /*!PROTON_CPP_CONNECTOR_HANDLER_H*/
