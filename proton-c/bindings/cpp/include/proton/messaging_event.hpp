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
#include "proton/proton_event.hpp"
#include "proton/link.hpp"

namespace proton {

class handler;
class container;
class connection;

typedef enum {
    PN_MESSAGING_PROTON = 0,  // Wrapped pn_event_t
    // Covenience events for C++ messaging_handlers
    PN_MESSAGING_ABORT,
    PN_MESSAGING_ACCEPTED,
    PN_MESSAGING_COMMIT,
    PN_MESSAGING_CONNECTION_CLOSED,
    PN_MESSAGING_CONNECTION_CLOSING,
    PN_MESSAGING_CONNECTION_ERROR,
    PN_MESSAGING_CONNECTION_OPENED,
    PN_MESSAGING_CONNECTION_OPENING,
    PN_MESSAGING_DISCONNECTED,
    PN_MESSAGING_FETCH,
    PN_MESSAGING_Id_LOADED,
    PN_MESSAGING_LINK_CLOSED,
    PN_MESSAGING_LINK_CLOSING,
    PN_MESSAGING_LINK_OPENED,
    PN_MESSAGING_LINK_OPENING,
    PN_MESSAGING_LINK_ERROR,
    PN_MESSAGING_MESSAGE,
    PN_MESSAGING_QUIT,
    PN_MESSAGING_RECORD_INSERTED,
    PN_MESSAGING_RECORDS_LOADED,
    PN_MESSAGING_REJECTED,
    PN_MESSAGING_RELEASED,
    PN_MESSAGING_REQUEST,
    PN_MESSAGING_RESPONSE,
    PN_MESSAGING_SENDABLE,
    PN_MESSAGING_SESSION_CLOSED,
    PN_MESSAGING_SESSION_CLOSING,
    PN_MESSAGING_SESSION_OPENED,
    PN_MESSAGING_SESSION_OPENING,
    PN_MESSAGING_SESSION_ERROR,
    PN_MESSAGING_SETTLED,
    PN_MESSAGING_START,
    PN_MESSAGING_TIMER,
    PN_MESSAGING_TRANSACTION_ABORTED,
    PN_MESSAGING_TRANSACTION_COMMITTED,
    PN_MESSAGING_TRANSACTION_DECLARED,
    PN_MESSAGING_TRANSPORT_CLOSED
} messaging_event_type_t;

class messaging_event : public proton_event
{
  public:
    messaging_event(pn_event_t *ce, pn_event_type_t t, class container &c);
    messaging_event(messaging_event_type_t t, proton_event &parent);
    ~messaging_event();
    virtual PN_CPP_EXTERN void dispatch(handler &h);
    virtual PN_CPP_EXTERN class connection &connection();
    virtual PN_CPP_EXTERN class sender sender();
    virtual PN_CPP_EXTERN class receiver receiver();
    virtual PN_CPP_EXTERN class link link();
    virtual PN_CPP_EXTERN class message message();
    virtual PN_CPP_EXTERN void message(class message &);
  private:
    messaging_event_type_t messaging_type_;
    proton_event *parent_event_;
    class message *message_;
    messaging_event operator=(const messaging_event&);
    messaging_event(const messaging_event&);
};

}

#endif  /*!PROTON_CPP_MESSAGINGEVENT_H*/
