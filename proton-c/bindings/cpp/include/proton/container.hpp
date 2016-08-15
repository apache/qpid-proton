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

#include "./connection_options.hpp"
#include "./function.hpp"
#include "./listener.hpp"
#include "./receiver_options.hpp"
#include "./sender_options.hpp"
#include "./thread_safe.hpp"

#include "./internal/config.hpp"
#include "./internal/export.hpp"

#include <string>

namespace proton {

class connection;
class connection_options;
class container_impl;
class messaging_handler;
class listen_handler;
class listener;
class receiver;
class receiver_options;
class sender;
class sender_options;
class task;

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
    PN_CPP_EXTERN virtual ~container();

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
    virtual returned<connection> connect(const std::string& url, const connection_options &) = 0;

    /// Connect to `url` and send an open request to the remote peer.
    virtual returned<connection> connect(const std::string& url) = 0;

    /// @cond INTERNAL
    /// Stop listening on url, must match the url string given to listen().
    /// You can also use the proton::listener object returned by listen()
    virtual void stop_listening(const std::string& url) = 0;
    /// @endcond

    /// Start listening on url.
    ///
    /// Calls to the @ref listen_handler are serialized for this listener,
    /// but handlers attached to separate listeners may be called concurrently.
    ///
    /// @param url identifies a listening url.
    /// @param lh handles listening events
    /// @return listener lets you stop listening
    virtual listener listen(const std::string& url, listen_handler& lh) = 0;

    /// Listen with a fixed set of options for all accepted connections.
    /// See listen(const std::string&, listen_handler&)
    virtual listener listen(const std::string& url, const connection_options&) = 0;

    /// Start listening on URL.
    /// New connections will use the handler from server_connection_options()
    virtual listener listen(const std::string& url) = 0;

    /// Run the container in this thread.
    /// Returns when the container stops.
    /// @see auto_stop() and stop().
    ///
    /// With a multithreaded container, call run() in multiple threads to create a thread pool.
    virtual void run() = 0;

    /// If true, stop the container when all active connections and listeners are closed.
    /// If false the container will keep running till stop() is called.
    ///
    /// auto_stop is set by default when a new container is created.
    virtual void auto_stop(bool) = 0;

    /// **Experimental** - Stop the container with an error_condition
    /// err.
    ///
    ///  - Abort all open connections and listeners.
    ///  - Process final handler events and injected functions
    ///  - If `!err.empty()`, handlers will receive on_transport_error
    ///  - run() will return in all threads.
    virtual void stop(const error_condition& err) = 0;

    /// **Experimental** - Stop the container with an empty error
    /// condition.
    ///
    /// @see stop(const error_condition&)
    virtual void stop() = 0;

    /// Open a connection and sender for `url`.
    virtual returned<sender> open_sender(const std::string &url) = 0;

    /// Open a connection and sender for `url`.
    ///
    /// Supplied sender options will override the container's
    /// template options.
    virtual returned<sender> open_sender(const std::string &url,
                                         const proton::sender_options &o) = 0;

    /// Open a connection and sender for `url`.
    ///
    /// Supplied connection options will override the
    /// container's template options.
    virtual returned<sender> open_sender(const std::string &url,
                                         const connection_options &c) = 0;

    /// Open a connection and sender for `url`.
    ///
    /// Supplied sender or connection options will override the
    /// container's template options.
    virtual returned<sender> open_sender(const std::string &url,
                                         const proton::sender_options &o,
                                         const connection_options &c) = 0;

    /// Open a connection and receiver for `url`.
    virtual returned<receiver> open_receiver(const std::string&url) = 0;


    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver options will override the container's
    /// template options.
    virtual returned<receiver> open_receiver(const std::string&url,
                                             const proton::receiver_options &o) = 0;

    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    virtual returned<receiver> open_receiver(const std::string&url,
                                             const connection_options &c) = 0;

    /// Open a connection and receiver for `url`.
    ///
    /// Supplied receiver or connection options will override the
    /// container's template options.
    virtual returned<receiver> open_receiver(const std::string&url,
                                             const proton::receiver_options &o,
                                             const connection_options &c) = 0;

    /// A unique identifier for the container.
    virtual std::string id() const = 0;

    /// Connection options that will be to outgoing connections. These
    /// are applied first and overriden by options provided in
    /// connect() and messaging_handler::on_connection_open().
    virtual void client_connection_options(const connection_options &) = 0;

    /// @copydoc client_connection_options
    virtual connection_options client_connection_options() const = 0;

    /// Connection options that will be applied to incoming
    /// connections. These are applied first and overridden by options
    /// provided in listen(), listen_handler::on_accept() and
    /// messaging_handler::on_connection_open().
    virtual void server_connection_options(const connection_options &) = 0;

    /// @copydoc server_connection_options
    virtual connection_options server_connection_options() const = 0;

    /// Sender options applied to senders created by this
    /// container. They are applied before messaging_handler::on_sender_open()
    /// and can be overridden.
    virtual void sender_options(const class sender_options &) = 0;

    /// @copydoc sender_options
    virtual class sender_options sender_options() const = 0;

    /// Receiver options applied to receivers created by this
    /// container. They are applied before messaging_handler::on_receiver_open()
    /// and can be overridden.
    virtual void receiver_options(const class receiver_options &) = 0;

