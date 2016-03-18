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
class event;
class messaging_adapter;

/// Callback functions for handling proton events.
///
/// Subclass and override event-handling member functions.
///
/// @see proton::event
class
PN_CPP_CLASS_EXTERN handler
{
  public:
    /// @cond INTERNAL
    /// XXX move configuration to connection or container

    /// Create a handler.
    ///
    /// @param prefetch set flow control to automatically pre-fetch
    /// this many messages
    ///
    /// @param auto_accept automatically accept received messages
    /// after on_message()
    ///
    /// @param auto_settle automatically settle on receipt of delivery
    /// for sent messages
    ///
    /// @param peer_close_is_error treat orderly remote connection
    /// close as error
    PN_CPP_EXTERN handler(int prefetch=10, bool auto_accept=true,
                          bool auto_settle=true,
                          bool peer_close_is_error=false);

    /// @endcond

    PN_CPP_EXTERN virtual ~handler();

    /// @name Event callbacks
    ///
    /// Override these member functions to handle events.
    ///
    /// @{

    /// The event loop is starting.
    PN_CPP_EXTERN virtual void on_start(event &e);
    /// A message is received.
    PN_CPP_EXTERN virtual void on_message(event &e);
    /// A message can be sent.
    PN_CPP_EXTERN virtual void on_sendable(event &e);

    /// transport_open is not present because currently there is no specific
    /// low level event to hang it from - you should put any initialisation code
    /// that needs a transport into the conection_open event.
    /// XXX Actually this makes me wonder if we shouldn't just introduce this event
    /// XXX and call its handler immediately before on_connection_open, just for the
    /// XXX symmetry of the API.

    /// The underlying network transport has closed.
    PN_CPP_EXTERN virtual void on_transport_close(event &e);
    /// The underlying network transport has closed with an error
    /// condition.
    PN_CPP_EXTERN virtual void on_transport_error(event &e);

    /// Note that every ..._open event is paired with a ..._close event which can clean
    /// up any resources created by the ..._open handler.
    /// In particular this is still true if an error is reported with an ..._error event.
    /// This makes resource management easier so that the error handling logic doesn't also
    /// have to manage the resource clean up, but can just assume that the close event will
    /// be along in a minute to handle the clean up.

    /// The remote peer opened the connection.
    PN_CPP_EXTERN virtual void on_connection_open(event &e);
    /// The remote peer closed the connection.
    PN_CPP_EXTERN virtual void on_connection_close(event &e);
    /// The remote peer closed the connection with an error condition.
    PN_CPP_EXTERN virtual void on_connection_error(event &e);

    /// The remote peer opened the session.
    PN_CPP_EXTERN virtual void on_session_open(event &e);
    /// The remote peer closed the session.
    PN_CPP_EXTERN virtual void on_session_close(event &e);
    /// The remote peer closed the session with an error condition.
    PN_CPP_EXTERN virtual void on_session_error(event &e);

    /// The remote peer opened the link.
    PN_CPP_EXTERN virtual void on_link_open(event &e);
    /// The remote peer closed the link.
    PN_CPP_EXTERN virtual void on_link_close(event &e);
    /// The remote peer closed the link with an error condition.
    PN_CPP_EXTERN virtual void on_link_error(event &e);

    /// The remote peer accepted an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_accept(event &e);
    /// The remote peer rejected an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_reject(event &e);
    /// The remote peer released an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_release(event &e);
    /// The remote peer settled an outgoing message.
    PN_CPP_EXTERN virtual void on_delivery_settle(event &e);

    // XXX are we missing on_delivery_modify?
    // XXX on_delivery_accept (and co) is a more discriminated on_delivery_settle

    // XXX note that AMQP modified state is indicated in _release

    /// @cond INTERNAL
    /// XXX settle API questions around task
    /// XXX register functions instead of having these funny generic events
    /// A timer fired.
    PN_CPP_EXTERN virtual void on_timer(event &e);
    /// @endcond

    /// Fallback event handling.
    PN_CPP_EXTERN virtual void on_unhandled(event &e);
    /// Fallback error handling.
    PN_CPP_EXTERN virtual void on_unhandled_error(event &e, const condition &c);

    /// @}

    /// @cond INTERNAL
  private:
    internal::pn_unique_ptr<messaging_adapter> messaging_adapter_;

    friend class container;
    friend class connection_engine;
    friend class connection_options;
    friend class link_options;
    /// @endcond
};

}

#endif // PROTON_CPP_MESSAGING_HANDLER_H
