#ifndef PROTON_MESSAGING_HANDLER_HPP
#define PROTON_MESSAGING_HANDLER_HPP

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
#include "./internal/export.hpp"

/// @file
/// @copybrief proton::messaging_handler

namespace proton {


/// Handler for Proton messaging events.
///
/// Subclass and override the event-handling member functions.
///
/// **Thread-safety**: A thread-safe handler can use the objects
/// passed as arguments, and other objects belonging to the same
/// proton::connection.  It *must not* use objects belonging to a
/// different connection. See @ref mt_page and proton::work_queue for
/// safe ways to communicate between connections.  Thread-safe
/// handlers can also be used in single-threaded code.
///
/// **Single-threaded only**: An application is single-threaded if it
/// calls container::run() exactly once, and only makes make proton
/// calls from handler functions. Single-threaded handler functions
/// can use objects belonging to another connection, but *must* call
/// connection::wake() on the other connection before returning. Such
/// a handler is not thread-safe.
///
/// ### Overview of connection life-cycle and automatic re-connect
///
/// See the documentation of individual event functions for more details.
///
/// on_connection_open() - @copybrief on_connection_open()
///
/// on_transport_error() -@copybrief on_transport_error()
///
/// on_connection_error() - @copybrief on_connection_error()
///
/// on_connection_close() - @copybrief on_connection_close()
///
/// on_transport_close() - @copybrief on_transport_close()
///
/// @see reconnect_options
///
class
PN_CPP_CLASS_EXTERN messaging_handler {
  public:
    PN_CPP_EXTERN messaging_handler();
    PN_CPP_EXTERN virtual ~messaging_handler();

    /// The container event loop is starting.
    ///
    /// This is the first event received after calling
    /// `container::run()`.
    PN_CPP_EXTERN virtual void on_container_start(container&);

    /// The container event loop is stopping.
    ///
    /// This is the last event received before the container event
    /// loop stops.
    PN_CPP_EXTERN virtual void on_container_stop(container&);

    /// A message is received.
    PN_CPP_EXTERN virtual void on_message(delivery&, message&);

    /// A message can be sent.
    PN_CPP_EXTERN virtual void on_sendable(sender&);

    /// The underlying network transport is open
    PN_CPP_EXTERN virtual void on_transport_open(transport&);

    /// The final event for a connection: there will be no more
    /// reconnect attempts and no more event functions.
    ///
    /// Called exactly once regardless of how many times the connection
    /// was re-connected, whether it failed due to transport or connection
    /// errors or was closed without error.
    ///
    /// This is a good place to do per-connection clean-up.
    /// transport::connection() is always valid at this point.
    ///
    PN_CPP_EXTERN virtual void on_transport_close(transport&);

    /// Unexpected disconnect, transport::error() provides details; if @ref
    /// reconnect_options are enabled there may be an automatic re-connect.
    ///
    /// If there is a successful automatic reconnect, on_connection_open() will
    /// be called again.
    ///
    /// transport::connection() is always valid at this point.
    ///
    /// Calling connection::close() will cancel automatic re-connect and force
    /// the transport to close immediately, see on_transport_close()
    ///
    PN_CPP_EXTERN virtual void on_transport_error(transport&);

    /// The remote peer opened the connection: called once on initial open, and
    /// again on each successful automatic re-connect (see @ref
    /// reconnect_options)
    ///
    /// connection::reconnected() is false for the initial call, true for
    /// any subsequent calls due to reconnect.
    ///
    /// @note You can use the initial call to open initial @ref session, @ref
    /// sender and @ref receiver endpoints. All active endpoints are *automatically*
    /// re-opened after an automatic re-connect so you should take care
    /// not to open them again, for example:
    ///
    ///     if (!c.reconnected()) {
    ///         // Do initial per-connection setup here.
    ///         // Open initial senders/receivers if needed.
    ///     } else {
    ///         // Handle a successful reconnect here.
    ///         // NOTE: Don't re-open active senders/receivers
    ///     }
    ///
    /// For a server on_connection_open() is called when a client attempts to
    /// open a connection.  The server can call connection::open() to accept or
    /// connection::close(const error_condition&) to reject.
    ///
    /// @see reconnect_options, on_transport_error()
    PN_CPP_EXTERN virtual void on_connection_open(connection&);

    /// The remote peer closed the connection.
    ///
    /// If there was a connection error, this is called immediately after
    /// on_connection_error() and connection::error() is set
    ///
    PN_CPP_EXTERN virtual void on_connection_close(connection&);

    /// The remote peer indicated a fatal error, connection::error() provides details.
    ///
    /// A connection error is an application problem, for example an illegal
    /// action or a lack of resources. There will be no automatic re-connect
    /// attempts, on_connection_close() will be called immediately after.
    ///
    /// @note A connection close with the special error condition
    /// `amqp:connection-forced` is treated as a transport error, not a
    /// connection error, see on_transport_error()
    ///
    PN_CPP_EXTERN virtual void on_connection_error(connection&);

    /// The remote peer opened the session.
    PN_CPP_EXTERN virtual void on_session_open(session&);

    /// The remote peer closed the session.
    PN_CPP_EXTERN virtual void on_session_close(session&);

    /// The remote peer closed the session with an error condition.
    PN_CPP_EXTERN virtual void on_session_error(session&);

    /// The remote peer opened the link.
    PN_CPP_EXTERN virtual void on_receiver_open(receiver&);

    /// The remote peer detached the link.
    PN_CPP_EXTERN virtual void on_receiver_detach(receiver&);

    /// The remote peer closed the link.
    PN_CPP_EXTERN virtual void on_receiver_close(receiver&);

    /// The remote peer closed the link with an error condition.
    PN_CPP_EXTERN virtual void on_receiver_error(receiver&);

    /// The remote peer opened the link.
    PN_CPP_EXTERN virtual void on_sender_open(sender&);

    /// The remote peer detached the link.
    PN_CPP_EXTERN virtual void on_sender_detach(sender&);

    /// The remote peer closed the link.
    PN_CPP_EXTERN virtual void on_sender_close(sender&);

    /// The remote peer closed the link with an error condition.
    PN_CPP_EXTERN virtual void on_sender_error(sender&);

    /// The receiving peer accepted a transfer.
    PN_CPP_EXTERN virtual void on_tracker_accept(tracker&);

    /// The receiving peer rejected a transfer.
    PN_CPP_EXTERN virtual void on_tracker_reject(tracker&);

    /// The receiving peer released a transfer.
    PN_CPP_EXTERN virtual void on_tracker_release(tracker&);

    /// The receiving peer settled a transfer.
    PN_CPP_EXTERN virtual void on_tracker_settle(tracker&);

    /// The sending peer settled a transfer.
    PN_CPP_EXTERN virtual void on_delivery_settle(delivery&);

    /// **Unsettled API** - The receiving peer has requested a drain of
    /// remaining credit.
    PN_CPP_EXTERN virtual void on_sender_drain_start(sender&);

    /// **Unsettled API** - The credit outstanding at the time of the
    /// drain request has been consumed or returned.
    PN_CPP_EXTERN virtual void on_receiver_drain_finish(receiver&);

    /// **Unsettled API** - An event that can be triggered from
    /// another thread.
    ///
    /// This event is triggered by a call to `connection::wake()`.  It
    /// is used to notify the application that something needs
    /// attention.
    ///
    /// **Thread-safety** - The application handler and the triggering
    /// thread must use some form of thread-safe state or
    /// communication to tell the handler what it needs to do.  See
    /// `proton::work_queue` for an easier way to execute code safely
    /// in the handler thread.
    ///
    /// @note Spurious calls to `on_connection_wake()` can occur
    /// without any application call to `connection::wake()`.
    PN_CPP_EXTERN virtual void on_connection_wake(connection&);

    /// Fallback error handling.
    PN_CPP_EXTERN virtual void on_error(const error_condition&);
};

} // namespace proton

#endif // PROTON_MESSAGING_HANDLER_HPP
