#ifndef CONNECTION_ENGINE_HPP
#define CONNECTION_ENGINE_HPP

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

#include "proton/condition.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/error.hpp"
#include "proton/export.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/transport.hpp"
#include "proton/types.hpp"

#include <cstddef>
#include <utility>
#include <string>

struct pn_collector_t;

namespace proton {

class handler;

/// Contains classes to integrate proton into different IO and threading environments.
namespace io {

///@cond INTERNAL
class connection_engine_context;
///

/// Pointer to a mutable memory region with a size.
struct mutable_buffer {
    char* data;
    size_t size;

    mutable_buffer(char* data_=0, size_t size_=0) : data(data_), size(size_) {}
};

/// Pointer to a const memory region with a size.
struct const_buffer {
    const char* data;
    size_t size;

    const_buffer(const char* data_=0, size_t size_=0) : data(data_), size(size_) {}
};

/// A protocol engine to integrate AMQP into any IO or concurrency framework.
///
/// io::connection_engine manages a single proton::connection and dispatches
/// events to a proton::handler. It does no IO of its own, but allows you to
/// integrate AMQP protocol handling into any IO or concurrency framework.
///
/// The application is coded the same way as for the proton::container. The
/// application implements a proton::handler to respond to transport,
/// connection, session, link and message events. With a little care, the same
/// handler classes can be used for both container and connection_engine, the
/// \ref broker.cpp example illustrates this.
///
/// You need to write the IO code to read AMQP data to the read_buffer(). The
/// engine parses the AMQP frames. dispatch() calls the appropriate functions on
/// the applications proton::handler. You write output data from the engines
/// write_buffer() to your IO.
///
/// The engine is not safe for concurrent use, but you can process different
/// engines concurrently. A common pattern for high-performance servers is to
/// serialize read/write activity per-connection and dispatch in a fixed-size
/// thread pool.
///
/// The engine is designed to work with a classic reactor (e.g. select, poll,
/// epoll) or an async-request driven proactor (e.g. windows completion ports,
/// boost.asio, libuv etc.)
///
class
PN_CPP_CLASS_EXTERN connection_engine {
  public:
    // TODO aconway 2016-03-18: this will change
    class container {
      public:
        /// Create a container with id.  Default to random UUID.
        PN_CPP_EXTERN container(const std::string &id = "");
        PN_CPP_EXTERN ~container();

        /// Return the container-id
        PN_CPP_EXTERN std::string id() const;

        /// Make options to configure a new engine, using the default options.
        ///
        /// Call this once for each new engine as the options include a generated unique link_prefix.
        /// You can modify the configuration before creating the engine but you should not
        /// modify the container_id or link_prefix.
        PN_CPP_EXTERN connection_options make_options();

        /// Set the default options to be used for connection engines.
        /// The container will set the container_id and link_prefix when make_options is called.
        PN_CPP_EXTERN void options(const connection_options&);

      private:
        class impl;
        internal::pn_unique_ptr<impl> impl_;
    };

    /// Create a connection engine that dispatches to handler.
    PN_CPP_EXTERN connection_engine(handler&, const connection_options& = connection_options());

    PN_CPP_EXTERN virtual ~connection_engine();

    /// The engine's read buffer. Read data into this buffer then call read_done() when complete.
    /// Returns mutable_buffer(0, 0) if the engine cannot currently read data.
    /// Calling dispatch() may open up more buffer space.
    PN_CPP_EXTERN mutable_buffer read_buffer();

    /// Indicate that the first n bytes of read_buffer() have valid data.
    /// This changes the buffer, call read_buffer() to get the updated buffer.
    PN_CPP_EXTERN void read_done(size_t n);

    /// Indicate that the read side of the transport is closed and no more data will be read.
    PN_CPP_EXTERN void read_close();

    /// The engine's write buffer. Write data from this buffer then call write_done()
    /// Returns const_buffer(0, 0) if the engine has nothing to write.
    /// Calling dispatch() may generate more data in the write buffer.
    PN_CPP_EXTERN const_buffer write_buffer() const;

    /// Indicate that the first n bytes of write_buffer() have been written successfully.
    /// This changes the buffer, call write_buffer() to get the updated buffer.
    PN_CPP_EXTERN void write_done(size_t n);

    /// Indicate that the write side of the transport has closed and no more data will be written.
    PN_CPP_EXTERN void write_close();

    /// Indicate that the transport has closed with an error condition.
    /// This calls both read_close() and write_close().
    /// The error condition will be passed to handler::on_transport_error()
    PN_CPP_EXTERN void close(const std::string& name, const std::string& description);

    /// Dispatch all available events and call the corresponding \ref handler methods.
    ///
    /// Returns true if the engine is still active, false if it is finished and
    /// can be destroyed. The engine is finished when either of the following is
    /// true:
    ///
    /// - both read_close() and write_close() have been called, no more IO is possible.
    /// - The AMQP connection() is closed AND write_buffer() is empty.
    ///
    /// May expand the read_buffer() and/or the write_buffer().
    ///
    /// @throw any exceptions thrown by the \ref handler.
    PN_CPP_EXTERN bool dispatch();

    /// Get the AMQP connection associated with this connection_engine.
    PN_CPP_EXTERN proton::connection connection() const;

  private:
    connection_engine(const connection_engine&);
    connection_engine& operator=(const connection_engine&);

    proton::handler& handler_;
    proton::connection connection_;
    proton::transport transport_;
    proton::internal::pn_ptr<pn_collector_t> collector_;
};
}}

#endif // CONNECTION_ENGINE_HPP
