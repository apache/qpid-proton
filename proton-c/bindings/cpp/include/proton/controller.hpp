#ifndef PROTON_MT_HPP
#define PROTON_MT_HPP

/*
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
 */


#include <proton/handler.hpp>
#include <proton/connection_options.hpp>
#include <proton/error_condition.hpp>

#include <functional>
#include <memory>

namespace proton {

class connection;

/// The controller lets the application initiate and listen for connections, and
/// start/stop overall IO activity. A single controller can manage many
/// connections.
///
/// A controller associates a proton::handler with a proton::connection (the
/// AMQP protocol connection) and the corresponding proton::transport
/// (representing the underlying IO connection)
///
/// The first call to a proton::handler is always handler::on_transport_open(),
/// the last is handler::on_transport_close(). Handlers can be deleted after
/// handler::on_transport_close().
///
class controller {
  public:
    /// Create an instance of the default controller implementation.
    /// @param container_id set on connections for this controller.
    /// If empty, generate a random QUID default.
    PN_CPP_EXTERN static std::unique_ptr<controller> create();

    /// Get the controller associated with a connection.
    /// @throw proton::error if this is not a controller-managed connection.
    PN_CPP_EXTERN static controller& get(const proton::connection&);

    controller(const controller&) = delete;

    virtual ~controller() {}

    /// Start listening on address.
    ///
    /// @param address identifies a listening address.
    ///
    /// @param make_handler returns a handler for each accepted connection.
    /// handler::on_connection_open() is called with the incoming
    /// proton::connection.  The handler can accept by calling
    /// connection::open()) or reject by calling connection::close()).
    ///
    /// Calls to the factory for this address are serialized. Calls for separate
    /// addresses in separate calls to listen() may be concurrent.
    ///
    virtual void listen(
        const std::string& address,
        std::function<proton::handler*(const std::string&)> make_handler,
        const connection_options& = connection_options()) = 0;

    /// Stop listening on address, must match exactly with address string given to listen().
    virtual void stop_listening(const std::string& address) = 0;

    /// Connect to address.
    ///
    /// The handler will get a proton::handler::on_connection_open() call the
    /// connection is completed by the other end.
    virtual void connect(
        const std::string& address, proton::handler&,
        const connection_options& = connection_options()) = 0;

    /// Default options for all connections, e.g. container_id.
    virtual void options(const connection_options&) = 0;

    /// Default options for all connections, e.g. container_id.
    virtual connection_options options() = 0;

    /// Run the controller in this thread. Returns when the controller is
    /// stopped.  Multiple threads can call run()
    virtual void run() = 0;

    /// Stop the controller: abort open connections, run() will return in all threads.
    /// Handlers will receive on_transport_error() with the error_condition.
    virtual void stop(const error_condition& = error_condition()) = 0;

    /// The controller will stop (run() will return) when there are no open
    /// connections or listeners left.
    virtual void stop_on_idle() = 0;

    /// Wait till the controller is stopped.
    virtual void wait() = 0;

  protected:
    controller() {}
};

}


#endif // PROTON_MT_HPP
