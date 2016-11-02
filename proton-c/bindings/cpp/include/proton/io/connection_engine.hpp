#ifndef PROTON_IO_CONNECTION_ENGINE_HPP
#define PROTON_IO_CONNECTION_ENGINE_HPP

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

#include "../internal/config.hpp"
#include "../connection.hpp"
#include "../connection_options.hpp"
#include "../error.hpp"
#include "../error_condition.hpp"
#include "../internal/export.hpp"
#include "../internal/pn_unique_ptr.hpp"
#include "../transport.hpp"
#include "../types.hpp"

#include <proton/connection_engine.h>

#include <cstddef>
#include <utility>
#include <string>

namespace proton {

class event_loop;
class proton_handler;

namespace io {

/// **Experimental** - Pointer to a mutable memory region with a size.
struct mutable_buffer {
    char* data;                 ///< Beginning of the buffered data.
    size_t size;                ///< Number of bytes in the buffer.

    /// Construct a buffer starting at data_ with size_ bytes.
    mutable_buffer(char* data_=0, size_t size_=0) : data(data_), size(size_) {}
};

/// **Experimental** - Pointer to a const memory region with a size.
struct const_buffer {
    const char* data;           ///< Beginning of the buffered data.
    size_t size;                ///< Number of bytes in the buffer.

    /// Construct a buffer starting at data_ with size_ bytes.
    const_buffer(const char* data_=0, size_t size_=0) : data(data_), size(size_) {}
};

/// **Experimental** - An AMQP protocol engine for a single
/// connection.
///
/// A connection_engine is a protocol engine that integrates AMQP into
/// any IO or concurrency framework.
///
/// io::connection_engine manages a single proton::connection and dispatches
/// events to a proton::messaging_handler. It does no IO of its own, but allows you to
/// integrate AMQP protocol handling into any IO or concurrency framework.
///
/// The application is coded the same way as for the
/// proton::container. The application implements a
/// proton::messaging_handler to respond to transport, connection,
/// session, link, and message events. With a little care, the same
/// handler classes can be used for both container and
/// connection_engine. the @ref broker.cpp example illustrates this.
///
/// You need to write the IO code to read AMQP data to the
/// read_buffer(). The engine parses the AMQP frames. dispatch() calls
/// the appropriate functions on the applications proton::messaging_handler. You
/// write output data from the engine's write_buffer() to your IO.
///
/// The engine is not safe for concurrent use, but you can process
/// different engines concurrently. A common pattern for
/// high-performance servers is to serialize read/write activity
/// per connection and dispatch in a fixed-size thread pool.
///
/// The engine is designed to work with a classic reactor (e.g.,
/// select, poll, epoll) or an async-request driven proactor (e.g.,
/// windows completion ports, boost.asio, libuv).
///
/// The engine never throws exceptions.
class
PN_CPP_CLASS_EXTERN connection_engine {
  public:
    /// An engine that is not associated with a proton::container or
    /// proton::event_loop.
    ///
    /// Accessing the container or event_loop for this connection in
    /// a proton::messaging_handler will throw a proton::error exception.
    ///
    PN_CPP_EXTERN connection_engine();

    /// Create a connection engine associated with a proton::container and
    /// optional event_loop. If the event_loop is not provided attempts to use
    /// it will throw proton::error.
    ///
    /// Takes ownership of the event_loop. Note the proton::connection created
    /// by this connection_engine can outlive the connection_engine itself if
    /// the user pins it in memory using the proton::thread_safe<> template.
    /// The event_loop is deleted when, and only when, the proton::connection is.
    ///
    PN_CPP_EXTERN connection_engine(proton::container&, event_loop* loop = 0);

    PN_CPP_EXTERN ~connection_engine();

    /// Configure a connection by applying exactly the options in opts (including proton::messaging_handler)
    /// Does not apply any default options, to apply container defaults use connect() or accept()
    /// instead. If server==true, configure a server connection.
    void configure(const connection_options& opts=connection_options(), bool server=false);

    /// Call configure() with client options and call connection::open()
    /// Options applied: container::id(), container::client_connection_options(), opts.
    PN_CPP_EXTERN void connect(const connection_options& opts);

    /// Call configure() with server options.
    /// Options applied: container::id(), container::server_connection_options(), opts.
    ///
    /// Note this does not call connection::open(). If there is a messaging_handler in the
    /// composed options it will receive messaging_handler::on_connection_open() and can
    /// respond with connection::open() or connection::close()
    PN_CPP_EXTERN void accept(const connection_options& opts);

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
    PN_CPP_EXTERN const_buffer write_buffer();

    /// Indicate that the first n bytes of write_buffer() have been written successfully.
    /// This changes the buffer, call write_buffer() to get the updated buffer.
    PN_CPP_EXTERN void write_done(size_t n);

    /// Indicate that the write side of the transport has closed and no more data can be written.
    /// Note that there may still be events to dispatch() or data to read.
    PN_CPP_EXTERN void write_close();

    /// Inform the engine that the transport been disconnected unexpectedly,
    /// without completing the AMQP connection close sequence.
    ///
    /// This calls read_close(), write_close(), sets the transport().error() and
    /// queues an `on_transport_error` event. You must call dispatch() one more
    /// time to dispatch the messaging_handler::on_transport_error() call and other final
    /// events.
    ///
    /// Note this does not close the connection() so that a proton::messaging_handler can
    /// distinguish between a connection close error sent by the remote peer and
    /// a transport failure.
    ///
    PN_CPP_EXTERN void disconnected(const error_condition& = error_condition());

    /// Dispatch all available events and call the corresponding \ref messaging_handler methods.
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
    /// The event_loop is availabe via proton::thread_safe<connection>(connection())
    PN_CPP_EXTERN proton::connection connection() const;

    /// Get the transport associated with this connection_engine.
    PN_CPP_EXTERN proton::transport transport() const;

    /// Get the container associated with this connection_engine, if there is one.
    PN_CPP_EXTERN proton::container* container() const;

 private:
    void init();
    connection_engine(const connection_engine&);
    connection_engine& operator=(const connection_engine&);

    messaging_handler* handler_;
    proton::container* container_;
    pn_connection_engine_t engine_;
};

} // io
} // proton

#endif // PROTON_IO_CONNECTION_ENGINE_HPP
