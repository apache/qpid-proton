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

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/error_condition.hpp"
#include "proton/receiver.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/session.hpp"
#include "proton/transport.hpp"

#include "proton_bits.hpp"

#include "proton/connection.h"
#include "proton/session.h"

namespace proton {

messaging_handler::messaging_handler(){}

messaging_handler::~messaging_handler(){}

void messaging_handler::on_container_start(container &) {}
void messaging_handler::on_container_stop(container &) {}
void messaging_handler::on_message(delivery &, message &) {}
void messaging_handler::on_sendable(sender &) {}
void messaging_handler::on_transport_close(transport &) {}
void messaging_handler::on_transport_error(transport &t) { on_error(t.error()); }
void messaging_handler::on_transport_open(transport &) {}

void messaging_handler::on_connection_close(connection &) {}
void messaging_handler::on_connection_error(connection &c) { on_error(c.error()); }
void messaging_handler::on_connection_open(connection &c) {
    if (c.uninitialized()) {
        pn_connection_open(unwrap(c));
    }
}
void messaging_handler::on_connection_wake(connection&) {}

void messaging_handler::on_session_close(session &) {}
void messaging_handler::on_session_error(session &s) { on_error(s.error()); }
void messaging_handler::on_session_open(session &s) {
    if (s.uninitialized()) {
        pn_session_open(unwrap(s));
    }
}
void messaging_handler::on_receiver_close(receiver &) {}
void messaging_handler::on_receiver_error(receiver &l) { on_error(l.error()); }
void messaging_handler::on_receiver_open(receiver &l) {
    if (l.uninitialized()) {
        l.open(l.connection().receiver_options());
    }
}
void messaging_handler::on_receiver_detach(receiver &) {}
void messaging_handler::on_sender_close(sender &) {}
void messaging_handler::on_sender_error(sender &l) { on_error(l.error()); }
void messaging_handler::on_sender_open(sender &l) {
    if (l.uninitialized()) {
        l.open(l.connection().sender_options());
    }
}
void messaging_handler::on_sender_detach(sender &) {}
void messaging_handler::on_tracker_accept(tracker &) {}
void messaging_handler::on_tracker_reject(tracker &) {}
void messaging_handler::on_tracker_release(tracker &) {}
void messaging_handler::on_tracker_settle(tracker &) {}
void messaging_handler::on_delivery_settle(delivery &) {}
void messaging_handler::on_sender_drain_start(sender &) {}
void messaging_handler::on_receiver_drain_finish(receiver &) {}

void messaging_handler::on_error(const error_condition& c) { throw proton::error(c.what()); }

}