    /// @copydoc receiver_options
    virtual class receiver_options receiver_options() const = 0;

#if PN_CPP_HAS_STD_FUNCTION
    /// Schedule a function to be called after the duration
    virtual void schedule(duration, std::function<void()>) = 0;
#endif
    /// Schedule a function to be called after the duration.
    /// C++03 compatible, for C++11 use the schedule(duration, std::function<void()>)
    virtual void schedule(duration, void_function0&) = 0;
};

/// @cond INTERNAL
/// This class is intended for container implementers, it simplifies implementing container
/// by performing all the default actions for shortened method signatures
///
/// Note: This class will only be useful if you want all the ususal defaulted behaviours
/// in your class as it makes them all non virtual, so you can't use just some of them.
///
/// It means that in the usual case the container interface is smaller and a little simpler.
class PN_CPP_CLASS_EXTERN standard_container : public container {
  public:
    // Pull in base class functions here so we don't need to define them again
    using container::stop;
    using container::connect;
    using container::listen;
    using container::open_receiver;
    using container::open_sender;

    PN_CPP_EXTERN returned<connection> connect(const std::string& url);
    PN_CPP_EXTERN listener listen(const std::string& url, const connection_options&);
    PN_CPP_EXTERN listener listen(const std::string& url);
    PN_CPP_EXTERN void stop();
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url);
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url,
                                               const proton::sender_options &o);
    PN_CPP_EXTERN returned<sender> open_sender(const std::string &url,
                                               const proton::connection_options &o);
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url);
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url,
                                                   const proton::receiver_options &o);
    PN_CPP_EXTERN returned<receiver> open_receiver(const std::string&url,
                                                   const proton::connection_options &o);
};
/// @endcond

/// This is an header only class that can be used to help using containers more natural
/// by allowing them to be treated as value types.
template <class Ptr>
class container_ref : public container {
  public:
#if PN_CPP_HAS_RVALUE_REFERENCES
    /// Move constructor.
    container_ref(Ptr&& p) : impl_(std::move(p)) {}
#else
    /// This class will only work correctly if ownership is transferred here
    /// so using std::auto_ptr for Ptr is necessary for pre C++11
    container_ref(Ptr p) : impl_(p) {}
#endif

    returned<connection> connect(const std::string& url, const connection_options& opts) { return impl_->connect(url, opts); }
    returned<connection> connect(const std::string& url) { return impl_->connect(url); }
    listener listen(const std::string& url, listen_handler& l) { return impl_->listen(url, l); }
    listener listen(const std::string& url, const connection_options& opts) { return impl_->listen(url, opts); }
    listener listen(const std::string& url) { return impl_->listen(url); }


    ///@cond INTERNAL
    void stop_listening(const std::string& url) { impl_->stop_listening(url); }
    ///@endcond
    void run() { impl_->run(); }
    void auto_stop(bool set) { impl_->auto_stop(set); }

    ///@copydoc container::stop
    void stop(const error_condition& err) { impl_->stop(err); }
    ///@copydoc container::stop
    void stop() { impl_->stop(); }

    ///@copydoc container::open_sender
    returned<sender> open_sender(
        const std::string &url,
        const class sender_options &o,
        const connection_options &c) { return impl_->open_sender(url, o, c); }
    ///@copydoc container::open_sender
    returned<sender> open_sender(
        const std::string &url,
        const class connection_options &o) { return impl_->open_sender(url, o); }
    ///@copydoc container::open_sender
    returned<sender> open_sender(
        const std::string &url,
        const class sender_options &o) { return impl_->open_sender(url, o); }
    ///@copydoc container::open_sender
    returned<sender> open_sender(
        const std::string &url) { return impl_->open_sender(url); }

    ///@copydoc container::open_receiver
    returned<receiver> open_receiver(
        const std::string&url,
        const class receiver_options &o,
        const connection_options &c) { return impl_->open_receiver(url, o, c); }
    ///@copydoc container::open_receiver
    returned<receiver> open_receiver(
        const std::string&url,
        const class receiver_options &o) { return impl_->open_receiver(url, o); }
    ///@copydoc container::open_receiver
    returned<receiver> open_receiver(
        const std::string&url,
        const class connection_options &o) { return impl_->open_receiver(url, o); }
    ///@copydoc container::open_receiver
    returned<receiver> open_receiver(
        const std::string&url) { return impl_->open_receiver(url); }

    std::string id() const { return impl_->id(); }

#if PN_CPP_HAS_STD_FUNCTION
    PN_CPP_EXTERN void schedule(duration d, std::function<void()> f) { return impl_->schedule(d, f); }
#endif
    PN_CPP_EXTERN void schedule(duration d, void_function0& f) { return impl_->schedule(d, f); }

    void client_connection_options(const connection_options& c) { impl_->client_connection_options(c); }
    connection_options client_connection_options() const { return impl_->client_connection_options(); }

    void server_connection_options(const connection_options &o) { impl_->server_connection_options(o); }
    connection_options server_connection_options() const { return impl_->server_connection_options(); }

    void sender_options(const class sender_options &o) { impl_->sender_options(o); }
    class sender_options sender_options() const { return impl_->sender_options(); }

    void receiver_options(const class receiver_options & o) { impl_->receiver_options(o); }
    class receiver_options receiver_options() const { return impl_->receiver_options(); }

  private:
    Ptr impl_;
};

} // proton

#endif // PROTON_CONTAINER_HPP
