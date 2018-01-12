#ifndef PROTON_LISTEN_HANDLER_HPP
#define PROTON_LISTEN_HANDLER_HPP

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
#include <string>

/// @file
/// @copybrief proton::listen_handler

namespace proton {

// XXX Discuss more
/// **Unsettled API** - A handler for incoming connections.
///
/// Implement this interface and pass to proton::container::listen()
/// to be notified of new connections.
class PN_CPP_CLASS_EXTERN listen_handler {
  public:
    PN_CPP_EXTERN virtual ~listen_handler();

    /// Called when the listener is opened successfully.
    PN_CPP_EXTERN virtual void on_open(listener&);

    /// Called for each accepted connection.
    ///
    /// Returns connection_options to apply, including a proton::messaging_handler for
    /// the connection.  messaging_handler::on_connection_open() will be called with
    /// the proton::connection, it can call connection::open() to accept or
    /// connection::close() to reject the connection.
    PN_CPP_EXTERN virtual connection_options on_accept(listener&);

    /// Called if there is a listening error, with an error message.
    /// close() will also be called.
    PN_CPP_EXTERN virtual void on_error(listener&, const std::string&);

    /// Called when this listen_handler is no longer needed, and can be deleted.
    PN_CPP_EXTERN virtual void on_close(listener&);
};

} // proton

#endif // PROTON_LISTEN_HANDLER_HPP
