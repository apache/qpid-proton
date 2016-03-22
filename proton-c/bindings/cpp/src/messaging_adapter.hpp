#ifndef PROTON_CPP_MESSAGING_ADAPTER_H
#define PROTON_CPP_MESSAGING_ADAPTER_H

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

#include "proton/handler.hpp"

#include "proton_handler.hpp"

#include "proton/event.h"
#include "proton/reactor.h"

///@cond INTERNAL

namespace proton {

/// Convert the low level proton-c events to the higher level proton::handler calls
class messaging_adapter : public proton_handler
{
  public:
    messaging_adapter(handler &delegate);
    virtual ~messaging_adapter();

    void on_reactor_init(proton_event &e);
    void on_link_flow(proton_event &e);
    void on_delivery(proton_event &e);
    void on_connection_remote_open(proton_event &e);
    void on_connection_remote_close(proton_event &e);
    void on_session_remote_open(proton_event &e);
    void on_session_remote_close(proton_event &e);
    void on_link_local_open(proton_event &e);
    void on_link_remote_open(proton_event &e);
    void on_link_remote_close(proton_event &e);
    void on_transport_tail_closed(proton_event &e);
    void on_timer_task(proton_event &e);

  private:
    handler &delegate_;  // The handler for generated messaging_event's
};

}
///@endcond INTERNAL
#endif  /*!PROTON_CPP_MESSAGING_ADAPTER_H*/
