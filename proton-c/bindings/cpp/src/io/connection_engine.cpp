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
#include "proton/error.hpp"
#include "proton/handler.hpp"
#include "proton/uuid.hpp"

#include "contexts.hpp"
#include "id_generator.hpp"
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

connection_engine::connection_engine(class handler &h, const connection_options& opts):
    handler_(h),
    connection_(make_wrapper(internal::take_ownership(pn_connection()).get())),
    transport_(make_wrapper(internal::take_ownership(pn_transport()).get())),
    collector_(internal::take_ownership(pn_collector()).get())
{
    if (!connection_ || !transport_ || !collector_)
        throw proton::error("engine create");
    transport_.bind(connection_);
    pn_connection_collect(unwrap(connection_), collector_.get());
    opts.apply(connection_);

    // Provide local random defaults for connection_id and link_prefix if not by opts.
    if (connection_.container_id().empty())
        pn_connection_set_container(unwrap(connection_), uuid::random().str().c_str());
    id_generator &link_gen = connection_context::get(connection_).link_gen;
    if (link_gen.prefix().empty())
        link_gen.prefix(uuid::random().str()+"/");
}

connection_engine::~connection_engine() {
    transport_.unbind();
    pn_collector_free(collector_.release()); // Break cycle with connection_
}

bool connection_engine::dispatch() {
    proton_handler& h = *handler_.messaging_adapter_;
    for (pn_event_t *e = pn_collector_peek(collector_.get());
         e;
         e = pn_collector_peek(collector_.get()))
    {
        proton_event pe(e, 0);
        try {
            pe.dispatch(h);
        } catch (const std::exception& e) {
            close(error_condition("exception", e.what()));
        }
        pn_collector_pop(collector_.get());
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

void connection_engine::close(const proton::error_condition& err) {
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

void connection_engine::work_queue(class work_queue* wq) {
    connection_context::get(connection()).work_queue = wq;
}

}}
