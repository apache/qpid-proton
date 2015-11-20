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

#include "proton/engine.hpp"
#include "proton/error.hpp"

#include "uuid.hpp"
#include "proton_bits.hpp"
#include "messaging_event.hpp"

#include <proton/connection.h>
#include <proton/transport.h>
#include <proton/event.h>

namespace proton {

struct engine::impl {

    impl(class handler& h, pn_transport_t *t) :
        handler(h), transport(t), connection(pn_connection()), collector(pn_collector())
    {}

    ~impl() {
        pn_transport_free(transport);
        pn_connection_free(connection);
        pn_collector_free(collector);
    }

    void check(int err, const std::string& msg) {
        if (err)
            throw proton::error(msg + error_str(pn_transport_error(transport), err));
    }

    pn_event_t *peek() { return pn_collector_peek(collector); }
    void pop() { pn_collector_pop(collector); }

    class handler& handler;
    pn_transport_t *transport;
    pn_connection_t *connection;
    pn_collector_t * collector;
};

engine::engine(handler &h, const std::string& id_) : impl_(new impl(h, pn_transport())) {
    if (!impl_->transport || !impl_->connection || !impl_->collector) 
        throw error("engine setup failed");
    std::string id = id_.empty() ? uuid().str() : id_;
    pn_connection_set_container(impl_->connection, id.c_str());
    impl_->check(pn_transport_bind(impl_->transport, impl_->connection), "engine bind: ");
    pn_connection_collect(impl_->connection, impl_->collector);
}

engine::~engine() {}

buffer<char> engine::input() {
    ssize_t n = pn_transport_capacity(impl_->transport);
    if (n <= 0)
        return buffer<char>();
    return buffer<char>(pn_transport_tail(impl_->transport), n);
}

void engine::close_input() {
    pn_transport_close_tail(impl_->transport);
    run();
}

void engine::received(size_t n) {
    impl_->check(pn_transport_process(impl_->transport, n), "engine process: ");
    run();
}

void engine::run() {
    for (pn_event_t *e = impl_->peek(); e; e = impl_->peek()) {
        switch (pn_event_type(e)) {
          case PN_CONNECTION_REMOTE_CLOSE:
            pn_transport_close_tail(impl_->transport);
            break;
          case PN_CONNECTION_LOCAL_CLOSE:
            pn_transport_close_head(impl_->transport);
            break;
          default:
            break;
        }
        messaging_event mevent(e, pn_event_type(e), this);
        mevent.dispatch(impl_->handler);
        impl_->pop();
    }
}

buffer<const char> engine::output() {
    ssize_t n = pn_transport_pending(impl_->transport);
    if (n <= 0)
        return buffer<const char>();
    return buffer<const char>(pn_transport_head(impl_->transport), n);
}

void engine::sent(size_t n) {
    pn_transport_pop(impl_->transport, n);
    run();
}

void engine::close_output() {
    pn_transport_close_head(impl_->transport);
    run();
}

bool engine::closed() const {
    return pn_transport_closed(impl_->transport);
}

class connection engine::connection() const {
    return impl_->connection;
}

std::string engine::id() const { return connection().container_id(); }

}
