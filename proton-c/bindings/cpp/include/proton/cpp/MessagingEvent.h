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
#include "proton/cpp/ProtonEvent.h"
#include "proton/cpp/Link.h"

namespace proton {
namespace reactor {

class Handler;
class Container;
class Connection;

typedef enum {
    PN_MESSAGING_PROTON = 0,  // Wrapped pn_event_t
    // Covenience events for C++ MessagingHandlers
    PN_MESSAGING_ABORT,
    PN_MESSAGING_ACCEPTED,
    PN_MESSAGING_COMMIT,
    PN_MESSAGING_CONNECTION_CLOSE,
    PN_MESSAGING_CONNECTION_CLOSED,
    PN_MESSAGING_CONNECTION_CLOSING,
    PN_MESSAGING_CONNECTION_OPEN,
    PN_MESSAGING_CONNECTION_OPENED,
    PN_MESSAGING_DISCONNECTED,
    PN_MESSAGING_FETCH,
    PN_MESSAGING_ID_LOADED,
    PN_MESSAGING_LINK_CLOSING,
    PN_MESSAGING_LINK_OPENED,
    PN_MESSAGING_LINK_OPENING,
    PN_MESSAGING_MESSAGE,
    PN_MESSAGING_QUIT,
    PN_MESSAGING_RECORD_INSERTED,
    PN_MESSAGING_RECORDS_LOADED,
    PN_MESSAGING_REJECTED,
    PN_MESSAGING_RELEASED,
    PN_MESSAGING_REQUEST,
    PN_MESSAGING_RESPONSE,
    PN_MESSAGING_SENDABLE,
    PN_MESSAGING_SETTLED,
    PN_MESSAGING_START,
    PN_MESSAGING_TIMER,
    PN_MESSAGING_TRANSACTION_ABORTED,
    PN_MESSAGING_TRANSACTION_COMMITTED,
    PN_MESSAGING_TRANSACTION_DECLARED
} MessagingEventType_t;

class MessagingEvent : public ProtonEvent
{
  public:
    MessagingEvent(pn_event_t *ce, pn_event_type_t t, Container &c);
    MessagingEvent(MessagingEventType_t t, ProtonEvent *parent, Container &c);
    ~MessagingEvent();
    virtual PROTON_CPP_EXTERN void dispatch(Handler &h);
    virtual PROTON_CPP_EXTERN Connection &getConnection();
    virtual PROTON_CPP_EXTERN Sender getSender();
    virtual PROTON_CPP_EXTERN Receiver getReceiver();
    virtual PROTON_CPP_EXTERN Link getLink();
    virtual PROTON_CPP_EXTERN Message getMessage();
    virtual PROTON_CPP_EXTERN void setMessage(Message &);
  private:
    MessagingEventType_t messagingType;
    ProtonEvent *parentEvent;
    Message *message;
    MessagingEvent operator=(const MessagingEvent&);
    MessagingEvent(const MessagingEvent&);
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_MESSAGINGEVENT_H*/
