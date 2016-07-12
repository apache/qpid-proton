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
#include "proton_handler.hpp"
#include "proton_event.hpp"

namespace proton {

proton_handler::proton_handler() {}
proton_handler::~proton_handler() {}

// Everything goes to on_unhandled() unless overriden by subclass

void proton_handler::on_reactor_init(proton_event &e) { on_unhandled(e); }
void proton_handler::on_reactor_quiesced(proton_event &e) { on_unhandled(e); }
void proton_handler::on_reactor_final(proton_event &e) { on_unhandled(e); }
void proton_handler::on_timer_task(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_init(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_bound(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_unbound(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_local_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_local_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_remote_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_remote_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_connection_final(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_init(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_local_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_local_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_remote_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_remote_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_session_final(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_init(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_local_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_local_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_local_detach(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_remote_open(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_remote_close(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_remote_detach(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_flow(proton_event &e) { on_unhandled(e); }
void proton_handler::on_link_final(proton_event &e) { on_unhandled(e); }
void proton_handler::on_delivery(proton_event &e) { on_unhandled(e); }
void proton_handler::on_transport(proton_event &e) { on_unhandled(e); }
void proton_handler::on_transport_error(proton_event &e) { on_unhandled(e); }
void proton_handler::on_transport_head_closed(proton_event &e) { on_unhandled(e); }
void proton_handler::on_transport_tail_closed(proton_event &e) { on_unhandled(e); }
void proton_handler::on_transport_closed(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_init(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_updated(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_readable(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_writable(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_expired(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_error(proton_event &e) { on_unhandled(e); }
void proton_handler::on_selectable_final(proton_event &e) { on_unhandled(e); }

void proton_handler::on_unhandled(proton_event &) {}

}
