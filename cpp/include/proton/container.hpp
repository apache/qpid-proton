#ifndef PROTON_CONTAINER_HPP
#define PROTON_CONTAINER_HPP

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
#include "./returned.hpp"
#include "./types_fwd.hpp"

#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"

#include <string>

/// @file
/// @copybrief proton::container

namespace proton {

/// A top-level container of connections, sessions, and links.
///
/// A container gives a unique identity to each communicating peer. It
/// is often a process-level object.
///
/// It also serves as an entry point to the API, allowing connections,
/// senders, and receivers to be established. It can be supplied with
/// an event handler in order to intercept important messaging events,
/// such as newly received messages or newly issued credit for sending
/// messages.
class PN_CPP_CLASS_EXTERN container {
  public:
    /// Create a container with a global handler for messaging events.
    ///
    /// **Thread safety** - in a multi-threaded container this handler will be
    /// called concurrently. You can use locks to make that safe, or use a
    /// separate handler for each connection.  See @ref mt_page.
    ///
    /// @param handler global handler, called for events on all connections
    /// managed by the container.
    ///
    /// @param id sets the container's unique identity.
    PN_CPP_EXTERN container(messaging_handler& handler, const std::string& id);

    /// Create a container with a global handler for messaging events.
    ///
    /// **Thread safety** - in a multi-threaded container this handler will be
    /// called concurrently. You can use locks to make that safe, or use a
    /// separate handler for each connection.  See @ref mt_page.
    ///
    /// @param handler global handler, called for events on all connections
    /// managed by the container.
    PN_CPP_EXTERN container(messaging_handler& handler);

    /// Create a container.
    /// @param id sets the container's unique identity.
    PN_CPP_EXTERN container(const std::string& id);

    /// Create a container.
    ///
    /// This will create a default random identity
    PN_CPP_EXTERN container();

    /// Destroy a container.
    ///
    /// A container must not be destroyed while a call to run() is in progress,
    /// in particular it must not be destroyed from a @ref messaging_handler
    /// callback.
    ///
    /// **Thread safety** - in a multi-threaded application, run() must return
    /// in all threads that call it before destroying the container.
    ///
    PN_CPP_EXTERN ~container();

    /// Connect to `conn_url` and send an open request to the remote
    /// peer.
    ///
    /// Options are applied to the connection as follows.
    ///
    ///  1. client_connection_options()
    ///  2. Options passed to connect()
    ///
    /// Values in later options override earlier ones.  The handler in
    /// the composed options is used to call
    /// `messaging_handler::on_connection_open()` when the open
    /// response is received from the remote peer.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<connection> connect(const std::string& conn_url,
                                               const connection_options& conn_opts);

    /// @copybrief connect()
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<connection> connect(const std::string& conn_url);

    /// Connect using the default @ref connect-config file.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<connection> connect();

    /// Listen for new connections on `listen_url`.
    ///
    /// If the listener opens successfully, listen_handler::on_open() is called.
    /// If it fails to open, listen_handler::on_error() then listen_handler::close()
    /// are called.
    ///
    /// listen_handler::on_accept() is called for each incoming connection to determine
    /// the @ref connection_options to use, including the @ref messaging_handler.
    ///
    /// **Thread safety** - Calls to `listen_handler` methods are serialized for
    /// this listener, but handlers attached to separate listeners may be called
    /// concurrently.
    PN_CPP_EXTERN listener listen(const std::string& listen_url,
                                  listen_handler& handler);

    /// @copybrief listen
    ///
    /// Use a fixed set of options for all accepted connections.  See
    /// listen(const std::string&, listen_handler&).
    ///
    /// **Thread safety** - for multi-threaded applications we recommend using a
    /// @ref listen_handler to create a new @ref messaging_handler for each connection.
    /// See listen(const std::string&, listen_handler&) and @ref mt_page
    PN_CPP_EXTERN listener listen(const std::string& listen_url,
                                  const connection_options& conn_opts);

    /// @copybrief listen
    ///
    /// New connections will use the handler from
    /// `server_connection_options()`. See listen(const std::string&, const
    /// connection_options&);
    PN_CPP_EXTERN listener listen(const std::string& listen_url);

    /// Run the container in the current thread.
    ///
    /// The call returns when the container stops. See `auto_stop()`
    /// and `stop()`.
    ///
    /// **C++ versions** - With C++11 or later, you can call `run()`
    /// in multiple threads to create a thread pool.  See also
    /// `run(int count)`.
    PN_CPP_EXTERN void run();

#if PN_CPP_SUPPORTS_THREADS
    /// Run the container with a pool of `count` threads, including the current thread.
    ///
    /// **C++ versions** - Available with C++11 or later.
    ///
    /// The call returns when the container stops. See `auto_stop()`
    /// and `stop()`.
    PN_CPP_EXTERN void run(int count);
#endif

