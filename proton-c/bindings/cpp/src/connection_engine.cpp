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

#include "proton/connection_engine.hpp"
#include "proton/error.hpp"
#include "proton/handler.hpp"
#include "proton/uuid.hpp"

#include "contexts.hpp"
#include "id_generator.hpp"
#include "messaging_adapter.hpp"
#include "messaging_event.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "proton_bits.hpp"

#include <proton/connection.h>
#include <proton/transport.h>
#include <proton/event.h>

#include <algorithm>

#include <iosfwd>

namespace proton {

namespace {
void set_error(connection_engine_context *ctx_, const std::string& reason) {
    pn_condition_t *c = pn_transport_condition(ctx_->transport);
    pn_condition_set_name(c, "io_error");
    pn_condition_set_description(c, reason.c_str());
}

void close_transport(connection_engine_context *ctx_) {
    if (pn_transport_pending(ctx_->transport) >= 0)
        pn_transport_close_head(ctx_->transport);
    if (pn_transport_capacity(ctx_->transport) >= 0)
        pn_transport_close_tail(ctx_->transport);
}

std::string  make_id(const std::string s="") { return s.empty() ? uuid::random().str() : s; }
}

connection_engine::io_error::io_error(const std::string& msg) : error(msg) {}

class connection_engine::container::impl {
  public:
    impl(const std::string s="") : id_(make_id(s)) {}

    const std::string id_;
    id_generator id_gen_;
    connection_options options_;
};

connection_engine::container::container(const std::string& s) : impl_(new impl(s)) {}

connection_engine::container::~container() {}

std::string connection_engine::container::id() const { return impl_->id_; }

connection_options connection_engine::container::make_options() {
    connection_options opts = impl_->options_;
    opts.container_id(id()).link_prefix(impl_->id_gen_.next()+"/");
    return opts;
}

void connection_engine::container::options(const connection_options &opts) {
    impl_->options_ = opts;
}

connection_engine::connection_engine(class handler &h, const connection_options& opts) {
    connection_ = proton::connection(internal::take_ownership(pn_connection()).get());
    internal::pn_ptr<pn_transport_t> transport = internal::take_ownership(pn_transport());
    internal::pn_ptr<pn_collector_t> collector = internal::take_ownership(pn_collector());
    if (!connection_ || !transport || !collector)
        throw proton::error("engine create");
    int err = pn_transport_bind(transport.get(), connection_.pn_object());
    if (err)
        throw error(msg() << "transport bind:" << pn_code(err));
    pn_connection_collect(connection_.pn_object(), collector.get());

    ctx_ = &connection_engine_context::get(connection_); // Creates context
    ctx_->engine_handler = &h;
    ctx_->transport = transport.release();
    ctx_->collector = collector.release();
    opts.apply(connection_);
    // Provide defaults for connection_id and link_prefix if not set.
    std::string cid = connection_.container_id();
    if (cid.empty()) {
        cid = make_id();
        pn_connection_set_container(connection_.pn_object(), cid.c_str());
    }
    id_generator &link_gen = connection_context::get(connection_).link_gen;
    if (link_gen.prefix().empty()) {
        link_gen.prefix(make_id()+"/");
    }
}

connection_engine::~connection_engine() {
    pn_transport_unbind(ctx_->transport);
    pn_transport_free(ctx_->transport);
    internal::pn_ptr<pn_connection_t> c(connection_.pn_object());
    connection_ = proton::connection();
    pn_connection_free(c.release());
    pn_collector_free(ctx_->collector);
}

void connection_engine::process(int flags) {
    if (closed()) return;
    if (flags & WRITE) try_write();
    dispatch();
    if (flags & READ) try_read();
    dispatch();

    if (connection_.closed() && !closed()) {
        dispatch();
        while (can_write()) {
            try_write(); // Flush final data.
        }
        // no transport errors.
        close_transport(ctx_);
    }
    if (closed()) {
        pn_transport_unbind(ctx_->transport);
        dispatch();
        try { io_close(); } catch(const io_error&) {} // Tell the IO to close.
    }
}

void connection_engine::dispatch() {
    proton_handler& h = *ctx_->engine_handler->messaging_adapter_;
    pn_collector_t* c = ctx_->collector;
    for (pn_event_t *e = pn_collector_peek(c); e; e = pn_collector_peek(c)) {
        if (pn_event_type(e) == PN_CONNECTION_INIT) {
            // Make the messaging_adapter issue a START event.
            proton_event(e, PN_REACTOR_INIT, 0).dispatch(h);
        }
        proton_event(e, pn_event_type(e), 0).dispatch(h);
        pn_collector_pop(c);
    }
}

size_t connection_engine::can_read() const {
    return std::max(ssize_t(0), pn_transport_capacity(ctx_->transport));
}

void connection_engine::try_read() {
    size_t max = can_read();
    if (max == 0) return;
    try {
        std::pair<size_t, bool> r = io_read(pn_transport_tail(ctx_->transport), max);
        if (r.second) {
            if (r.first > max)
                throw io_error(msg() << "read invalid size: " << r.first << ">" << max);
            pn_transport_process(ctx_->transport, r.first);
        } else {
            pn_transport_close_tail(ctx_->transport);
        }
    } catch (const io_error& e) {
        set_error(ctx_, e.what());
        pn_transport_close_tail(ctx_->transport);
    }
}

size_t connection_engine::can_write() const {
    return std::max(ssize_t(0), pn_transport_pending(ctx_->transport));
}

void connection_engine::try_write() {
    size_t max = can_write();
    if (max == 0) return;
    try {
        size_t n = io_write(pn_transport_head(ctx_->transport), max);
        if (n > max) {
            throw io_error(msg() << "write invalid size: " << n << " > " << max);
        }
        pn_transport_pop(ctx_->transport, n);
    } catch (const io_error& e) {
        set_error(ctx_, e.what());
        pn_transport_close_head(ctx_->transport);
    }
}

bool connection_engine::closed() const {
    return pn_transport_closed(ctx_->transport);
}

connection connection_engine::connection() const { return connection_.pn_object(); }

const connection_options connection_engine::no_opts;

}
