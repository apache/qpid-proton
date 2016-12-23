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
#include "./types_fwd.hpp"

#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"

#ifdef PN_CPP_HAS_STD_FUNCTION
#include <functional>
#endif
#include <string>

namespace proton {

/// A top-level container of connections, sessions, senders, and
/// receivers.
///
/// A container gives a unique identity to each communicating peer. It
/// is often a process-level object.
///
/// It serves as an entry point to the API, allowing connections,
/// senders, and receivers to be established. It can be supplied with
/// an event handler in order to intercept important messaging events,
/// such as newly received messages or newly issued credit for sending
/// messages.
class PN_CPP_CLASS_EXTERN container {
  public:
    /// Create a container.
    PN_CPP_EXTERN container(messaging_handler& h, const std::string& id="");

    /// Create a container.
    PN_CPP_EXTERN container(const std::string& id="");

    PN_CPP_EXTERN ~container();

    /// Connect to `url` and send an open request to the remote peer.
    ///
    /// Options are applied to the connection as follows, values in later
    /// options override earlier ones:
    ///
    ///  1. client_connection_options()
    ///  2. options passed to connect()
    ///
    /// The handler in the composed options is used to call
    /// proton::messaging_handler::on_connection_open() when the remote peer's
    /// open response is received.
    PN_CPP_EXTERN returned<connection> connect(const std::string& url, const connection_options &);

    /// Connect to `url` and send an open request to the remote peer.
    PN_CPP_EXTERN returned<connection> connect(const std::string& url);

    /// @cond INTERNAL
    /// Stop listening on url, must match the url string given to listen().
    /// You can also use the proton::listener object returned by listen()
    PN_CPP_EXTERN void stop_listening(const std::string& url);
    /// @endcond

    /// Start listening on url.
    ///
    /// Calls to the @ref listen_handler are serialized for this listener,
    /// but handlers attached to separate listeners may be called concurrently.
    ///
    /// @param url identifies a listening url.
    /// @param lh handles listening events
    /// @return listener lets you stop listening
    PN_CPP_EXTERN listener listen(const std::string& url, listen_handler& lh);

    /// Listen with a fixed set of options for all accepted connections.
    /// See listen(const std::string&, listen_handler&)
    PN_CPP_EXTERN listener listen(const std::string& url, const connection_options&);

    /// Start listening on URL.
    /// New connections will use the handler from server_connection_options()
    PN_CPP_EXTERN listener listen(const std::string& url);

    /// Run the container in this thread.
    /// Returns when the container stops.
    /// @see auto_stop() and stop().
    ///
    /// With a multithreaded container, call run() in multiple threads to create a thread pool.
    PN_CPP_EXTERN void run();

    /// If true, stop the container when all active connections and listeners are closed.
    /// If false the container will keep running till stop() is called.
    ///
    /// auto_stop is set by default when a new container is created.
    PN_CPP_EXTERN void auto_stop(bool);

    /// **Experimental** - Stop the container with an error_condition
    /// err.
    ///
    ///  - Abort all open connections and listeners.
    ///  - Process final handler events and injected functions
    ///  - If `!err.empty()`, handlers will receive on_transport_error
    ///  - run() will return in all threads.
    PN_CPP_EXTERN void stop(const error_condition& err);

    /// **Experimental** - Stop the container with an empty error
    /// condition.
    ///
    /// @see stop(const error_condition&)
    PN_CPP_EXTERN void stop();

    /// Open a connection and sender for `url`.
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url);

    /// Open a connection and sender for `url`.
    ///
    /// Supplied sender options will override the container's
    /// template options.
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url,
                                         const proton::sender_options &o);

    /// Open a connection and sender for `url`.
    ///
    /// Supplied connection options will override the
    /// container's template options.
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url,
                                         const connection_options &c);

    /// Open a connection and sender for `url`.
    ///
    /// Supplied sender or connection options will override the
    /// container's template options.
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url,
                                         const proton::sender_options &o,
                                         const connection_options &c);

    /// Open a connection and receiver for `url`.
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url);


    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver options will override the container's
    /// template options.
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url,
                                             const proton::receiver_options &o);

    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url,
                                             const connection_options &c);

    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url,
                                             const proton::receiver_options &o,
                                             const connection_options &c);

    /// A unique identifier for the container.
    PN_CPP_EXTERN std::string id() const;

    /// Connection options that will be to outgoing connections. These
    /// are applied first and overriden by options provided in
    /// connect() and messaging_handler::on_connection_open().
    PN_CPP_EXTERN void client_connection_options(const connection_options &);

    /// @copydoc client_connection_options
    PN_CPP_EXTERN connection_options client_connection_options() const;

    /// Connection options that will be applied to incoming
    /// connections. These are applied first and overridden by options
    /// provided in listen(), listen_handler::on_accept() and
    /// messaging_handler::on_connection_open().
    PN_CPP_EXTERN void server_connection_options(const connection_options &);

    /// @copydoc server_connection_options
    PN_CPP_EXTERN connection_options server_connection_options() const;

    /// Sender options applied to senders created by this
    /// container. They are applied before messaging_handler::on_sender_open()
    /// and can be overridden.
    PN_CPP_EXTERN void sender_options(const class sender_options &);

    /// @copydoc sender_options
    PN_CPP_EXTERN class sender_options sender_options() const;

    /// Receiver options applied to receivers created by this
    /// container. They are applied before messaging_handler::on_receiver_open()
    /// and can be overridden.
    PN_CPP_EXTERN void receiver_options(const class receiver_options &);

    /// @copydoc receiver_options
    PN_CPP_EXTERN class receiver_options receiver_options() const;

    /// Schedule a function to be called after the duration.  C++03
    /// compatible, for C++11 use schedule(duration,
    /// std::function<void()>)
    PN_CPP_EXTERN void schedule(duration, void_function0&);

#if PN_CPP_HAS_STD_FUNCTION
    /// Schedule a function to be called after the duration
    PN_CPP_EXTERN void schedule(duration, std::function<void()>);
#endif

  private:
    class impl;
    internal::pn_unique_ptr<impl> impl_;

  friend class connection_options;
  friend class session_options;
  friend class receiver_options;
  friend class sender_options;
};

} // proton

#endif // PROTON_CONTAINER_HPP
