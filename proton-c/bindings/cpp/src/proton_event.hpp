#ifndef PROTON_CPP_PROTONEVENT_H
#define PROTON_CPP_PROTONEVENT_H

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
#include "proton/link.hpp"

#include "proton/event.h"

namespace proton {

class proton_handler;
class container;
class connection;
class connection_engine;

/** Event information for a proton::proton_handler */
class proton_event
{
  public:
    /// The type of an event
    enum event_type {
    ///@name Event types
    ///@{

      /**
      * Defined as a programming convenience. No event of this type will
      * ever be generated.
      */
      EVENT_NONE=PN_EVENT_NONE,

      /**
      * A reactor has been started. Events of this type point to the reactor.
      */
      REACTOR_INIT=PN_REACTOR_INIT,

      /**
      * A reactor has no more events to process. Events of this type
      * point to the reactor.
      */
      REACTOR_QUIESCED=PN_REACTOR_QUIESCED,

      /**
      * A reactor has been stopped. Events of this type point to the reactor.
      */
      REACTOR_FINAL=PN_REACTOR_FINAL,

      /**
      * A timer event has occurred.
      */
      TIMER_TASK=PN_TIMER_TASK,

      /**
      * The connection has been created. This is the first event that
      * will ever be issued for a connection. Events of this type point
      * to the relevant connection.
      */
      CONNECTION_INIT=PN_CONNECTION_INIT,

      /**
      * The connection has been bound to a transport. This event is
      * issued when the transport::bind() is called.
      */
      CONNECTION_BOUND=PN_CONNECTION_BOUND,

      /**
      * The connection has been unbound from its transport. This event is
      * issued when transport::unbind() is called.
      */
      CONNECTION_UNBOUND=PN_CONNECTION_UNBOUND,

      /**
      * The local connection endpoint has been closed. Events of this
      * type point to the relevant connection.
      */
      CONNECTION_LOCAL_OPEN=PN_CONNECTION_LOCAL_OPEN,

      /**
      * The remote endpoint has opened the connection. Events of this
      * type point to the relevant connection.
      */
      CONNECTION_REMOTE_OPEN=PN_CONNECTION_REMOTE_OPEN,

      /**
      * The local connection endpoint has been closed. Events of this
      * type point to the relevant connection.
      */
      CONNECTION_LOCAL_CLOSE=PN_CONNECTION_LOCAL_CLOSE,

      /**
      *  The remote endpoint has closed the connection. Events of this
      *  type point to the relevant connection.
      */
      CONNECTION_REMOTE_CLOSE=PN_CONNECTION_REMOTE_CLOSE,

      /**
      * The connection has been freed and any outstanding processing has
      * been completed. This is the final event that will ever be issued
      * for a connection.
      */
      CONNECTION_FINAL=PN_CONNECTION_FINAL,

      /**
      * The session has been created. This is the first event that will
      * ever be issued for a session.
      */
      SESSION_INIT=PN_SESSION_INIT,

      /**
      * The local session endpoint has been opened. Events of this type
      * point to the relevant session.
      */
      SESSION_LOCAL_OPEN=PN_SESSION_LOCAL_OPEN,

      /**
      * The remote endpoint has opened the session. Events of this type
      * point to the relevant session.
      */
      SESSION_REMOTE_OPEN=PN_SESSION_REMOTE_OPEN,

      /**
      * The local session endpoint has been closed. Events of this type
      * point ot the relevant session.
      */
      SESSION_LOCAL_CLOSE=PN_SESSION_LOCAL_CLOSE,

      /**
      * The remote endpoint has closed the session. Events of this type
      * point to the relevant session.
      */
      SESSION_REMOTE_CLOSE=PN_SESSION_REMOTE_CLOSE,

      /**
      * The session has been freed and any outstanding processing has
      * been completed. This is the final event that will ever be issued
      * for a session.
      */
      SESSION_FINAL=PN_SESSION_FINAL,

      /**
      * The link has been created. This is the first event that will ever
      * be issued for a link.
      */
      LINK_INIT=PN_LINK_INIT,

