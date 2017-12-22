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

/// A handler for Proton messaging events.
///
/// Subclass and override the event-handling member functions.
///
/// #### Close and error handling
///
/// There are several objects that have `on_X_close` and `on_X_error`
/// functions.  They are called as follows:
///
/// - If `X` is closed cleanly, with no error status, then `on_X_close`
///   is called.
///
/// - If `X` is closed with an error, then `on_X_error` is called,
///   followed by `on_X_close`. The error condition is also available
///   in `on_X_close` from `X::error()`.
///
/// By default, if you do not implement `on_X_error`, it will call
/// `on_error`.  If you do not implement `on_error` it will throw a
/// `proton::error` exception, which may not be what you want but
/// does help to identify forgotten error handling quickly.
///
/// #### Resource cleanup
///
/// Every `on_X_open` event is paired with an `on_X_close` event which
/// can clean up any resources created by the open handler.  In
/// particular this is still true if an error is reported with an
/// `on_X_error` event.  The error-handling logic doesn't have to
/// manage resource clean up.  It can assume that the close event will
/// be along to handle it.
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

    /// The underlying network transport has closed.
    PN_CPP_EXTERN virtual void on_transport_close(transport&);

    /// The underlying network transport has closed with an error
    /// condition.
    PN_CPP_EXTERN virtual void on_transport_error(transport&);

    /// The remote peer opened the connection.
    PN_CPP_EXTERN virtual void on_connection_open(connection&);

    /// The remote peer closed the connection.
    PN_CPP_EXTERN virtual void on_connection_close(connection&);

    /// The remote peer closed the connection with an error condition.
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

} // proton

#endif // PROTON_MESSAGING_HANDLER_HPP
