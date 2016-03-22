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

#include "proton/error.hpp"
#include "proton/transport.hpp"

#include "proton_event.hpp"
#include "messaging_adapter.hpp"

#include "proton/handlers.h"

#include <algorithm>

namespace proton {

handler::handler() : messaging_adapter_(new messaging_adapter(*this)) {}

handler::~handler(){}

void handler::on_start(event &e) { on_unhandled(e); }
void handler::on_message(event &e, message &) { on_unhandled(e); }
void handler::on_sendable(event &e, sender &) { on_unhandled(e); }
void handler::on_timer(event &e) { on_unhandled(e); }
void handler::on_transport_close(event &e, transport &) { on_unhandled(e); }
void handler::on_transport_error(event &e, transport &t) { on_unhandled_error(e, t.condition()); }
void handler::on_connection_close(event &e, connection &) { on_unhandled(e); }
void handler::on_connection_error(event &e, connection &c) { on_unhandled_error(e, c.remote_condition()); }
void handler::on_connection_open(event &e, connection &) { on_unhandled(e); }
void handler::on_session_close(event &e, session &) { on_unhandled(e); }
void handler::on_session_error(event &e, session &s) { on_unhandled_error(e, s.remote_condition()); }
void handler::on_session_open(event &e, session &) { on_unhandled(e); }
void handler::on_receiver_close(event &e, receiver &) { on_unhandled(e); }
void handler::on_receiver_error(event &e, receiver &l) { on_unhandled_error(e, l.remote_condition()); }
void handler::on_receiver_open(event &e, receiver &) { on_unhandled(e); }
void handler::on_sender_close(event &e, sender &) { on_unhandled(e); }
void handler::on_sender_error(event &e, sender &l) { on_unhandled_error(e, l.remote_condition()); }
void handler::on_sender_open(event &e, sender &) { on_unhandled(e); }
void handler::on_delivery_accept(event &e, delivery &) { on_unhandled(e); }
void handler::on_delivery_reject(event &e, delivery &) { on_unhandled(e); }
void handler::on_delivery_release(event &e, delivery &) { on_unhandled(e); }
void handler::on_delivery_settle(event &e, delivery &) { on_unhandled(e); }

void handler::on_unhandled(event &) {}
void handler::on_unhandled_error(event &, const condition& c) { throw proton::error(c.what()); }

}
