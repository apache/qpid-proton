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

#include "proton/connection.hpp"
#include "proton/transport.hpp"

#include "proton_event.hpp"
#include "messaging_adapter.hpp"

#include "proton/handlers.h"

#include <algorithm>

namespace proton {

handler::handler() : messaging_adapter_(new messaging_adapter(*this)) {}

handler::~handler(){}

void handler::on_container_start(container &) {}
void handler::on_message(delivery &, message &) {}
void handler::on_sendable(sender &) {}
void handler::on_timer(container &) {}
void handler::on_transport_close(transport &) {}
void handler::on_transport_error(transport &t) { on_unhandled_error(t.condition()); }
void handler::on_connection_close(connection &) {}
void handler::on_connection_error(connection &c) { on_unhandled_error(c.remote_condition()); }
void handler::on_connection_open(connection &) {}
void handler::on_session_close(session &) {}
void handler::on_session_error(session &s) { on_unhandled_error(s.remote_condition()); }
void handler::on_session_open(session &) {}
void handler::on_receiver_close(receiver &) {}
void handler::on_receiver_error(receiver &l) { on_unhandled_error(l.remote_condition()); }
void handler::on_receiver_open(receiver &) {}
void handler::on_sender_close(sender &) {}
void handler::on_sender_error(sender &l) { on_unhandled_error(l.remote_condition()); }
void handler::on_sender_open(sender &) {}
void handler::on_delivery_accept(delivery &) {}
void handler::on_delivery_reject(delivery &) {}
void handler::on_delivery_release(delivery &) {}
void handler::on_delivery_settle(delivery &) {}

void handler::on_unhandled_error(const condition& c) { throw proton::error(c.what()); }

}
