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
#include "proton/io/link_namer.hpp"

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

#include <iosfwd>

#include <assert.h>

namespace proton {
namespace io {

connection_engine::connection_engine(class container& cont, link_namer& namer, event_loop* loop) :
    handler_(0),
    connection_(make_wrapper(internal::take_ownership(pn_connection()).get())),
    transport_(make_wrapper(internal::take_ownership(pn_transport()).get())),
    collector_(internal::take_ownership(pn_collector()).get()),
    container_(cont)
{
    if (!connection_ || !transport_ || !collector_)
        throw proton::error("connection_engine create failed");
    pn_transport_bind(unwrap(transport_), unwrap(connection_));
    pn_connection_collect(unwrap(connection_), collector_.get());
    connection_context& ctx = connection_context::get(connection_);
    ctx.container = &container_;
    ctx.link_gen = &namer;
    ctx.event_loop.reset(loop);
}

void connection_engine::configure(const connection_options& opts) {
    opts.apply(connection_);
    handler_ = opts.handler();
    if (handler_) {
        collector_ = internal::take_ownership(pn_collector());
        pn_connection_collect(unwrap(connection_), collector_.get());
    }
    connection_context::get(connection_).collector = collector_.get();
}

connection_engine::~connection_engine() {
    pn_transport_unbind(unwrap(transport_));
    if (collector_.get())
        pn_collector_free(collector_.release()); // Break cycle with connection_
}

void connection_engine::connect(const connection_options& opts) {
    connection_options all;
    all.container_id(container_.id());
    all.update(container_.client_connection_options());
    all.update(opts);
    configure(all);
    connection().open();
}

void connection_engine::accept(const connection_options& opts) {
    connection_options all;
    all.container_id(container_.id());
    all.update(container_.server_connection_options());
    all.update(opts);
    configure(all);
}

bool connection_engine::dispatch() {
    if (collector_.get()) {
        for (pn_event_t *e = pn_collector_peek(collector_.get());
             e;
             e = pn_collector_peek(collector_.get()))
        {
            proton_event pe(e, container_);
            try {
                pe.dispatch(*handler_);
            } catch (const std::exception& e) {
                disconnected(error_condition("exception", e.what()));
            }
            pn_collector_pop(collector_.get());
        }
    }
    return !(pn_transport_closed(unwrap(transport_)));
}

mutable_buffer connection_engine::read_buffer() {
    ssize_t cap = pn_transport_capacity(unwrap(transport_));
    if (cap > 0)
        return mutable_buffer(pn_transport_tail(unwrap(transport_)), cap);
    else
        return mutable_buffer(0, 0);
}

void connection_engine::read_done(size_t n) {
    if (n > 0)
        pn_transport_process(unwrap(transport_), n);
}

void connection_engine::read_close() {
    pn_transport_close_tail(unwrap(transport_));
}

const_buffer connection_engine::write_buffer() const {
    ssize_t pending = pn_transport_pending(unwrap(transport_));
    if (pending > 0)
        return const_buffer(pn_transport_head(unwrap(transport_)), pending);
    else
        return const_buffer(0, 0);
}

void connection_engine::write_done(size_t n) {
    if (n > 0)
        pn_transport_pop(unwrap(transport_), n);
}

void connection_engine::write_close() {
    pn_transport_close_head(unwrap(transport_));
}

void connection_engine::disconnected(const proton::error_condition& err) {
    set_error_condition(err, pn_transport_condition(unwrap(transport_)));
    read_close();
    write_close();
}

proton::connection connection_engine::connection() const {
    return connection_;
}

proton::transport connection_engine::transport() const {
    return transport_;
}

proton::container& connection_engine::container() const {
    return container_;
}

}}
