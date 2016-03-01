#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

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

#include <proton/connection_engine.hpp>
#include <proton/url.hpp>

namespace proton {

/// IO using sockets, file descriptors, or handles, for use with
/// proton::connection_engine.
///
/// Note that you can use proton::connection_engine to communicate using AMQP
/// over your own IO implementation or to integrate an existing IO framework of
/// your choice, this implementation is provided as a convenience if sockets is
/// sufficient for your needs.

namespace io {

/// @name Setup and teardown
///
/// Call proton::io::initialize before using any functions in the
/// proton::io namespace.  Call proton::io::finalize when you are
/// done.
///
/// You can call initialize/finalize more than once as long as they
/// are in matching pairs. Use proton::io::guard to call
/// initialize/finalize around a scope.
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

/// A connection_engine for socket-based IO.
class socket_engine : public connection_engine {
  public:
    /// Wrap an open socket. Sets non-blocking mode.
    PN_CPP_EXTERN socket_engine(descriptor socket_, handler&, const connection_options& = no_opts);

    /// Create socket engine connected to url.
    PN_CPP_EXTERN socket_engine(const url&, handler&, const connection_options& = no_opts);

    PN_CPP_EXTERN ~socket_engine();

    /// Get the socket descriptor.
    descriptor socket() const { return socket_; }

    /// Start the engine.
    PN_CPP_EXTERN void run();

  protected:
    PN_CPP_EXTERN std::pair<size_t, bool> io_read(char* buf, size_t max);
    PN_CPP_EXTERN size_t io_write(const char*, size_t);
    PN_CPP_EXTERN void io_close();

  private:
    void init();
    guard guard_;
    descriptor socket_;
};

}} // proton::io

#endif // SOCKET_IO_HPP