    /// Enable or disable automatic container stop.  It is enabled by
    /// default.
    ///
    /// If true, the container stops when all active connections and
    /// listeners are closed.  If false, the container keeps running
    /// until `stop()` is called.
    PN_CPP_EXTERN void auto_stop(bool enabled);

    /// Stop the container with error condition `err`.
    ///
    /// @copydetails stop()
    PN_CPP_EXTERN void stop(const error_condition& err);

    /// Stop the container with an empty error condition.
    ///
    /// This function initiates shutdown and immediately returns.  The
    /// shutdown process has the following steps.
    ///
    ///  - Abort all open connections and listeners.
    ///  - Process final handler events and queued work.
    ///  - If `!err.empty()`, fire `messaging_handler::on_transport_error`.
    ///
    /// When the process is complete, `run()` returns in all threads.
    ///
    /// **Thread safety** - It is safe to call this method in any thread.
    PN_CPP_EXTERN void stop();

    /// Open a connection and sender for `addr_url`.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<sender> open_sender(const std::string& addr_url);

    /// Open a connection and sender for `addr_url`.
    ///
    /// Supplied sender options will override the container's
    /// template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<sender> open_sender(const std::string& addr_url,
                                               const proton::sender_options& snd_opts);

    /// Open a connection and sender for `addr_url`.
    ///
    /// Supplied connection options will override the
    /// container's template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<sender> open_sender(const std::string& addr_url,
                                               const connection_options& conn_opts);

    /// Open a connection and sender for `addr_url`.
    ///
    /// Supplied sender or connection options will override the
    /// container's template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<sender> open_sender(const std::string& addr_url,
                                               const proton::sender_options& snd_opts,
                                               const connection_options& conn_opts);

    /// Open a connection and receiver for `addr_url`.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string& addr_url);


    /// Open a connection and receiver for `addr_url`.
    ///
    /// Supplied receiver options will override the container's
    /// template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string& addr_url,
                                                   const proton::receiver_options& rcv_opts);

    /// Open a connection and receiver for `addr_url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string& addr_url,
                                                   const connection_options& conn_opts);

    /// Open a connection and receiver for `addr_url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    ///
    /// @copydetails returned
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string& addr_url,
                                                   const proton::receiver_options& rcv_opts,
                                                   const connection_options& conn_opts);

    /// A unique identifier for the container.
    PN_CPP_EXTERN std::string id() const;

    /// Connection options applied to outgoing connections. These are
    /// applied first and then overridden by any options provided in
    /// `connect()` or `messaging_handler::on_connection_open()`.
    PN_CPP_EXTERN void client_connection_options(const connection_options& conn_opts);

    /// @copydoc client_connection_options
    PN_CPP_EXTERN connection_options client_connection_options() const;

    /// Connection options applied to incoming connections. These are
    /// applied first and then overridden by any options provided in
    /// `listen()`, `listen_handler::on_accept()`, or
    /// `messaging_handler::on_connection_open()`.
    PN_CPP_EXTERN void server_connection_options(const connection_options& conn_opts);

    /// @copydoc server_connection_options
    PN_CPP_EXTERN connection_options server_connection_options() const;

    /// Sender options applied to senders created by this
    /// container. They are applied before
    /// `messaging_handler::on_sender_open()` and can be overridden.
    PN_CPP_EXTERN void sender_options(const class sender_options& snd_opts);

    /// @copydoc sender_options
    PN_CPP_EXTERN class sender_options sender_options() const;

    /// Receiver options applied to receivers created by this
    /// container. They are applied before
    /// `messaging_handler::on_receiver_open()` and can be overridden.
    PN_CPP_EXTERN void receiver_options(const class receiver_options& rcv_opts);

    /// @copydoc receiver_options
    PN_CPP_EXTERN class receiver_options receiver_options() const;

    /// Schedule `fn` for execution after a duration.  The piece of
    /// work can be created from a function object.
    ///
    /// **C++ versions** - With C++11 and later, use a
    /// `std::function<void()>` type for the `fn` parameter.
    PN_CPP_EXTERN void schedule(duration dur, work fn);

    /// **Deprecated** - Use `container::schedule(duration, work)`.
    PN_CPP_EXTERN PN_CPP_DEPRECATED("Use 'container::schedule(duration, work)'") void schedule(duration dur, void_function0& fn);

  private:
    /// Declare both v03 and v11 if compiling with c++11 as the library contains both.
    /// A C++11 user should never call the v03 overload so it is private in this case
#if PN_CPP_HAS_LAMBDAS && PN_CPP_HAS_VARIADIC_TEMPLATES
    PN_CPP_EXTERN void schedule(duration dur, internal::v03::work fn);
#endif
    class impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class connection_options;
  friend class session_options;
  friend class receiver_options;
  friend class sender_options;
  friend class work_queue;
    /// @endcond
};

} // proton

#endif // PROTON_CONTAINER_HPP
