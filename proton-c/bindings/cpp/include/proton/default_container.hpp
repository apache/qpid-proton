#ifndef PROTON_DEFAULT_CONTAINER_HPP
#define PROTON_DEFAULT_CONTAINER_HPP

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

#include "./container.hpp"

namespace proton {

/// A single-threaded container.
///
/// @copydoc container
class PN_CPP_CLASS_EXTERN  default_container : public container {
  public:
    /// Create a default, single-threaded container with a messaging_handler.
    /// The messaging_handler will be called for all events on all connections in the container.
    ///
    /// Container ID should be unique within your system. If empty a random UUID is generated.
    PN_CPP_EXTERN explicit default_container(proton::messaging_handler& h, const std::string& id = "");

    /// Create a default, single-threaded container without a messaging_handler.
    ///
    /// Connections get their handlesr via proton::connection_options.
    /// Container-wide defaults are set with client_connection_options() and
    /// server_connection_options(). Per-connection options are set in connect()
    /// and proton_listen_handler::on_accept for the proton::listen_handler
    /// passed to listen()
    ///
    /// Container ID should be unique within your system. If empty a random UUID is generated.
    PN_CPP_EXTERN explicit default_container(const std::string& id = "");

    /// Wrap an existing container implementation as a default_container.
    /// Takes ownership of c.
    PN_CPP_EXTERN explicit default_container(container* c) : impl_(c) {}

    PN_CPP_EXTERN returned<connection> connect(const std::string& url, const connection_options &) PN_CPP_OVERRIDE;
    PN_CPP_EXTERN listener listen(const std::string& url, listen_handler& l) PN_CPP_OVERRIDE;
    using container::listen;

    /// @cond INTERNAL
    /// XXX Make private
    PN_CPP_EXTERN void stop_listening(const std::string& url) PN_CPP_OVERRIDE;
    /// @endcond

    PN_CPP_EXTERN void run() PN_CPP_OVERRIDE;
    PN_CPP_EXTERN void auto_stop(bool set) PN_CPP_OVERRIDE;

    PN_CPP_EXTERN void stop(const error_condition& err = error_condition()) PN_CPP_OVERRIDE;

    PN_CPP_EXTERN returned<sender> open_sender(
        const std::string &url,
        const proton::sender_options &o = proton::sender_options(),
        const connection_options &c = connection_options()) PN_CPP_OVERRIDE;

    PN_CPP_EXTERN returned<receiver> open_receiver(
        const std::string&url,
        const proton::receiver_options &o = proton::receiver_options(),
        const connection_options &c = connection_options()) PN_CPP_OVERRIDE;

    PN_CPP_EXTERN std::string id() const PN_CPP_OVERRIDE;

    PN_CPP_EXTERN void client_connection_options(const connection_options &o) PN_CPP_OVERRIDE;
    PN_CPP_EXTERN connection_options client_connection_options() const PN_CPP_OVERRIDE;

    PN_CPP_EXTERN void server_connection_options(const connection_options &o) PN_CPP_OVERRIDE;
    PN_CPP_EXTERN connection_options server_connection_options() const PN_CPP_OVERRIDE;

    PN_CPP_EXTERN void sender_options(const class sender_options &o) PN_CPP_OVERRIDE;
    PN_CPP_EXTERN class sender_options sender_options() const PN_CPP_OVERRIDE;

    PN_CPP_EXTERN void receiver_options(const class receiver_options & o) PN_CPP_OVERRIDE;
    PN_CPP_EXTERN class receiver_options receiver_options() const PN_CPP_OVERRIDE;

  private:
    internal::pn_unique_ptr<container> impl_;
};

} // proton

#endif // PROTON_DEFAULT_CONTAINER_HPP
