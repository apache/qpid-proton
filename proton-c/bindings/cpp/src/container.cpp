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

#include "proton/connection.hpp"
#include "proton/link_options.hpp"
#include "proton/session.hpp"
#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton/url.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/task.hpp"

#include "container_impl.hpp"
#include "connector.hpp"
#include "contexts.hpp"
#include "messaging_adapter.hpp"

#include "proton/connection.h"
#include "proton/session.h"

namespace proton {

//// Public container class.

container::container(const std::string& id) {
    impl_.reset(new container_impl(*this, 0, id));
}

container::container(handler &mhandler, const std::string& id) {
    impl_.reset(new container_impl(*this, mhandler.messaging_adapter_.get(), id));
}

container::~container() {}

connection container::connect(const url &host, const connection_options &opts) {
    return impl_->connect(host, opts);
}

reactor container::reactor() const { return impl_->reactor_; }

std::string container::id() const { return impl_->id_; }

void container::run() { impl_->reactor_.run(); }

sender container::open_sender(const proton::url &url, const proton::link_options &lo, const connection_options &co) {
    return impl_->open_sender(url, lo, co);
}

receiver container::open_receiver(const proton::url &url, const proton::link_options &lo, const connection_options &co) {
    return impl_->open_receiver(url, lo, co);
}

acceptor container::listen(const proton::url &url, const connection_options &opts) {
    return impl_->listen(url, opts);
}

task container::schedule(int delay, handler *h) { return impl_->schedule(delay, h ? h->messaging_adapter_.get() : 0); }

void container::client_connection_options(const connection_options &o) { impl_->client_connection_options(o); }

void container::server_connection_options(const connection_options &o) { impl_->server_connection_options(o); }

void container::link_options(const class link_options &o) { impl_->link_options(o); }

} // namespace proton
