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

#include "./internal/export.hpp"
#include "./endpoint.hpp"
#include "./internal/object.hpp"
#include "./session.hpp"

#include <proton/types.h>

#include <string>

struct pn_connection_t;

namespace proton {

class messaging_handler;
class connection_options;
class sender;
class sender_options;
class receiver;
class receiver_options;
class container;
template <class T> class thread_safe;

/// A connection to a remote AMQP peer.
class
PN_CPP_CLASS_EXTERN connection : public internal::object<pn_connection_t>, public endpoint {
    /// @cond INTERNAL
    PN_CPP_EXTERN connection(pn_connection_t* c) : internal::object<pn_connection_t>(c) {}
    /// @endcond

  public:
    /// Create an empty connection.
    connection() : internal::object<pn_connection_t>(0) {}

    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    /// Get the container.
    ///
    /// @throw proton::error if this connection is not managed by a
    /// container
    PN_CPP_EXTERN class container &container() const;

    /// Get the transport for the connection.
    PN_CPP_EXTERN class transport transport() const;

    /// Return the AMQP hostname attribute for the connection.
    PN_CPP_EXTERN std::string virtual_host() const;

    /// Return the container ID for the connection.
    PN_CPP_EXTERN std::string container_id() const;

    /// Return authenticated user for the connection
    /// Note: The value returned is not stable until the on_transport_open event is received
    PN_CPP_EXTERN std::string user() const;

    /// Open the connection.
    ///
    /// @see endpoint_lifecycle
    PN_CPP_EXTERN void open();

    /// @copydoc open
    PN_CPP_EXTERN void open(const connection_options &);

    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN void close(const error_condition&);

    /// Open a new session.
    PN_CPP_EXTERN session open_session();

    /// @copydoc open_session
    PN_CPP_EXTERN session open_session(const session_options &);

    /// Get the default session.  A default session is created on the
    /// first call and reused for the lifetime of the connection.
    PN_CPP_EXTERN session default_session();

    /// Open a sender for `addr` on default_session().
    PN_CPP_EXTERN sender open_sender(const std::string &addr);

    /// @copydoc open_sender
    PN_CPP_EXTERN sender open_sender(const std::string &addr, const sender_options &);

    /// Open a receiver for `addr` on default_session().
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr);

    /// @copydoc open_receiver
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr,
                                         const receiver_options &);

    /// Return all sessions on this connection.
    PN_CPP_EXTERN session_range sessions() const;

    /// Return all receivers on this connection.
    PN_CPP_EXTERN receiver_range receivers() const;

    /// Return all senders on this connection.
    PN_CPP_EXTERN sender_range senders() const;

    /// Get the maximum frame size.
    ///
    /// @see @ref connection_options::max_frame_size
    PN_CPP_EXTERN uint32_t max_frame_size() const;

    /// Get the maximum number of open sessions.
    ///
    /// @see @ref connection_options::max_sessions
    PN_CPP_EXTERN uint16_t max_sessions() const;

    /// Get the idle timeout.
    ///
    /// @see @ref connection_options::idle_timeout
    PN_CPP_EXTERN uint32_t idle_timeout() const;

    /// @cond INTERNAL
  friend class internal::factory<connection>;
  friend class connector;
  friend class proton::thread_safe<connection>;
    /// @endcond
};

} // proton

#endif // PROTON_CONNECTION_HPP
