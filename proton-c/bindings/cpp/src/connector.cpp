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
#include "proton/url.hpp"
#include "proton/reconnect_timer.hpp"
#include "proton/task.hpp"
#include "proton/sasl.hpp"

#include "container_impl.hpp"
#include "proton_event.hpp"

#include "proton/connection.h"
#include "proton/transport.h"

namespace proton {

connector::connector(connection&c, const connection_options &opts) :
    connection_(c), options_(opts), reconnect_timer_(0), transport_configured_(false)
{}

connector::~connector() { delete reconnect_timer_; }

void connector::address(const url &a) {
    address_ = a;
}

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
    connection_.host(address_.host_port());
    pn_transport_t *pnt = pn_transport();
    transport t(pnt);
    if (!address_.username().empty())
        connection_.user(address_.username());
    if (!address_.password().empty())
        connection_.password(address_.password());
    t.bind(connection_);
    pn_decref(pnt);
    // Apply options to the new transport.
    options_.apply(connection_);
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

void connector::on_transport_closed(proton_event &e) {
    if (!connection_) return;
    if (connection_.state() & endpoint::LOCAL_ACTIVE) {
        if (reconnect_timer_) {
            e.connection().transport().unbind();
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
                    connection_.container().impl_.get()->schedule(delay, this);
                    return;
                }
            }
        }
    }
    connection_.release();
    connection_  = 0;
}

void connector::on_timer_task(proton_event &) {
    connect();
}

}
