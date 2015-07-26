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
#include "proton/container.hpp"
#include "proton/handler.hpp"
#include "proton/error.hpp"
#include "connection_impl.hpp"
#include "proton/transport.hpp"
#include "msg.hpp"
#include "contexts.hpp"
#include "private_impl_ref.hpp"
#include "container_impl.hpp"

#include "proton/connection.h"

namespace proton {

void connection_impl::incref(connection_impl *impl_) {
    impl_->refcount_++;
}

void connection_impl::decref(connection_impl *impl_) {
    impl_->refcount_--;
    if (impl_->refcount_ == 0)
        delete impl_;
}

connection_impl::connection_impl(class container &c, pn_connection_t &pn_conn)
    : container_(c), refcount_(0), override_(0), transport_(0), default_session_(0),
      pn_connection_(&pn_conn), reactor_reference_(this)
{
    connection_context(pn_connection_, this);
}

connection_impl::connection_impl(class container &c, handler *handler)
    : container_(c), refcount_(0), override_(0), transport_(0), default_session_(0),
      reactor_reference_(this)
{
    pn_handler_t *chandler = 0;
    if (handler) {
        container_impl *container_impl = private_impl_ref<class container>::get(c);
        chandler = container_impl->wrap_handler(handler);
    }
    pn_connection_ = pn_reactor_connection(container_.reactor(), chandler);
    if (chandler)
        pn_decref(chandler);
    connection_context(pn_connection_, this);
}

connection_impl::~connection_impl() {
    delete transport_;
    delete override_;
}

transport &connection_impl::transport() {
    if (transport_)
        return *transport_;
    throw error(MSG("connection has no transport"));
}

handler* connection_impl::override() { return override_; }
void connection_impl::override(handler *h) {
    if (override_)
        delete override_;
    override_ = h;
}

void connection_impl::open() {
    pn_connection_open(pn_connection_);
}

void connection_impl::close() {
    pn_connection_close(pn_connection_);
}

pn_connection_t *connection_impl::pn_connection() { return pn_connection_; }

std::string connection_impl::hostname() {
    return std::string(pn_connection_get_hostname(pn_connection_));
}

connection &connection_impl::connection() {
    // endpoint interface.  Should be implemented in the connection object.
    throw error(MSG("Internal error"));
}

container &connection_impl::container() { return (container_); }


// TODO: Rework this.  Rename and document excellently for user handlers on connections.
// Better: provide general solution for handlers that delete before the C reactor object
// has finished sending events.
void connection_impl::reactor_detach() {
    // "save" goes out of scope last, preventing possible recursive destructor
    // confusion with reactor_reference.
    class connection save(reactor_reference_);
    if (reactor_reference_)
        reactor_reference_ = proton::connection();
    pn_connection_ = 0;
}

connection &connection_impl::reactor_reference(pn_connection_t *conn) {
    if (!conn)
        throw error(MSG("amqp_null Proton connection"));
    connection_impl *impl_ = connection_context(conn);
    if (!impl_) {
        // First time we have seen this connection
        pn_reactor_t *reactor = pn_object_reactor(conn);
        if (!reactor)
            throw error(MSG("Invalid Proton connection specifier"));
        class container container(container_context(reactor));
        if (!container)  // can't be one created by our container
            throw error(MSG("Unknown Proton connection specifier"));
        impl_ = new connection_impl(container, *conn);
    }
    return impl_->reactor_reference_;
}

link connection_impl::link_head(endpoint::state mask) {
    return link(pn_link_head(pn_connection_, mask));
}

}
