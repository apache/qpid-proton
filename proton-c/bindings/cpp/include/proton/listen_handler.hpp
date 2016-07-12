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

namespace proton {

// XXX Discuss more
/// **Experimental** - A handler for incoming connections.
///
/// Implement this interface and pass to proton::container::listen()
/// to be notified of new connections.
class listen_handler {
  public:
    virtual ~listen_handler() {}

    /// Called for each accepted connection.
    ///
    /// Returns connection_options to apply, including a proton::messaging_handler for
    /// the connection.  messaging_handler::on_connection_open() will be called with
    /// the proton::connection, it can call connection::open() to accept or
    /// connection::close() to reject the connection.
    virtual connection_options on_accept()= 0;

    /// Called if there is a listening error, with an error message.
    /// close() will also be called.
    virtual void on_error(const std::string&) {}

    /// Called when this listen_handler is no longer needed, and can be deleted.
    virtual void on_close() {}
};

} // proton

#endif // PROTON_LISTEN_HANDLER_HPP
