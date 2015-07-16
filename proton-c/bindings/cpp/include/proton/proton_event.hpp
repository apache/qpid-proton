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
#include "proton/event.hpp"
#include "proton/link.hpp"

namespace proton {

class handler;
class container;
class connection;
class container;

/** Event information for a proton::proton_handler */
class proton_event : public event
{
  public:

    ///@name Event types
    ///@{

    /// The type of an event
    typedef int event_type;

    /**
     * Defined as a programming convenience. No event of this type will
     * ever be generated.
     */
    PN_CPP_EXTERN static const event_type EVENT_NONE;

    /**
     * A reactor has been started. Events of this type point to the reactor.
     */
    PN_CPP_EXTERN static const event_type REACTOR_INIT;

    /**
     * A reactor has no more events to process. Events of this type
     * point to the reactor.
     */
    PN_CPP_EXTERN static const event_type REACTOR_QUIESCED;

    /**
     * A reactor has been stopped. Events of this type point to the reactor.
     */
    PN_CPP_EXTERN static const event_type REACTOR_FINAL;

    /**
     * A timer event has occurred.
     */
    PN_CPP_EXTERN static const event_type TIMER_TASK;

    /**
     * The connection has been created. This is the first event that
     * will ever be issued for a connection. Events of this type point
     * to the relevant connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_INIT;

    /**
     * The connection has been bound to a transport. This event is
     * issued when the transport::bind() is called.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_BOUND;

    /**
     * The connection has been unbound from its transport. This event is
     * issued when transport::unbind() is called.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_UNBOUND;

    /**
     * The local connection endpoint has been closed. Events of this
     * type point to the relevant connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_LOCAL_OPEN;

    /**
     * The remote endpoint has opened the connection. Events of this
     * type point to the relevant connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_REMOTE_OPEN;

    /**
     * The local connection endpoint has been closed. Events of this
     * type point to the relevant connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_LOCAL_CLOSE;

    /**
     *  The remote endpoint has closed the connection. Events of this
     *  type point to the relevant connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_REMOTE_CLOSE;

    /**
     * The connection has been freed and any outstanding processing has
     * been completed. This is the final event that will ever be issued
     * for a connection.
     */
    PN_CPP_EXTERN static const event_type CONNECTION_FINAL;

    /**
     * The session has been created. This is the first event that will
     * ever be issued for a session.
     */
    PN_CPP_EXTERN static const event_type SESSION_INIT;

    /**
     * The local session endpoint has been opened. Events of this type
     * point ot the relevant session.
     */
    PN_CPP_EXTERN static const event_type SESSION_LOCAL_OPEN;

    /**
     * The remote endpoint has opened the session. Events of this type
     * point to the relevant session.
     */
    PN_CPP_EXTERN static const event_type SESSION_REMOTE_OPEN;

    /**
     * The local session endpoint has been closed. Events of this type
     * point ot the relevant session.
     */
    PN_CPP_EXTERN static const event_type SESSION_LOCAL_CLOSE;

    /**
     * The remote endpoint has closed the session. Events of this type
     * point to the relevant session.
     */
    PN_CPP_EXTERN static const event_type SESSION_REMOTE_CLOSE;

    /**
     * The session has been freed and any outstanding processing has
     * been completed. This is the final event that will ever be issued
     * for a session.
     */
    PN_CPP_EXTERN static const event_type SESSION_FINAL;

    /**
     * The link has been created. This is the first event that will ever
     * be issued for a link.
     */
    PN_CPP_EXTERN static const event_type LINK_INIT;

    /**
     * The local link endpoint has been opened. Events of this type
     * point ot the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_LOCAL_OPEN;

    /**
     * The remote endpoint has opened the link. Events of this type
     * point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_REMOTE_OPEN;

    /**
     * The local link endpoint has been closed. Events of this type
     * point ot the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_LOCAL_CLOSE;

    /**
     * The remote endpoint has closed the link. Events of this type
     * point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_REMOTE_CLOSE;

    /**
     * The local link endpoint has been detached. Events of this type
     * point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_LOCAL_DETACH;

    /**
     * The remote endpoint has detached the link. Events of this type
     * point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_REMOTE_DETACH;

    /**
     * The flow control state for a link has changed. Events of this
     * type point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_FLOW;

    /**
     * The link has been freed and any outstanding processing has been
     * completed. This is the final event that will ever be issued for a
     * link. Events of this type point to the relevant link.
     */
    PN_CPP_EXTERN static const event_type LINK_FINAL;

    /**
     * A delivery has been created or updated. Events of this type point
     * to the relevant delivery.
     */
    PN_CPP_EXTERN static const event_type DELIVERY;

    /**
     * The transport has new data to read and/or write. Events of this
     * type point to the relevant transport.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT;

    /**
     * The transport has authenticated, if this is received by a server
     * the associated transport has authenticated an incoming connection
     * and transport::user() can be used to obtain the authenticated
     * user.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT_AUTHENTICATED;

    /**
     * Indicates that a transport error has occurred. Use
     * transport::condition() to access the details of the error
     * from the associated transport.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT_ERROR;

    /**
     * Indicates that the head of the transport has been closed. This
     * means the transport will never produce more bytes for output to
     * the network. Events of this type point to the relevant transport.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT_HEAD_CLOSED;

    /**
     * Indicates that the tail of the transport has been closed. This
     * means the transport will never be able to process more bytes from
     * the network. Events of this type point to the relevant transport.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT_TAIL_CLOSED;

    /**
     * Indicates that the both the head and tail of the transport are
     * closed. Events of this type point to the relevant transport.
     */
    PN_CPP_EXTERN static const event_type TRANSPORT_CLOSED;

    PN_CPP_EXTERN static const event_type SELECTABLE_INIT;
    PN_CPP_EXTERN static const event_type SELECTABLE_UPDATED;
    PN_CPP_EXTERN static const event_type SELECTABLE_READABLE;
    PN_CPP_EXTERN static const event_type SELECTABLE_WRITABLE;
    PN_CPP_EXTERN static const event_type SELECTABLE_ERROR;
    PN_CPP_EXTERN static const event_type SELECTABLE_EXPIRED;
    PN_CPP_EXTERN static const event_type SELECTABLE_FINAL;

    ///@}

    virtual PN_CPP_EXTERN void dispatch(handler &h);
    virtual PN_CPP_EXTERN class container &container();
    virtual PN_CPP_EXTERN class connection &connection();
    virtual PN_CPP_EXTERN class sender sender();
    virtual PN_CPP_EXTERN class receiver receiver();
    virtual PN_CPP_EXTERN class link link();

    /** Get type of event */
    PN_CPP_EXTERN event_type type();

    PN_CPP_EXTERN pn_event_t* pn_event();

  protected:
    PN_CPP_EXTERN proton_event(pn_event_t *ce, pn_event_type_t t, class container &c);
  private:
    pn_event_t *pn_event_;
    event_type type_;
    class container &container_;
};

}

#endif  /*!PROTON_CPP_PROTONEVENT_H*/
