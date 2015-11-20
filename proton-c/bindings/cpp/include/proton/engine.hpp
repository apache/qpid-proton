#ifndef ENGINE_HPP
#define ENGINE_HPP
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

#include "proton/pn_unique_ptr.hpp"
#include "proton/export.hpp"
#include "proton/event_loop.hpp"

#include <cstddef>
#include <utility>

namespace proton {

class handler;
class connection;

/// Pointers to a byte range to use as a buffer.
template <class T> class buffer {
  public:
    explicit buffer(T* begin__=0, T* end__=0) : begin_(begin__), end_(end__) {}
    explicit buffer(T* ptr, size_t n) : begin_(ptr), end_(ptr + n) {}
    T* begin() const { return begin_; }
    T* end() const { return end_; }
    size_t size() const { return end() - begin(); }
    bool empty() const { return !size(); }

 private:
    T* begin_;
    T* end_;
};

/**
 * An engine is an event_loop that manages a single AMQP connection.  It is
 * useful for integrating AMQP into an existing IO framework.
 *
 * The engine provides a simple "bytes-in/bytes-out" interface. Incoming AMQP
 * bytes from any kind of data connection are fed into the engine and processed
 * to dispatch events to a proton::handler.  The resulting AMQP output data is
 * available from the engine and can sent back over the connection.
 *
 * The engine does no IO of its own. It assumes a two-way flow of bytes over
 * some externally-managed "connection". The "connection" could be a socket
 * managed by select, poll, epoll or some other mechanism, or it could be
 * something else such as an RDMA connection, a shared-memory buffer or a Unix
 * pipe.
 *
 * The engine is an alternative event_loop to the proton::container. The
 * container is easier to use in single-threaded, stand-alone applications that
 * want to use standard socket connections.  The engine can be embedding into
 * any existing IO framework for any type of IO.
 *
 * The application is coded the same way for engine or container: you implement
 * proton::handler. Handlers attached to an engine will receive transport,
 * connection, session, link and message events. They will not receive reactor,
 * selectable or timer events, the engine assumes those are managed externally.
 *
 * THREAD SAFETY: A single engine instance cannot be called concurrently, but
 * different engine instances can be processed concurrently in separate threads.
 */
class engine : public event_loop {
  public:

    // TODO aconway 2015-11-02: engine() take connection-options.
    // TODO aconway 2015-11-02: engine needs to accept application events.
    // TODO aconway 2015-11-02: generalize reconnect logic for container and engine.

    /**
     * Create an engine that will advertise id as the AMQP container-id for its connection.
     */
    PN_CPP_EXTERN engine(handler&, const std::string& id=std::string());

    PN_CPP_EXTERN ~engine();

    /**
     * Input buffer. If input.size() == 0 means no input can be accepted right now, but
     * sending output might free up space.
     */
    PN_CPP_EXTERN buffer<char> input();

    /**
     * Process n bytes from input(). Calls handler functions for the AMQP events
     * encoded by the received data.
     *
     * After the call the input() and output() buffers may have changed.
     */
    PN_CPP_EXTERN void received(size_t n);

    /**
     * Indicate that no more input data is available from the external
     * connection. May call handler functions.
     */
    PN_CPP_EXTERN void close_input();

    /**
     * Output buffer. Send data from this buffer and call sent() to indicate it
     * has been sent. If output().size() == 0 there is no data to send,
     * receiving more input data might make output data available.
     */
    PN_CPP_EXTERN buffer<const char> output();

    /**
     * Call when the first n bytes from the start of the output buffer have been
     * sent.  May call handler functions.
     *
     * After the call the input() and output() buffers may have changed.
     */
    PN_CPP_EXTERN void sent(size_t n);

    /**
     * Indicate that no more output data can be sent to the external connection.
     * May call handler functions.
     */
    PN_CPP_EXTERN void close_output();

    /**
     * True if engine is closed. This can either be because close_input() and
     * close_output() were called to indicate the external connection has
     * closed, or because AMQP close frames were received in the AMQP data.  In
     * either case no more input() buffer space or output() data will be
     * available for this engine.
     */
    PN_CPP_EXTERN bool closed() const;

    /** The AMQP connection associated with this engine. */
    PN_CPP_EXTERN class connection  connection() const;

    /** The AMQP container-id associated with this engine. */
    PN_CPP_EXTERN  std::string id() const;

  private:
    void run();

    struct impl;
    pn_unique_ptr<impl> impl_;
};

}
#endif // ENGINE_HPP
