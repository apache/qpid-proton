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
#include "proton/error_condition.hpp"

#include "proton/error_condition.hpp"
#include "proton/listen_handler.hpp"
#include "proton/listener.hpp"
#include "proton/uuid.hpp"

#include "proactor_container_impl.hpp"

namespace proton {

container::container(messaging_handler& h, const std::string& id) :
    impl_(new impl(*this, id, &h)) {}
container::container(messaging_handler& h) :
    impl_(new impl(*this, uuid::random().str(), &h)) {}
container::container(const std::string& id) :
    impl_(new impl(*this, id)) {}
container::container() :
    impl_(new impl(*this, uuid::random().str())) {}
container::~container() {}

returned<connection> container::connect(const std::string &url) {
    return connect(url, connection_options());
}

returned<sender> container::open_sender(const std::string &url) {
    return open_sender(url, proton::sender_options(), connection_options());
}

returned<sender> container::open_sender(const std::string &url, const proton::sender_options &lo) {
    return open_sender(url, lo, connection_options());
}

returned<sender> container::open_sender(const std::string &url, const proton::connection_options &co) {
    return open_sender(url, sender_options(), co);
}

returned<receiver> container::open_receiver(const std::string &url) {
    return open_receiver(url, proton::receiver_options(), connection_options());
}

returned<receiver> container::open_receiver(const std::string &url, const proton::receiver_options &lo) {
    return open_receiver(url, lo, connection_options());
}

returned<receiver> container::open_receiver(const std::string &url, const proton::connection_options &co) {
    return open_receiver(url, receiver_options(), co);
}

listener container::listen(const std::string& url, const connection_options& opts) {
    return impl_->listen(url, opts);
}

listener container::listen(const std::string &url) {
    return impl_->listen(url);
}

void container::stop() { stop(error_condition()); }

returned<connection> container::connect(const std::string& url, const connection_options& opts) {
    return impl_->connect(url, opts);
}

returned<connection> container::connect() {
    return impl_->connect();
}

listener container::listen(const std::string& url, listen_handler& l) { return impl_->listen(url, l); }

void container::run() { impl_->run(1); }

#if PN_CPP_SUPPORTS_THREADS
void container::run(int threads) { impl_->run(threads); }
#endif

void container::auto_stop(bool set) { impl_->auto_stop(set); }

void container::stop(const error_condition& err) { impl_->stop(err); }

returned<sender> container::open_sender(
    const std::string &url,
    const class sender_options &o,
    const connection_options &c) {
    return impl_->open_sender(url, o, c);
}

returned<receiver> container::open_receiver(
    const std::string&url,
    const class receiver_options &o,
    const connection_options &c) {
    return impl_->open_receiver(url, o, c);
}

std::string container::id() const { return impl_->id(); }

void container::schedule(duration d, internal::v03::work f) { return impl_->schedule(d, f); }
#if PN_CPP_HAS_LAMBDAS && PN_CPP_HAS_VARIADIC_TEMPLATES
void container::schedule(duration d, internal::v11::work f) { return impl_->schedule(d, f); }
#endif

void container::schedule(duration d, void_function0& f) { return impl_->schedule(d, make_work(&void_function0::operator(), &f)); }

void container::client_connection_options(const connection_options& c) { impl_->client_connection_options(c); }
connection_options container::client_connection_options() const { return impl_->client_connection_options(); }

void container::server_connection_options(const connection_options &o) { impl_->server_connection_options(o); }
connection_options container::server_connection_options() const { return impl_->server_connection_options(); }

void container::sender_options(const class sender_options &o) { impl_->sender_options(o); }
class sender_options container::sender_options() const { return impl_->sender_options(); }

void container::receiver_options(const class receiver_options & o) { impl_->receiver_options(o); }
class receiver_options container::receiver_options() const { return impl_->receiver_options(); }

} // namespace proton
