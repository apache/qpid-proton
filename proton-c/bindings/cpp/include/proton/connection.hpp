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
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

class connection_context;
class handler;
class engine;

/** connection to a remote AMQP peer. */
class connection : public object<pn_connection_t>, endpoint
{
  public:
    connection(pn_connection_t* c=0) : object(c) {}

    /// Get the connection context object from the connection
    PN_CPP_EXTERN connection_context& context() const;

    /// Get the event_loop, can be a container or an engine.
    PN_CPP_EXTERN class event_loop &event_loop() const;

    /// Get the container, throw an exception if event_loop is not a container.
    PN_CPP_EXTERN class container &container() const;

    /// Get the engine, , throw an exception if event_loop is not an engine.
    PN_CPP_EXTERN class engine &engine() const;

    /// Return the AMQP host name for the connection.
    PN_CPP_EXTERN std::string host() const;

    /// Set the AMQP host name for the connection
    PN_CPP_EXTERN void host(const std::string& h);

    /// Return the container-ID for the connection. All connections have a container_id,
    /// even if they don't have a container event_loop.
    PN_CPP_EXTERN std::string container_id() const;

    // Set the container-ID for the connection
    PN_CPP_EXTERN void container_id(const std::string& id);

    /** Initiate local open, not complete till messaging_handler::on_connection_opened()
     * or proton_handler::on_connection_remote_open()
     */
    PN_CPP_EXTERN void open();

    /** Initiate local close, not complete till messaging_handler::on_connection_closed()
     * or proton_handler::on_connection_remote_close()
     */
    PN_CPP_EXTERN void close();

    /** Release link and session resources of this connection
     */
    PN_CPP_EXTERN void release();

    /** Create a new session */
    PN_CPP_EXTERN session open_session();

    /** Default session is created on first call and re-used for the lifetime of the connection */
    PN_CPP_EXTERN session default_session();

    /** Create a sender on default_session() with target=addr and optional handler h */
    PN_CPP_EXTERN sender open_sender(const std::string &addr, handler *h=0);

    /** Create a receiver on default_session() with target=addr and optional handler h */
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr, bool dynamic=false, handler *h=0);

    /** Return links on this connection matching the state mask. */
    PN_CPP_EXTERN link_range find_links(endpoint::state mask) const;

    /** Return sessions on this connection matching the state mask. */
    PN_CPP_EXTERN session_range find_sessions(endpoint::state mask) const;

    /** Get the endpoint state */
    PN_CPP_EXTERN endpoint::state state() const;
};

}

#endif  /*!PROTON_CPP_CONNECTION_H*/
