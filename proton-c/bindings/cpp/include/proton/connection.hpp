#ifndef PROTON_CPP_CONNECTION_H
#define PROTON_CPP_CONNECTION_H

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

#include "proton/export.hpp"
#include "proton/endpoint.hpp"
#include "proton/link.hpp"
#include "proton/object.hpp"
#include "proton/session.hpp"
#include "proton/connection_options.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

class handler;

namespace io {
class connection_engine;
}

/// A connection to a remote AMQP peer.
class
PN_CPP_CLASS_EXTERN connection : public internal::object<pn_connection_t>, public endpoint {
    /// @cond INTERNAL
    connection(pn_connection_t* c) : internal::object<pn_connection_t>(c) {}
    /// @endcond

  public:
    connection() : internal::object<pn_connection_t>(0) {}

    /// Get the state of this connection.
    PN_CPP_EXTERN endpoint::state state() const;

    PN_CPP_EXTERN condition local_condition() const;
    PN_CPP_EXTERN condition remote_condition() const;

    /// Get the container.
    ///
    /// @throw proton::error if this connection is not managed by a
    /// container
    PN_CPP_EXTERN class container &container() const;

    /// Get the transport for the connection.
    PN_CPP_EXTERN class transport transport() const;

    /// Return the AMQP host name for the connection.
    PN_CPP_EXTERN std::string host() const;

    /// Return the container ID for the connection.
    PN_CPP_EXTERN std::string container_id() const;

    /// @cond INTERNAL
    /// XXX connection options
    /// Initiate local open.  The operation is not complete till
    /// handler::on_connection_open().
    PN_CPP_EXTERN void open();
    /// @endcond

    /// Initiate local close.  The operation is not complete till
    /// handler::on_connection_close().
    PN_CPP_EXTERN void close();

    /// @cond INTERNAL
    /// XXX undiscussed
    /// Release link and session resources of this connection.
    PN_CPP_EXTERN void release();
    /// @endcond

    /// Open a new session.
    PN_CPP_EXTERN session open_session();

    /// Get the default session.  A default session is created on the
    /// first call and reused for the lifetime of the connection.
    PN_CPP_EXTERN session default_session();

    /// Open a sender for `addr` on default_session().
    PN_CPP_EXTERN sender open_sender(const std::string &addr,
                                     const link_options &opts = link_options());

    /// Open a receiver for `addr` on default_session().
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr,
                                         const link_options &opts = link_options());

    /// Return links on this connection matching the state mask.
    PN_CPP_EXTERN link_range links() const;

    /// Return sessions on this connection matching the state mask.
    PN_CPP_EXTERN session_range sessions() const;

    /// @cond INTERNAL
    ///
    /// XXX not yet discussed, why this convenience but not others?
    /// opened?  should this not be on endpoint?
    ///
    /// True if the connection is fully closed, i.e. local and remote
    /// ends are closed.
    bool closed() const { return (state() & LOCAL_CLOSED) && (state() & REMOTE_CLOSED); }
    /// @endcond

    /// @cond INTERNAL
  private:
    void user(const std::string &);
    void password(const std::string &);
    void host(const std::string& h);

    friend class connection_context;
    friend class io::connection_engine;
    friend class connection_options;
    friend class connector;
    friend class container_impl;
    friend class transport;
    friend class session;
    friend class link;
    friend class delivery;
    friend class reactor;
    friend class proton_event;
    friend class override_handler;
    friend class messaging_adapter;
    /// @endcond
};

}

#endif // PROTON_CPP_CONNECTION_H
