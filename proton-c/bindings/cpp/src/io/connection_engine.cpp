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

#include "proton/io/connection_engine.hpp"

#include "proton/event_loop.hpp"
#include "proton/error.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/uuid.hpp"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <proton/connection.h>
#include <proton/transport.h>
#include <proton/event.h>

#include <algorithm>


namespace proton {
namespace io {

void connection_engine::init() {
    if (pn_connection_engine_init(&engine_, pn_connection(), pn_transport()) != 0) {
        this->~connection_engine(); // Dtor won't be called on throw from ctor.
        throw proton::error(std::string("connection_engine allocation failed"));
    }
}

connection_engine::connection_engine() : handler_(0), container_(0) { init(); }

connection_engine::connection_engine(class container& cont, event_loop* loop) : handler_(0), container_(&cont) {
    init();
    connection_context& ctx = connection_context::get(connection());
    ctx.container = container_;
    ctx.event_loop.reset(loop);
}

connection_engine::~connection_engine() {
    pn_connection_engine_destroy(&engine_);
}

void connection_engine::configure(const connection_options& opts, bool server) {
    proton::connection c(connection());
    opts.apply_unbound(c);
    if (server) pn_transport_set_server(engine_.transport);
    pn_connection_engine_bind(&engine_);
    opts.apply_bound(c);
    handler_ =  opts.handler();
    connection_context::get(connection()).collector = engine_.collector;
}

void connection_engine::connect(const connection_options& opts) {
    connection_options all;
    if (container_) {
        all.container_id(container_->id());
        all.update(container_->client_connection_options());
    }
    all.update(opts);
    configure(all, false);
    connection().open();
}

void connection_engine::accept(const connection_options& opts) {
    connection_options all;
    if (container_) {
        all.container_id(container_->id());
        all.update(container_->server_connection_options());
    }
    all.update(opts);
    configure(all, true);
}

bool connection_engine::dispatch() {
    pn_event_t* c_event;
    while ((c_event = pn_connection_engine_event(&engine_)) != NULL) {
        proton_event cpp_event(c_event, container_);
        try {
            if (handler_ != 0) {
                messaging_adapter adapter(*handler_);
                cpp_event.dispatch(adapter);
            }
        } catch (const std::exception& e) {
            pn_condition_t *cond = pn_transport_condition(engine_.transport);
            if (!pn_condition_is_set(cond)) {
                pn_condition_format(cond, "exception", "%s", e.what());
            }
        }
        pn_connection_engine_pop_event(&engine_);
    }
    return !pn_connection_engine_finished(&engine_);
}

mutable_buffer connection_engine::read_buffer() {
    pn_rwbytes_t buffer = pn_connection_engine_read_buffer(&engine_);
    return mutable_buffer(buffer.start, buffer.size);
}

void connection_engine::read_done(size_t n) {
    return pn_connection_engine_read_done(&engine_, n);
}

void connection_engine::read_close() {
    pn_connection_engine_read_close(&engine_);
}

const_buffer connection_engine::write_buffer() {
    pn_bytes_t buffer = pn_connection_engine_write_buffer(&engine_);
    return const_buffer(buffer.start, buffer.size);
}

void connection_engine::write_done(size_t n) {
    return pn_connection_engine_write_done(&engine_, n);
}

void connection_engine::write_close() {
    pn_connection_engine_write_close(&engine_);
}

void connection_engine::disconnected(const proton::error_condition& err) {
    pn_condition_t* condition = pn_transport_condition(engine_.transport);
    if (!pn_condition_is_set(condition))  {
        set_error_condition(err, condition);
    }
    pn_connection_engine_close(&engine_);
}

proton::connection connection_engine::connection() const {
    return make_wrapper(engine_.connection);
}

proton::transport connection_engine::transport() const {
    return make_wrapper(engine_.transport);
}

proton::container* connection_engine::container() const {
    return container_;
}

}}
