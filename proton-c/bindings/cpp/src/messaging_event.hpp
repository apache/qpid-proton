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
class messaging_event : public event
{
  public:

    std::string name() const;

    // TODO aconway 2015-07-16: document meaning of each event type.

    /** Event types for a messaging_handler */
    enum event_type {
        START,
        MESSAGE,
        SENDABLE,
        TRANSPORT_CLOSE,
        TRANSPORT_ERROR,
        CONNECTION_OPEN,
        CONNECTION_CLOSE,
        CONNECTION_ERROR,
        LINK_OPEN,
        LINK_CLOSE,
        LINK_ERROR,
        SESSION_OPEN,
        SESSION_CLOSE,
        SESSION_ERROR,
        DELIVERY_ACCEPT,
        DELIVERY_REJECT,
        DELIVERY_RELEASE,
        DELIVERY_SETTLE,
        TRANSACTION_DECLARE,
        TRANSACTION_COMMIT,
        TRANSACTION_ABORT,
        TIMER
    };

    messaging_event(event_type t, proton_event &parent);
    messaging_event(event_type t, pn_event_t*);
    ~messaging_event();

    class container& container() const;
    class transport transport() const;
    class connection connection() const;
    class session session() const;
    class sender sender() const;
    class receiver receiver() const;
    class link link() const;
    class delivery delivery() const;
    class message& message() const;

    event_type type() const;

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
