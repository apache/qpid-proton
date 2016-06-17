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

#include "connector.hpp"

#include "proton/connection.hpp"
#include "proton/transport.hpp"
#include "proton/container.hpp"
#include "proton/reconnect_timer.hpp"
#include "proton/task.hpp"
#include "proton/sasl.hpp"
#include "proton/url.hpp"

#include "container_impl.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <proton/connection.h>
#include <proton/transport.h>

namespace proton {

connector::connector(connection&c, const url& a, const connection_options &opts) :
    connection_(c), address_(a), options_(opts), reconnect_timer_(0), transport_configured_(false)
{}

connector::~connector() { delete reconnect_timer_; }

void connector::apply_options() {
    if (!connection_) return;
    options_.apply(connection_);
}

bool connector::transport_configured() { return transport_configured_; }

void connector::reconnect_timer(const class reconnect_timer &rt) {
    delete reconnect_timer_;
    reconnect_timer_ = new class reconnect_timer(rt);
}

void connector::connect() {
    pn_transport_t *pnt = pn_transport();
    transport t(make_wrapper(pnt));
    if (!address_.user().empty())
        connection_.user(address_.user());
    if (!address_.password().empty())
        connection_.password(address_.password());
    pn_transport_bind(pnt, unwrap(connection_));
    pn_decref(pnt);
    // Apply options to the new transport.
    options_.apply(connection_);
    // if virtual-host not set, use host from address as default
    if (!options_.is_virtual_host_set())
        pn_connection_set_hostname(unwrap(connection_), address_.host().c_str());
    transport_configured_ = true;
}

void connector::on_connection_local_open(proton_event &) {
    connect();
}

void connector::on_connection_remote_open(proton_event &) {
    if (reconnect_timer_) {
        reconnect_timer_->reset();
    }
}

void connector::on_connection_init(proton_event &) {
}

void connector::on_transport_tail_closed(proton_event &e) {
    on_transport_closed(e);
}

void connector::on_transport_closed(proton_event &) {
    if (!connection_) return;
    if (connection_.active()) {
        if (reconnect_timer_) {
            pn_transport_unbind(unwrap(connection_.transport()));
            transport_configured_ = false;
            int delay = reconnect_timer_->next_delay(timestamp::now());
            if (delay >= 0) {
                if (delay == 0) {
                    // log "Disconnected, reconnecting..."
                    connect();
                    return;
                }
                else {
                    // log "Disconnected, reconnecting in " <<  delay << " milliseconds"
                    static_cast<container_impl&>(connection_.container()).schedule(delay, this);
                    return;
                }
            }
        }
    }
    pn_connection_release(unwrap(connection_));
    connection_  = 0;
}

void connector::on_timer_task(proton_event &) {
    connect();
}

}
