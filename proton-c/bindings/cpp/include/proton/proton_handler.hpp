#ifndef PROTON_CPP_PROTONHANDLER_H
#define PROTON_CPP_PROTONHANDLER_H

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

namespace proton {

class event;
class proton_event;

/// Handler base class, subclass and over-ride event handling member functions.
/// @see proton::proton_event for meaning of events.
class proton_handler : virtual public handler
{
  public:
    PN_CPP_EXTERN proton_handler();

    ///@name Over-ride these member functions to handle events
    ///@{
    PN_CPP_EXTERN virtual void on_reactor_init(event &e);
    PN_CPP_EXTERN virtual void on_reactor_quiesced(event &e);
    PN_CPP_EXTERN virtual void on_reactor_final(event &e);
    PN_CPP_EXTERN virtual void on_timer_task(event &e);
    PN_CPP_EXTERN virtual void on_connection_init(event &e);
    PN_CPP_EXTERN virtual void on_connection_bound(event &e);
    PN_CPP_EXTERN virtual void on_connection_unbound(event &e);
    PN_CPP_EXTERN virtual void on_connection_local_open(event &e);
    PN_CPP_EXTERN virtual void on_connection_local_close(event &e);
    PN_CPP_EXTERN virtual void on_connection_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_connection_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_connection_final(event &e);
    PN_CPP_EXTERN virtual void on_session_init(event &e);
    PN_CPP_EXTERN virtual void on_session_local_open(event &e);
    PN_CPP_EXTERN virtual void on_session_local_close(event &e);
    PN_CPP_EXTERN virtual void on_session_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_session_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_session_final(event &e);
    PN_CPP_EXTERN virtual void on_link_init(event &e);
    PN_CPP_EXTERN virtual void on_link_local_open(event &e);
    PN_CPP_EXTERN virtual void on_link_local_close(event &e);
    PN_CPP_EXTERN virtual void on_link_local_detach(event &e);
    PN_CPP_EXTERN virtual void on_link_remote_open(event &e);
    PN_CPP_EXTERN virtual void on_link_remote_close(event &e);
    PN_CPP_EXTERN virtual void on_link_remote_detach(event &e);
    PN_CPP_EXTERN virtual void on_link_flow(event &e);
    PN_CPP_EXTERN virtual void on_link_final(event &e);
    PN_CPP_EXTERN virtual void on_delivery(event &e);
    PN_CPP_EXTERN virtual void on_transport(event &e);
    PN_CPP_EXTERN virtual void on_transport_error(event &e);
    PN_CPP_EXTERN virtual void on_transport_head_closed(event &e);
    PN_CPP_EXTERN virtual void on_transport_tail_closed(event &e);
    PN_CPP_EXTERN virtual void on_transport_closed(event &e);
    PN_CPP_EXTERN virtual void on_selectable_init(event &e);
    PN_CPP_EXTERN virtual void on_selectable_updated(event &e);
    PN_CPP_EXTERN virtual void on_selectable_readable(event &e);
    PN_CPP_EXTERN virtual void on_selectable_writable(event &e);
    PN_CPP_EXTERN virtual void on_selectable_expired(event &e);
    PN_CPP_EXTERN virtual void on_selectable_error(event &e);
    PN_CPP_EXTERN virtual void on_selectable_final(event &e);
    PN_CPP_EXTERN virtual void on_unhandled(event &e);
    ///@}
};

}

#endif  /*!PROTON_CPP_PROTONHANDLER_H*/
