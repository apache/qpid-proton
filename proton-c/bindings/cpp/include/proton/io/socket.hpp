#ifndef PROTON_IO_IO_HPP
#define PROTON_IO_IO_HPP

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

#include <proton/io/connection_engine.hpp>
#include <proton/url.hpp>


namespace proton {
namespace io {
namespace socket {

struct
PN_CPP_CLASS_EXTERN io_error : public proton::error {
    PN_CPP_EXTERN explicit io_error(const std::string&); ///< Construct with message
};

/// @name Setup and teardown
///
/// Call initialize() before using any functions in the proton::io::socket
/// namespace.  Call finalize() when you are done.
///
/// You can call initialize/finalize more than once as long as they are in
/// matching pairs. Use \ref guard to call initialize/finalize around a scope.
///
/// Note that on POSIX systems these are no-ops, but they are required
/// for Windows.
///
/// @{

/// Initialize the proton::io subsystem.
PN_CPP_EXTERN void initialize();

/// Finalize the proton::io subsystem.
PN_CPP_EXTERN void finalize(); // nothrow

/// Use to call io::initialize and io::finalize around a scope.
struct guard {
    guard() { initialize(); }
    ~guard() { finalize(); }
};

/// @}

/// An IO resource.
typedef int64_t descriptor;

/// An invalid descriptor.
PN_CPP_EXTERN extern const descriptor INVALID_DESCRIPTOR;

/// Return a string describing the most recent IO error.
PN_CPP_EXTERN std::string error_str();

/// Open a TCP connection to the host:port (port can be a service name or number) from a proton::url.
PN_CPP_EXTERN descriptor connect(const proton::url&);

/// Listening socket.
class listener {
  public:
    /// Listen on host/port. Empty host means listen on all interfaces.
    /// port can be a service name or number
    PN_CPP_EXTERN listener(const std::string& host, const std::string& port);
    PN_CPP_EXTERN ~listener();

    /// Accept a connection. Return the descriptor, set host, port to the remote address.
    /// port can be a service name or number.
    PN_CPP_EXTERN descriptor accept(std::string& host, std::string& port);

    /// Accept a connection, does not provide address info.
    descriptor accept() { std::string dummy; return accept(dummy, dummy); }

    /// Convert to descriptor
    descriptor socket() const { return socket_; }

  private:
    guard guard_;
    listener(const listener&);
    listener& operator=(const listener&);
    descriptor socket_;
};

/// A \ref connection_engine with non-blocking socket IO.
class engine : public connection_engine {
  public:
    /// Wrap an open socket. Does not automatically open the connection.
    PN_CPP_EXTERN engine(descriptor socket_, handler&, const connection_options& = connection_options());

    /// Create socket engine connected to url, open the connection as a client.
    PN_CPP_EXTERN engine(const url&, handler&, const connection_options& = connection_options());

    PN_CPP_EXTERN ~engine();

    /// Run the engine until it closes
    PN_CPP_EXTERN void run();

    /// Non-blocking read from socket to engine
    PN_CPP_EXTERN void read();

    /// Non-blocking write from engine to socket
    PN_CPP_EXTERN void write();

    descriptor socket() const { return socket_; }

  private:
    void init();
    guard guard_;
    descriptor socket_;
};

}}}

#endif  /*!PROTON_IO_IO_HPP*/
