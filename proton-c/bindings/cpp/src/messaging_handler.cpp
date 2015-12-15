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
#include "proton_event.hpp"
#include "proton/messaging_adapter.hpp"
#include "proton/handlers.h"
#include <algorithm>

namespace proton {

namespace {
class c_flow_controller : public proton_handler
{
  public:
    pn_handler_t *flowcontroller;

    // TODO: pn_flowcontroller requires a window > 1.
    c_flow_controller(int window) : flowcontroller(pn_flowcontroller(std::max(window, 2))) {}
    ~c_flow_controller() {
        pn_decref(flowcontroller);
    }

    void redirect(event &e) {
        proton_event *pne = dynamic_cast<proton_event *>(&e);
        pn_handler_dispatch(flowcontroller, pne->pn_event(), (pn_event_type_t) pne->type());
    }

    virtual void on_link_local_open(event &e) { redirect(e); }
    virtual void on_link_remote_open(event &e) { redirect(e); }
    virtual void on_link_flow(event &e) { redirect(e); }
    virtual void on_delivery(event &e) { redirect(e); }
};

} // namespace




messaging_handler::messaging_handler(int prefetch0, bool auto_accept0, bool auto_settle0, bool peer_close_is_error0) :
    prefetch_(prefetch0), auto_accept_(auto_accept0), auto_settle_(auto_settle0),
    peer_close_iserror_(peer_close_is_error0)
{
    create_helpers();
}

messaging_handler::messaging_handler(bool raw_handler, int prefetch0, bool auto_accept0, bool auto_settle0, bool peer_close_is_error0) :
    prefetch_(prefetch0), auto_accept_(auto_accept0), auto_settle_(auto_settle0),
    peer_close_iserror_(peer_close_is_error0)
{
    if (!raw_handler) {
        create_helpers();
    }
}

void messaging_handler::create_helpers() {
    if (prefetch_ > 0) {
        flow_controller_.reset(new c_flow_controller(prefetch_));
        add_child_handler(*flow_controller_);
    }
    messaging_adapter_.reset(new messaging_adapter(*this));
    add_child_handler(*messaging_adapter_);
}

messaging_handler::~messaging_handler(){}

void messaging_handler::on_delivery_accept(event &e) { on_unhandled(e); }
void messaging_handler::on_connection_close(event &e) { on_unhandled(e); }
void messaging_handler::on_connection_error(event &e) { on_unhandled(e); }
void messaging_handler::on_connection_open(event &e) { on_unhandled(e); }
void messaging_handler::on_disconnect(event &e) { on_unhandled(e); }
void messaging_handler::on_link_close(event &e) { on_unhandled(e); }
void messaging_handler::on_link_error(event &e) { on_unhandled(e); }
void messaging_handler::on_link_open(event &e) { on_unhandled(e); }
void messaging_handler::on_message(event &e) { on_unhandled(e); }
void messaging_handler::on_delivery_reject(event &e) { on_unhandled(e); }
void messaging_handler::on_delivery_release(event &e) { on_unhandled(e); }
void messaging_handler::on_sendable(event &e) { on_unhandled(e); }
void messaging_handler::on_session_close(event &e) { on_unhandled(e); }
void messaging_handler::on_session_error(event &e) { on_unhandled(e); }
void messaging_handler::on_session_open(event &e) { on_unhandled(e); }
void messaging_handler::on_delivery_settle(event &e) { on_unhandled(e); }
void messaging_handler::on_start(event &e) { on_unhandled(e); }
void messaging_handler::on_timer(event &e) { on_unhandled(e); }
void messaging_handler::on_transaction_abort(event &e) { on_unhandled(e); }
void messaging_handler::on_transaction_commit(event &e) { on_unhandled(e); }
void messaging_handler::on_transaction_declare(event &e) { on_unhandled(e); }

}
