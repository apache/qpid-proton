#ifndef PROTON_CONNECTION_HPP
#define PROTON_CONNECTION_HPP

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

#include "./fwd.hpp"
#include "./internal/export.hpp"
#include "./internal/object.hpp"
#include "./endpoint.hpp"
#include "./session.hpp"
#include "./symbol.hpp"
#include "./value.hpp"

#include <proton/type_compat.h>

#include <map>
#include <string>

/// @file
/// @copybrief proton::connection

struct pn_connection_t;

namespace proton {

/// A connection to a remote AMQP peer.
class
PN_CPP_CLASS_EXTERN connection : public internal::object<pn_connection_t>, public endpoint {
    /// @cond INTERNAL
    PN_CPP_EXTERN connection(pn_connection_t* c) : internal::object<pn_connection_t>(c) {}
    /// @endcond

  public:
    /// Create an empty connection.
    connection() : internal::object<pn_connection_t>(0) {}

    PN_CPP_EXTERN ~connection();

    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    /// Get the container.
    ///
    /// @throw proton::error if this connection is not managed by a
    /// container
    PN_CPP_EXTERN class container& container() const;

    /// Get the work_queue for the connection.
    PN_CPP_EXTERN class work_queue& work_queue() const;

    /// Get the transport for the connection.
    PN_CPP_EXTERN class transport transport() const;

    /// Return the remote AMQP hostname attribute for the connection.
    PN_CPP_EXTERN std::string virtual_host() const;

    /// Return the remote container ID for the connection.
    PN_CPP_EXTERN std::string container_id() const;

    /// Return authenticated user for the connection
    /// Note: The value returned is not stable until the on_transport_open event is received
    PN_CPP_EXTERN std::string user() const;

    /// Open the connection.
    /// @see messaging_handler
    PN_CPP_EXTERN void open();

    /// @copydoc open
    PN_CPP_EXTERN void open(const connection_options&);

    /// Close the connection.
    /// @see messaging_handler
    PN_CPP_EXTERN void close();

    /// @copydoc close
    PN_CPP_EXTERN void close(const error_condition&);

    /// Open a new session.
    PN_CPP_EXTERN session open_session();

    /// @copydoc open_session
    PN_CPP_EXTERN session open_session(const session_options&);

    /// Get the default session.  A default session is created on the
    /// first call and reused for the lifetime of the connection.
    PN_CPP_EXTERN session default_session();

    /// Open a sender for `addr` on default_session().
    PN_CPP_EXTERN sender open_sender(const std::string& addr);

    /// @copydoc open_sender
    PN_CPP_EXTERN sender open_sender(const std::string& addr, const sender_options&);

    /// Open a receiver for `addr` on default_session().
    PN_CPP_EXTERN receiver open_receiver(const std::string& addr);

    /// @copydoc open_receiver
    PN_CPP_EXTERN receiver open_receiver(const std::string& addr,
                                         const receiver_options&);

    /// @see proton::container::sender_options()
    PN_CPP_EXTERN class sender_options sender_options() const;

    /// @see container::receiver_options()
    PN_CPP_EXTERN class receiver_options receiver_options() const;

    /// Return all sessions on this connection.
    PN_CPP_EXTERN session_range sessions() const;

    /// Return all receivers on this connection.
    PN_CPP_EXTERN receiver_range receivers() const;

    /// Return all senders on this connection.
    PN_CPP_EXTERN sender_range senders() const;

    /// Get the maximum frame size allowed by the remote peer.
    ///
    /// @see @ref connection_options::max_frame_size
    PN_CPP_EXTERN uint32_t max_frame_size() const;

    /// Get the maximum number of open sessions allowed by the remote
    /// peer.
    ///
    /// @see @ref connection_options::max_sessions
    PN_CPP_EXTERN uint16_t max_sessions() const;

    /// **Unsettled API** - Extension capabilities offered by the remote peer.
    PN_CPP_EXTERN std::vector<symbol> offered_capabilities() const;

    /// **Unsettled API** - Extension capabilities desired by the remote peer.
    PN_CPP_EXTERN std::vector<symbol> desired_capabilities() const;

    /// **Unsettled API** - Connection properties
    PN_CPP_EXTERN std::map<symbol, value> properties() const;

    /// Get the idle timeout set by the remote peer.
    ///
    /// @see @ref connection_options::idle_timeout
    PN_CPP_EXTERN uint32_t idle_timeout() const;

    /// **Unsettled API** - Trigger an event from another thread.
    ///
    /// This method can be called from any thread. The Proton library
    /// will call `messaging_handler::on_connection_wake()` as soon as
    /// possible in the correct event-handling thread.
    ///
    /// **Thread-safety** - This is the *only* `proton::connection`
    /// function that can be called from outside the handler thread.
    ///
    /// @note Spurious `messaging_handler::on_connection_wake()` calls
    /// can occur even if the application does not call `wake()`.
    ///
    /// @note Multiple calls to `wake()` may be coalesced into a
    /// single call to `messaging_handler::on_connection_wake()` that
    /// occurs after all of them.
    ///
    /// The `proton::work_queue` interface provides an easier way
    /// execute code safely in the event-handler thread.
    PN_CPP_EXTERN void wake() const;

    /// **Unsettled API** - True if this connection has been automatically
    /// re-connected.
    ///
    /// @see reconnect_options, messaging_handler
    PN_CPP_EXTERN bool reconnected() const;

    /// **Unsettled API** - Update the connection options for this connection
    ///
    /// This method can be used to update the connection options used with this
    /// connection. Usually the connection options are only used during the initial
    /// connection attempt so this ability is only useful when automatically
    /// reconnect is enabled and you wish to change the connection options before
    /// the next reconnect attempt. This would usually be in the handler for the
    /// `on_transport_error` event @see messaging_handler.
    ///
    /// @note Connection options supplied in the parameter will be merged with the
    /// existing parameters as if `connection_options::update()` was used.
    PN_CPP_EXTERN void update_options(const connection_options&);

    /// @cond INTERNAL
  friend class internal::factory<connection>;
  friend class container;
    /// @endcond
};

} // proton

#endif // PROTON_CONNECTION_HPP
