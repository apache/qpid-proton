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
#include "proton/container.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

class handler;
class transport;

/** connection to a remote AMQP peer. */
class connection : public counted_facade<pn_connection_t, connection, endpoint>
{
  public:
    ///@name getters @{
    PN_CPP_EXTERN class transport& transport() const;
    PN_CPP_EXTERN class container& container() const;
    PN_CPP_EXTERN std::string host() const;
    ///@}

    /** Initiate local open, not complete till messaging_handler::on_connection_opened()
     * or proton_handler::on_connection_remote_open()
     */
    PN_CPP_EXTERN void open();

    /** Initiate local close, not complete till messaging_handler::on_connection_closed()
     * or proton_handler::on_connection_remote_close()
     */
    PN_CPP_EXTERN void close();

    /** Create a new session */
    PN_CPP_EXTERN class session& open_session();

    /** Default session is created on first call and re-used for the lifetime of the connection */
    PN_CPP_EXTERN class session& default_session();

    /** Create a sender on default_session() with target=addr and optional handler h */
    PN_CPP_EXTERN sender& open_sender(const std::string &addr, handler *h=0);

    /** Create a receiver on default_session() with target=addr and optional handler h */
    PN_CPP_EXTERN receiver& open_receiver(const std::string &addr, bool dynamic=false, handler *h=0);

    /** Return links on this connection matching the state mask. */
    PN_CPP_EXTERN link_range find_links(endpoint::state mask) const;

    /** Return sessions on this connection matching the state mask. */
    PN_CPP_EXTERN session_range find_sessions(endpoint::state mask) const;

    /** Get the endpoint state */
    PN_CPP_EXTERN endpoint::state state() const;
};

}

#endif  /*!PROTON_CPP_CONNECTION_H*/
