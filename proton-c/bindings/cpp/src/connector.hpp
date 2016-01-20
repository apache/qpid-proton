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

#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/url.hpp"
#include "proton/event.h"
#include "proton/reactor.h"

#include "proton_handler.hpp"

#include <string>


namespace proton {

class reconnect_timer;

class connector : public proton_handler
{
  public:
    connector(connection &c, const connection_options &opts);
    ~connector();
    void address(const url&);
    const url &address() const { return address_; }
    void connect();
    void apply_options();
    void reconnect_timer(const class reconnect_timer &);
    bool transport_configured();
    virtual void on_connection_local_open(proton_event &e);
    virtual void on_connection_remote_open(proton_event &e);
    virtual void on_connection_init(proton_event &e);
    virtual void on_transport_closed(proton_event &e);
    virtual void on_transport_tail_closed(proton_event &e);
    virtual void on_timer_task(proton_event &e);

  private:
    connection connection_;
    url address_;
    connection_options options_;
    class reconnect_timer *reconnect_timer_;
    bool transport_configured_;
};


}

#endif  /*!PROTON_CPP_CONNECTOR_HANDLER_H*/
