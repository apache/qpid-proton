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

#include "proton/export.hpp"
#include "proton/object.hpp"

#include <vector>

struct pn_handler_t;

namespace proton {

class event;
class proton_event;

/// Handler base class, subclass and over-ride event handling member functions.
/// @see proton::proton_event for meaning of events.
class proton_handler
{
  public:
    proton_handler();
    virtual ~proton_handler();

    ///@name Over-ride these member functions to handle events
    ///@{
    virtual void on_reactor_init(proton_event &e);
    virtual void on_reactor_quiesced(proton_event &e);
    virtual void on_reactor_final(proton_event &e);
    virtual void on_timer_task(proton_event &e);
    virtual void on_connection_init(proton_event &e);
    virtual void on_connection_bound(proton_event &e);
    virtual void on_connection_unbound(proton_event &e);
    virtual void on_connection_local_open(proton_event &e);
    virtual void on_connection_local_close(proton_event &e);
    virtual void on_connection_remote_open(proton_event &e);
    virtual void on_connection_remote_close(proton_event &e);
    virtual void on_connection_final(proton_event &e);
    virtual void on_session_init(proton_event &e);
    virtual void on_session_local_open(proton_event &e);
    virtual void on_session_local_close(proton_event &e);
    virtual void on_session_remote_open(proton_event &e);
    virtual void on_session_remote_close(proton_event &e);
    virtual void on_session_final(proton_event &e);
    virtual void on_link_init(proton_event &e);
    virtual void on_link_local_open(proton_event &e);
    virtual void on_link_local_close(proton_event &e);
    virtual void on_link_local_detach(proton_event &e);
    virtual void on_link_remote_open(proton_event &e);
    virtual void on_link_remote_close(proton_event &e);
    virtual void on_link_remote_detach(proton_event &e);
    virtual void on_link_flow(proton_event &e);
    virtual void on_link_final(proton_event &e);
    virtual void on_delivery(proton_event &e);
    virtual void on_transport(proton_event &e);
    virtual void on_transport_error(proton_event &e);
    virtual void on_transport_head_closed(proton_event &e);
    virtual void on_transport_tail_closed(proton_event &e);
    virtual void on_transport_closed(proton_event &e);
    virtual void on_selectable_init(proton_event &e);
    virtual void on_selectable_updated(proton_event &e);
    virtual void on_selectable_readable(proton_event &e);
    virtual void on_selectable_writable(proton_event &e);
    virtual void on_selectable_expired(proton_event &e);
    virtual void on_selectable_error(proton_event &e);
    virtual void on_selectable_final(proton_event &e);
    virtual void on_unhandled(proton_event &e);
    ///@}
};

}

#endif  /*!PROTON_CPP_PROTONHANDLER_H*/
