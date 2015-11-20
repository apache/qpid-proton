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

#include "proton/messaging_handler.hpp"

#include "proton/event.h"
#include "proton/reactor.h"

///@cond INTERNAL

namespace proton {

// Combine's Python's: endpoint_state_handler, incoming_message_handler, outgoing_message_handler

class messaging_adapter : public messaging_handler
{
  public:
    PN_CPP_EXTERN messaging_adapter(messaging_handler &delegate);
    PN_CPP_EXTERN virtual ~messaging_adapter();
    PN_CPP_EXTERN virtual void on_reactor_init(event &e);
    PN_CPP_EXTERN virtual void on_link_flow(event &e);
    PN_CPP_EXTERN virtual void on_delivery(event &e);
    PN_CPP_EXTERN virtual void on_unhandled(event &e);
    PN_CPP_EXTERN virtual void on_connection_closed(event &e);
    PN_CPP_EXTERN virtual void on_connection_closing(event &e);
    PN_CPP_EXTERN virtual void on_connection_error(event &e);
    PN_CPP_EXTERN virtual void on_connection_local_open(event &e);
    PN_CPP_EXTERN virtual void on_connection_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_connection_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_connection_opened(event &e);
    PN_CPP_EXTERN virtual void on_connection_opening(event &e);
    PN_CPP_EXTERN virtual void on_session_closed(event &e);
    PN_CPP_EXTERN virtual void on_session_closing(event &e);
    PN_CPP_EXTERN virtual void on_session_error(event &e);
    PN_CPP_EXTERN virtual void on_session_local_open(event &e);
    PN_CPP_EXTERN virtual void on_session_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_session_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_session_opened(event &e);
    PN_CPP_EXTERN virtual void on_session_opening(event &e);
    PN_CPP_EXTERN virtual void on_link_closed(event &e);
    PN_CPP_EXTERN virtual void on_link_closing(event &e);
    PN_CPP_EXTERN virtual void on_link_error(event &e);
    PN_CPP_EXTERN virtual void on_link_local_open(event &e);
    PN_CPP_EXTERN virtual void on_link_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_link_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_link_opened(event &e);
    PN_CPP_EXTERN virtual void on_link_opening(event &e);
    PN_CPP_EXTERN virtual void on_transport_tail_closed(event &e);
  private:
    messaging_handler &delegate_;  // The handler for generated messaging_event's
};

}
///@endcond INTERNAL
#endif  /*!PROTON_CPP_MESSAGING_ADAPTER_H*/
