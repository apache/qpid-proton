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

#include "proton/config.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/error.hpp"
#include "proton/error_condition.hpp"
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
class work_queue;            // Only used for multi-threaded connection_engines.

/** @page integration

This namespace contains a low-level "Service Provider Interface" that can be
used to implement the proton API over any native or 3rd party IO library.

The io::connection_engine is the core engine that converts raw AMQP bytes read
from any IO source into proton::handler event calls, and generates AMQP
byte-encoded output that can be written to any IO destination.

The integration needs to implement two user-visible interfaces:
 - proton::controller lets the user initiate or listen for connections.
 - proton::work_queue lets the user serialize their own work with a connection.

 @see epoll_controller.cpp for an example of an integration.

[TODO controller doesn't belong in the mt namespace, a single-threaded
integration would need a controller too.]

*/
namespace io {

/// Pointer to a mutable memory region with a size.
struct mutable_buffer {
    char* data;                 ///< Beginning of the buffered data.
    size_t size;                ///< Number of bytes in the buffer.

    /// Construct a buffer starting at data_ with size_ bytes.
    mutable_buffer(char* data_=0, size_t size_=0) : data(data_), size(size_) {}
};

/// Pointer to a const memory region with a size.
struct const_buffer {
    const char* data;           ///< Beginning of the buffered data.
    size_t size;                ///< Number of bytes in the buffer.

    /// Construct a buffer starting at data_ with size_ bytes.
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
/// The engine never throws exceptions.
///
class
PN_CPP_CLASS_EXTERN connection_engine {
  public:
    /// Create a connection engine that dispatches to handler.
    // TODO aconway 2016-04-06: no options, only via handler.
    PN_CPP_EXTERN connection_engine(handler&, const connection_options& = connection_options());

    PN_CPP_EXTERN ~connection_engine();

    /// The engine's read buffer. Read data into this buffer then call read_done() when complete.
    /// Returns mutable_buffer(0, 0) if the engine cannot currently read data.
    /// Calling dispatch() may open up more buffer space.
    PN_CPP_EXTERN mutable_buffer read_buffer();

    /// Indicate that the first n bytes of read_buffer() have valid data.
    /// This changes the buffer, call read_buffer() to get the updated buffer.
    PN_CPP_EXTERN void read_done(size_t n);

    /// Indicate that the read side of the transport is closed and no more data will be read.
    /// Note that there may still be events to dispatch() or data to write.
    PN_CPP_EXTERN void read_close();

    /// The engine's write buffer. Write data from this buffer then call write_done()
    /// Returns const_buffer(0, 0) if the engine has nothing to write.
    /// Calling dispatch() may generate more data in the write buffer.
    PN_CPP_EXTERN const_buffer write_buffer() const;

    /// Indicate that the first n bytes of write_buffer() have been written successfully.
    /// This changes the buffer, call write_buffer() to get the updated buffer.

    PN_CPP_EXTERN void write_done(size_t n);

    /// Indicate that the write side of the transport has closed and no more data can be written.
    /// Note that there may still be events to dispatch() or data to read.
    PN_CPP_EXTERN void write_close();

    /// Close the engine with an error that will be passed to handler::on_transport_error().
    /// Calls read_close() and write_close().
    /// Note: You still need to call dispatch() to process final close-down events.
    PN_CPP_EXTERN void close(const error_condition&);

    /// Dispatch all available events and call the corresponding \ref handler methods.
    ///
    /// Returns true if the engine is still active, false if it is finished and
    /// can be destroyed. The engine is finished when all events are dispatched
    /// and one of the following is true:
    ///
    /// - both read_close() and write_close() have been called, no more IO is possible.
    /// - The AMQP connection() is closed AND the write_buffer() is empty.
    ///
    /// May modify the read_buffer() and/or the write_buffer().
    ///
    PN_CPP_EXTERN bool dispatch();

    /// Get the AMQP connection associated with this connection_engine.
    PN_CPP_EXTERN proton::connection connection() const;

    /// Get the transport associated with this connection_engine.
    PN_CPP_EXTERN proton::transport transport() const;

    /// For controller connections, set the connection's work_queue. Set
    /// via plain pointer, not std::shared_ptr so the connection_engine can be
    /// compiled with C++03.  The work_queue must outlive the engine. The
    /// std::shared_ptr<work_queue> will be available via work_queue::get(this->connection())
    PN_CPP_EXTERN void work_queue(class work_queue*);

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
