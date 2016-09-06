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

connection_engine::connection_engine() :
    container_(0)
{
    int err;
    if ((err = pn_connection_engine_init(&c_engine_)))
        throw proton::error(std::string("connection_engine init failed: ")+pn_code(err));
}

connection_engine::connection_engine(class container& cont, event_loop* loop) :
    container_(&cont)
{
    int err;
    if ((err = pn_connection_engine_init(&c_engine_)))
        throw proton::error(std::string("connection_engine init failed: ")+pn_code(err));
    connection_context& ctx = connection_context::get(connection());
    ctx.container = container_;
    ctx.event_loop.reset(loop);
}

void connection_engine::configure(const connection_options& opts) {
    proton::connection c = connection();
    opts.apply_unbound(c);
    opts.apply_bound(c);
    handler_ =  opts.handler();
    connection_context::get(connection()).collector = c_engine_.collector;
    pn_connection_engine_start(&c_engine_);
}

connection_engine::~connection_engine() {
    pn_connection_engine_final(&c_engine_);
}

void connection_engine::connect(const connection_options& opts) {
    connection_options all;
    if (container_) {
        all.container_id(container_->id());
        all.update(container_->client_connection_options());
    }
    all.update(opts);
    configure(all);
    connection().open();
}

void connection_engine::accept(const connection_options& opts) {
    connection_options all;
    if (container_) {
        all.container_id(container_->id());
        all.update(container_->server_connection_options());
    }
    all.update(opts);
    configure(all);
}

bool connection_engine::dispatch() {
    pn_event_t* c_event;
    while ((c_event = pn_connection_engine_dispatch(&c_engine_)) != NULL) {
        proton_event cpp_event(c_event, container_);
        try {
            if (handler_ != 0) {
                messaging_adapter adapter(*handler_);
                cpp_event.dispatch(adapter);
            }
        } catch (const std::exception& e) {
            disconnected(error_condition("exception", e.what()));
        }
    }
    return !pn_connection_engine_finished(&c_engine_);
}

mutable_buffer connection_engine::read_buffer() {
    pn_rwbytes_t buffer = pn_connection_engine_read_buffer(&c_engine_);
    return mutable_buffer(buffer.start, buffer.size);
}

void connection_engine::read_done(size_t n) {
    return pn_connection_engine_read_done(&c_engine_, n);
}

void connection_engine::read_close() {
    pn_connection_engine_read_close(&c_engine_);
}

const_buffer connection_engine::write_buffer() {
    pn_bytes_t buffer = pn_connection_engine_write_buffer(&c_engine_);
    return const_buffer(buffer.start, buffer.size);
}

void connection_engine::write_done(size_t n) {
    return pn_connection_engine_write_done(&c_engine_, n);
}

void connection_engine::write_close() {
    pn_connection_engine_write_close(&c_engine_);
}

void connection_engine::disconnected(const proton::error_condition& err) {
    pn_condition_t* condition = pn_connection_engine_condition(&c_engine_);
    if (!pn_condition_is_set(condition))     // Don't overwrite existing condition
        set_error_condition(err, condition);
    pn_connection_engine_disconnected(&c_engine_);
}

proton::connection connection_engine::connection() const {
    return make_wrapper(c_engine_.connection);
}

proton::transport connection_engine::transport() const {
    return make_wrapper(c_engine_.transport);
}

proton::container* connection_engine::container() const {
    return container_;
}

}}
