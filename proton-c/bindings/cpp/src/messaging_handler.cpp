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
#include "messaging_adapter.hpp"

#include "proton/handlers.h"

#include <algorithm>

namespace proton {

messaging_handler::messaging_handler(int prefetch0, bool auto_accept0, bool auto_settle0, bool peer_close_is_error0) :
    messaging_adapter_(new messaging_adapter(*this, prefetch0, auto_accept0, auto_settle0, peer_close_is_error0))
{}

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

void messaging_handler::on_unhandled(event &) {}
void messaging_handler::on_unhandled_error(event &) {}
}
