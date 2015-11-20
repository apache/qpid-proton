#ifndef PROTON_CPP_MESSAGINGEVENT_H
#define PROTON_CPP_MESSAGINGEVENT_H

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
#include "proton_event.hpp"
#include "proton/link.hpp"
#include "proton/message.hpp"

namespace proton {

class handler;
class container;
class connection;
class message;

/** An event for the proton::messaging_handler */
class messaging_event : public proton_event
{
  public:

    std::string name() const;

    // TODO aconway 2015-07-16: document meaning of each event type.

    /** Event types for a messaging_handler */
    enum event_type {
        PROTON = 0,  // Wrapped pn_event_t
        ABORT,
        ACCEPTED,
        COMMIT,
        CONNECTION_CLOSED,
        CONNECTION_CLOSING,
        CONNECTION_ERROR,
        CONNECTION_OPENED,
        CONNECTION_OPENING,
        DISCONNECTED,
        FETCH,
        ID_LOADED,
        LINK_CLOSED,
        LINK_CLOSING,
        LINK_OPENED,
        LINK_OPENING,
        LINK_ERROR,
        MESSAGE,
        QUIT,
        RECORD_INSERTED,
        RECORDS_LOADED,
        REJECTED,
        RELEASED,
        REQUEST,
        RESPONSE,
        SENDABLE,
        SESSION_CLOSED,
        SESSION_CLOSING,
        SESSION_OPENED,
        SESSION_OPENING,
        SESSION_ERROR,
        SETTLED,
        START,
        TIMER,
        TRANSACTION_ABORTED,
        TRANSACTION_COMMITTED,
        TRANSACTION_DECLARED,
        TRANSPORT_CLOSED
    };

    messaging_event(pn_event_t *, proton_event::event_type, class event_loop *);
    messaging_event(event_type t, proton_event &parent);
    ~messaging_event();

    virtual PN_CPP_EXTERN void dispatch(handler &h);
    virtual PN_CPP_EXTERN class connection connection() const;
    virtual PN_CPP_EXTERN class sender sender() const;
    virtual PN_CPP_EXTERN class receiver receiver() const;
    virtual PN_CPP_EXTERN class link link() const;
    virtual PN_CPP_EXTERN class delivery delivery() const;
    virtual PN_CPP_EXTERN class message& message() const;

    PN_CPP_EXTERN event_type type() const;

  private:
  friend class messaging_adapter;
    event_type type_;
    proton_event *parent_event_;
    class message *message_;
    messaging_event operator=(const messaging_event&);
    messaging_event(const messaging_event&);
};

}

#endif  /*!PROTON_CPP_MESSAGINGEVENT_H*/
