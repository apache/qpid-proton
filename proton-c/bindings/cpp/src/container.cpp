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
#include "proton/messaging_event.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/messaging_adapter.hpp"
#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton/url.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#include "container_impl.hpp"
#include "private_impl_ref.hpp"
#include "connector.hpp"
#include "contexts.hpp"
#include "proton/connection.h"
#include "proton/session.h"

namespace proton {

container::container() : impl_(new container_impl(*this, 0)) {}

container::container(messaging_handler &mhandler) :
    impl_(new container_impl(*this, &mhandler)) {}

container::~container() {}

connection& container::connect(const url &host, handler *h) {
    return impl_->connect(host, h);
}

pn_reactor_t *container::reactor() { return impl_->reactor(); }

std::string container::container_id() { return impl_->container_id(); }

duration container::timeout() { return impl_->timeout(); }
void container::timeout(duration timeout) { impl_->timeout(timeout); }


sender& container::create_sender(connection &connection, const std::string &addr, handler *h) {
    return impl_->create_sender(connection, addr, h);
}

sender& container::create_sender(const proton::url &url) {
    return impl_->create_sender(url);
}

receiver& container::create_receiver(connection &connection, const std::string &addr, bool dynamic, handler *h) {
    return impl_->create_receiver(connection, addr, dynamic, h);
}

receiver& container::create_receiver(const proton::url &url) {
    return impl_->create_receiver(url);
}

acceptor& container::listen(const proton::url &url) {
    return impl_->listen(url);
}


void container::run() { impl_->run(); }
void container::start() { impl_->start(); }
bool container::process() { return impl_->process(); }
void container::stop() { impl_->stop(); }
void container::wakeup() { impl_->wakeup(); }
bool container::is_quiesced() { return impl_->is_quiesced(); }
void container::yield() { impl_->yield(); }

}
