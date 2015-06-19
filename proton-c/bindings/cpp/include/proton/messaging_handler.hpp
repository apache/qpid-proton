#ifndef PROTON_CPP_MESSAGING_HANDLER_H
#define PROTON_CPP_MESSAGING_HANDLER_H

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
#include "proton/acking.hpp"
#include "proton/event.h"

namespace proton {

class event;
class messaging_adapter;

class messaging_handler : public proton_handler , public acking
{
  public:
    PN_CPP_EXTERN messaging_handler(int prefetch=10, bool auto_accept=true, bool auto_settle=true,
                                       bool peer_close_isError=false);
    PN_CPP_EXTERN virtual ~messaging_handler();

    PN_CPP_EXTERN virtual void on_abort(event &e);
    PN_CPP_EXTERN virtual void on_accepted(event &e);
    PN_CPP_EXTERN virtual void on_commit(event &e);
    PN_CPP_EXTERN virtual void on_connection_closed(event &e);
    PN_CPP_EXTERN virtual void on_connection_closing(event &e);
    PN_CPP_EXTERN virtual void on_connection_error(event &e);
    PN_CPP_EXTERN virtual void on_connection_opening(event &e);
    PN_CPP_EXTERN virtual void on_connection_opened(event &e);
    PN_CPP_EXTERN virtual void on_disconnected(event &e);
    PN_CPP_EXTERN virtual void on_fetch(event &e);
    PN_CPP_EXTERN virtual void on_idLoaded(event &e);
    PN_CPP_EXTERN virtual void on_link_closed(event &e);
    PN_CPP_EXTERN virtual void on_link_closing(event &e);
    PN_CPP_EXTERN virtual void on_link_error(event &e);
    PN_CPP_EXTERN virtual void on_link_opened(event &e);
    PN_CPP_EXTERN virtual void on_link_opening(event &e);
    PN_CPP_EXTERN virtual void on_message(event &e);
    PN_CPP_EXTERN virtual void on_quit(event &e);
    PN_CPP_EXTERN virtual void on_record_inserted(event &e);
    PN_CPP_EXTERN virtual void on_records_loaded(event &e);
    PN_CPP_EXTERN virtual void on_rejected(event &e);
    PN_CPP_EXTERN virtual void on_released(event &e);
    PN_CPP_EXTERN virtual void on_request(event &e);
    PN_CPP_EXTERN virtual void on_response(event &e);
    PN_CPP_EXTERN virtual void on_sendable(event &e);
    PN_CPP_EXTERN virtual void on_session_closed(event &e);
    PN_CPP_EXTERN virtual void on_session_closing(event &e);
    PN_CPP_EXTERN virtual void on_session_error(event &e);
    PN_CPP_EXTERN virtual void on_session_opened(event &e);
    PN_CPP_EXTERN virtual void on_session_opening(event &e);
    PN_CPP_EXTERN virtual void on_settled(event &e);
    PN_CPP_EXTERN virtual void on_start(event &e);
    PN_CPP_EXTERN virtual void on_timer(event &e);
    PN_CPP_EXTERN virtual void on_transaction_aborted(event &e);
    PN_CPP_EXTERN virtual void on_transaction_committed(event &e);
    PN_CPP_EXTERN virtual void on_transaction_declared(event &e);
    PN_CPP_EXTERN virtual void on_transport_closed(event &e);

 private:
    int prefetch_;
    bool auto_accept_;
    bool auto_settle_;
    bool peer_close_iserror_;
    messaging_adapter *messaging_adapter_;
    handler *flow_controller_;
    PN_CPP_EXTERN messaging_handler(
        bool raw_handler, int prefetch=10, bool auto_accept=true,
        bool auto_settle=true, bool peer_close_isError=false);
    friend class container_impl;
    friend class messaging_adapter;
    PN_CPP_EXTERN void create_helpers();
};

}

#endif  /*!PROTON_CPP_MESSAGING_HANDLER_H*/