      /**
      * The local link endpoint has been opened. Events of this type
      * point ot the relevant link.
      */
      LINK_LOCAL_OPEN=PN_LINK_LOCAL_OPEN,

      /**
      * The remote endpoint has opened the link. Events of this type
      * point to the relevant link.
      */
      LINK_REMOTE_OPEN=PN_LINK_REMOTE_OPEN,

      /**
      * The local link endpoint has been closed. Events of this type
      * point ot the relevant link.
      */
      LINK_LOCAL_CLOSE=PN_LINK_LOCAL_CLOSE,

      /**
      * The remote endpoint has closed the link. Events of this type
      * point to the relevant link.
      */
      LINK_REMOTE_CLOSE=PN_LINK_REMOTE_CLOSE,

      /**
      * The local link endpoint has been detached. Events of this type
      * point to the relevant link.
      */
      LINK_LOCAL_DETACH=PN_LINK_LOCAL_DETACH,

      /**
      * The remote endpoint has detached the link. Events of this type
      * point to the relevant link.
      */
      LINK_REMOTE_DETACH=PN_LINK_REMOTE_DETACH,

      /**
      * The flow control state for a link has changed. Events of this
      * type point to the relevant link.
      */
      LINK_FLOW=PN_LINK_FLOW,

      /**
      * The link has been freed and any outstanding processing has been
      * completed. This is the final event that will ever be issued for a
      * link. Events of this type point to the relevant link.
      */
      LINK_FINAL=PN_LINK_FINAL,

      /**
      * A delivery has been created or updated. Events of this type point
      * to the relevant delivery.
      */
      DELIVERY=PN_DELIVERY,

      /**
      * The transport has new data to read and/or write. Events of this
      * type point to the relevant transport.
      */
      TRANSPORT=PN_TRANSPORT,

      /**
      * The transport has authenticated, if this is received by a server
      * the associated transport has authenticated an incoming connection
      * and transport::user() can be used to obtain the authenticated
      * user.
      */
      TRANSPORT_AUTHENTICATED=PN_TRANSPORT_AUTHENTICATED,

      /**
      * Indicates that a transport error has occurred. Use
      * transport::condition() to access the details of the error
      * from the associated transport.
      */
      TRANSPORT_ERROR=PN_TRANSPORT_ERROR,

      /**
      * Indicates that the head of the transport has been closed. This
      * means the transport will never produce more bytes for output to
      * the network. Events of this type point to the relevant transport.
      */
      TRANSPORT_HEAD_CLOSED=PN_TRANSPORT_HEAD_CLOSED,

      /**
      * Indicates that the tail of the transport has been closed. This
      * means the transport will never be able to process more bytes from
      * the network. Events of this type point to the relevant transport.
      */
      TRANSPORT_TAIL_CLOSED=PN_TRANSPORT_TAIL_CLOSED,

      /**
      * Indicates that the both the head and tail of the transport are
      * closed. Events of this type point to the relevant transport.
      */
      TRANSPORT_CLOSED=PN_TRANSPORT_CLOSED,

      SELECTABLE_INIT=PN_SELECTABLE_INIT,
      SELECTABLE_UPDATED=PN_SELECTABLE_UPDATED,
      SELECTABLE_READABLE=PN_SELECTABLE_READABLE,
      SELECTABLE_WRITABLE=PN_SELECTABLE_WRITABLE,
      SELECTABLE_ERROR=PN_SELECTABLE_ERROR,
      SELECTABLE_EXPIRED=PN_SELECTABLE_EXPIRED,
      SELECTABLE_FINAL=PN_SELECTABLE_FINAL
    };
    ///@}

    proton_event(pn_event_t *, pn_event_type_t, class container*);

    std::string name() const;

    void dispatch(proton_handler& h);

    class container* container() const;

    class transport transport() const;
    class connection connection() const;
    class session session() const;
    class sender sender() const;
    class receiver receiver() const;
    class link link() const;
    class delivery delivery() const;

    /** Get type of event */
    event_type type() const;

    pn_event_t* pn_event() const;

  private:
    mutable pn_event_t *pn_event_;
    event_type type_;
    class container *container_;
};

}

#endif  /*!PROTON_CPP_PROTONEVENT_H*/
