/*
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
 */

#include "proton/io/connection_driver.hpp"

#include "proton/connection.hpp"
#include "proton/container.hpp"
#include "proton/error.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/transport.hpp"
#include "proton/uuid.hpp"
#include "proton/work_queue.hpp"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"

#include <proton/connection.h>
#include <proton/transport.h>
#include <proton/event.h>

#include <algorithm>


namespace proton {
namespace io {

void connection_driver::init() {
    if (pn_connection_driver_init(&driver_, pn_connection(), pn_transport()) != 0) {
        this->~connection_driver(); // Dtor won't be called on throw from ctor.
        throw proton::error(std::string("connection_driver allocation failed"));
    }
}

connection_driver::connection_driver() : handler_(0) { init(); }

connection_driver::connection_driver(const std::string& id) : container_id_(id), handler_(0) {
    init();
}

connection_driver::~connection_driver() {
    pn_connection_driver_destroy(&driver_);
}

void connection_driver::configure(const connection_options& opts, bool server) {
    proton::connection c(connection());
    opts.apply_unbound(c);
    if (server) {
        pn_transport_set_server(driver_.transport);
        opts.apply_unbound_server(driver_.transport);
    } else {
        opts.apply_unbound_client(driver_.transport);
    }
    pn_connection_driver_bind(&driver_);
    handler_ =  opts.handler();
}

void connection_driver::connect(const connection_options& opts) {
    connection_options all;
    all.container_id(container_id_);
    all.update(opts);
    configure(all, false);
    connection().open();
}

void connection_driver::accept(const connection_options& opts) {
    connection_options all;
    all.container_id(container_id_);
    all.update(opts);
    configure(all, true);
}

bool connection_driver::has_events() const {
    return driver_.collector && pn_collector_peek(driver_.collector);
}

bool connection_driver::dispatch() {
    pn_event_t* c_event;
    while ((c_event = pn_connection_driver_next_event(&driver_)) != NULL) {
        try {
            if (handler_ != 0) {
                messaging_adapter::dispatch(*handler_, c_event);
            }
        } catch (const std::exception& e) {
            pn_condition_t *cond = pn_transport_condition(driver_.transport);
            if (!pn_condition_is_set(cond)) {
                pn_condition_format(cond, "exception", "%s", e.what());
            }
        }
    }
    return !pn_connection_driver_finished(&driver_);
}

mutable_buffer connection_driver::read_buffer() {
    pn_rwbytes_t buffer = pn_connection_driver_read_buffer(&driver_);
    return mutable_buffer(buffer.start, buffer.size);
}

void connection_driver::read_done(size_t n) {
    return pn_connection_driver_read_done(&driver_, n);
}

void connection_driver::read_close() {
    pn_connection_driver_read_close(&driver_);
}

const_buffer connection_driver::write_buffer() {
    pn_bytes_t buffer = pn_connection_driver_write_buffer(&driver_);
    return const_buffer(buffer.start, buffer.size);
}

void connection_driver::write_done(size_t n) {
    return pn_connection_driver_write_done(&driver_, n);
}

void connection_driver::write_close() {
    pn_connection_driver_write_close(&driver_);
}

timestamp connection_driver::tick(timestamp now) {
    return timestamp(pn_transport_tick(driver_.transport, now.milliseconds()));
}

void connection_driver::disconnected(const proton::error_condition& err) {
    pn_condition_t* condition = pn_transport_condition(driver_.transport);
    if (!pn_condition_is_set(condition))  {
        set_error_condition(err, condition);
    }
    pn_connection_driver_close(&driver_);
}

proton::connection connection_driver::connection() const {
    return make_wrapper(driver_.connection);
}

proton::transport connection_driver::transport() const {
    return make_wrapper(driver_.transport);
}

}}
