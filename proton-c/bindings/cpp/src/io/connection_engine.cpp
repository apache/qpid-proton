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

namespace {
std::string  make_id(const std::string s="") {
    return s.empty() ? uuid::random().str() : s;
}
}

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

connection_engine::connection_engine(class handler &h, const connection_options& opts) :
    handler_(h),
    connection_(internal::take_ownership(pn_connection()).get()),
    transport_(internal::take_ownership(pn_transport()).get()),
    collector_(internal::take_ownership(pn_collector()).get())
{
    if (!connection_ || !transport_ || !collector_)
        throw proton::error("engine create");
    transport_.bind(connection_);
    pn_connection_collect(connection_.pn_object(), collector_.get());
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
    transport_.unbind();
    pn_collector_free(collector_.release()); // Break cycle with connection_
}

bool connection_engine::dispatch() {
    proton_handler& h = *handler_.messaging_adapter_;
    for (pn_event_t *e = pn_collector_peek(collector_.get());
         e;
         e = pn_collector_peek(collector_.get()))
    {
        proton_event(e, pn_event_type(e), 0).dispatch(h);
        pn_collector_pop(collector_.get());
    }
    return !(pn_transport_closed(transport_.pn_object()) ||
          (connection().closed() && write_buffer().size == 0));
}

mutable_buffer connection_engine::read_buffer() {
    ssize_t cap = pn_transport_capacity(transport_.pn_object());
    if (cap > 0)
        return mutable_buffer(pn_transport_tail(transport_.pn_object()), cap);
    else
        return mutable_buffer(0, 0);
}

void connection_engine::read_done(size_t n) {
    pn_transport_process(transport_.pn_object(), n);
}

void connection_engine::read_close() {
    pn_transport_close_tail(transport_.pn_object());
}

const_buffer connection_engine::write_buffer() const {
    ssize_t pending = pn_transport_pending(transport_.pn_object());
    if (pending > 0)
        return const_buffer(pn_transport_head(transport_.pn_object()), pending);
    else
        return const_buffer(0, 0);
}

void connection_engine::write_done(size_t n) {
    pn_transport_pop(transport_.pn_object(), n);
}

void connection_engine::write_close() {
    pn_transport_close_head(transport_.pn_object());
}

void connection_engine::close(const std::string& name, const std::string& description) {
    pn_condition_t* c = pn_transport_condition(transport_.pn_object());
    pn_condition_set_name(c, name.c_str());
    pn_condition_set_description(c, description.c_str());
    read_close();
    write_close();
}

proton::connection connection_engine::connection() const {
    return connection_;
}

}}
