#ifndef PROTON_CPP_MESSAGING_HANDLER_H
#define PROTON_CPP_MESSAGING_HANDLER_H

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

#include "proton/export.hpp"

#include "proton/pn_unique_ptr.hpp"

namespace proton {

class condition;
class container;
class event;
class transport;
class connection;
class session;
class link;
class sender;
class receiver;
class delivery;
class message;
class messaging_adapter;

namespace io {
class connection_engine;
}

/// Callback functions for handling proton events.
///
/// Subclass and override event-handling member functions.
///
/// @see proton::event
class
PN_CPP_CLASS_EXTERN handler
{
  public:
    PN_CPP_EXTERN handler();
    PN_CPP_EXTERN virtual ~handler();

    /// @name Event callbacks
    ///
    /// Override these member functions to handle events.
    ///
    /// @{

    /// The event loop is starting.
    PN_CPP_EXTERN virtual void on_container_start(container &c);
    /// A message is received.
    PN_CPP_EXTERN virtual void on_message(delivery &d, message &m);
    /// A message can be sent.
    PN_CPP_EXTERN virtual void on_sendable(sender &s);

    /// transport_open is not present because currently there is no specific
    /// low level event to hang it from - you should put any initialisation code
    /// that needs a transport into the conection_open event.
    /// XXX Actually this makes me wonder if we shouldn't just introduce this event
    /// XXX and call its handler immediately before on_connection_open, just for the
    /// XXX symmetry of the API.

    /// The underlying network transport has closed.
    PN_CPP_EXTERN virtual void on_transport_close(transport &t);
    /// The underlying network transport has closed with an error
    /// condition.
    PN_CPP_EXTERN virtual void on_transport_error(transport &t);

    /// Note that every ..._open event is paired with a ..._close event which can clean
    /// up any resources created by the ..._open handler.
    /// In particular this is still true if an error is reported with an ..._error event.
    /// This makes resource management easier so that the error handling logic doesn't also
    /// have to manage the resource clean up, but can just assume that the close event will
    /// be along in a minute to handle the clean up.

    /// The remote peer opened the connection.
    PN_CPP_EXTERN virtual void on_connection_open(connection &c);
    /// The remote peer closed the connection.
    PN_CPP_EXTERN virtual void on_connection_close(connection &c);
    /// The remote peer closed the connection with an error condition.
    PN_CPP_EXTERN virtual void on_connection_error(connection &c);

    /// The remote peer opened the session.
    PN_CPP_EXTERN virtual void on_session_open(session &s);
    /// The remote peer closed the session.
    PN_CPP_EXTERN virtual void on_session_close(session &s);
    /// The remote peer closed the session with an error condition.
    PN_CPP_EXTERN virtual void on_session_error(session &s);

    /// The remote peer opened the link.
    PN_CPP_EXTERN virtual void on_receiver_open(receiver& l);
    /// The remote peer closed the link.
    PN_CPP_EXTERN virtual void on_receiver_close(receiver& l);
    /// The remote peer closed the link with an error condition.
    PN_CPP_EXTERN virtual void on_receiver_error(receiver& l);

    /// The remote peer opened the link.
    PN_CPP_EXTERN virtual void on_sender_open(sender& l);
    /// The remote peer closed the link.
    PN_CPP_EXTERN virtual void on_sender_close(sender& l);
    /// The remote peer closed the link with an error condition.
    PN_CPP_EXTERN virtual void on_sender_error(sender& l);

    /// The remote peer accepted an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_accept(delivery &d);
    /// The remote peer rejected an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_reject(delivery &d);
    /// The remote peer released an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_release(delivery &d);
    /// The remote peer settled an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_settle(delivery &d);

    // XXX are we missing on_delivery_modify?
    // XXX on_delivery_accept (and co) is a more discriminated on_delivery_settle

    // XXX note that AMQP modified state is indicated in _release

    /// @cond INTERNAL
    /// XXX settle API questions around task
    /// XXX register functions instead of having these funny generic events
    /// A timer fired.
    PN_CPP_EXTERN virtual void on_timer(container &c);
    /// @endcond

    /// Fallback error handling.
    PN_CPP_EXTERN virtual void on_unhandled_error(const condition &c);

    /// @}

    /// @cond INTERNAL
  private:
    internal::pn_unique_ptr<messaging_adapter> messaging_adapter_;

    friend class container;
    friend class io::connection_engine;
    friend class connection_options;
    friend class link_options;
    /// @endcond
};

}

#endif // PROTON_CPP_MESSAGING_HANDLER_H
