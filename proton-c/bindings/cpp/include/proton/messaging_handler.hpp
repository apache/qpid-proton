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

#include "proton/proton_handler.hpp"
#include "proton/event.h"

#include <stdexcept>

namespace proton {

class event;
class messaging_adapter;

class messaging_exception : public std::runtime_error {
  public:
    messaging_exception(event& e);
};

/** messaging_handler base class. Provides a simpler set of events than
 * proton::proton_handler and automates some common tasks.  Subclass and
 * over-ride event handling member functions.
 * @see proton::messaging_event for meaning of events.
 */
class messaging_handler
{
  public:
    /** Create a messaging_handler
     *@param prefetch set flow control to automatically pre-fetch this many messages
     *@param auto_accept automatically accept received messages after on_message()
     *@param auto_settle automatically settle on receipt of delivery for sent messages.
     *@param peer_close_is_error treat orderly remote connection close as error.
     */
    PN_CPP_EXTERN messaging_handler(int prefetch=10, bool auto_accept=true, bool auto_settle=true,
                                    bool peer_close_is_error=false);

    PN_CPP_EXTERN virtual ~messaging_handler();

    ///@name Over-ride these member functions to handle events
    ///@{
    PN_CPP_EXTERN virtual void on_start(event &e);
    PN_CPP_EXTERN virtual void on_message(event &e);
    PN_CPP_EXTERN virtual void on_sendable(event &e);
    PN_CPP_EXTERN virtual void on_disconnect(event &e);

    PN_CPP_EXTERN virtual void on_connection_open(event &e);
    PN_CPP_EXTERN virtual void on_connection_close(event &e);
    PN_CPP_EXTERN virtual void on_connection_error(event &e);

    PN_CPP_EXTERN virtual void on_session_open(event &e);
    PN_CPP_EXTERN virtual void on_session_close(event &e);
    PN_CPP_EXTERN virtual void on_session_error(event &e);

    PN_CPP_EXTERN virtual void on_link_open(event &e);
    PN_CPP_EXTERN virtual void on_link_close(event &e);
    PN_CPP_EXTERN virtual void on_link_error(event &e);

    PN_CPP_EXTERN virtual void on_delivery_accept(event &e);
    PN_CPP_EXTERN virtual void on_delivery_reject(event &e);
    PN_CPP_EXTERN virtual void on_delivery_release(event &e);
    PN_CPP_EXTERN virtual void on_delivery_settle(event &e);

    PN_CPP_EXTERN virtual void on_transaction_declare(event &e);
    PN_CPP_EXTERN virtual void on_transaction_commit(event &e);
    PN_CPP_EXTERN virtual void on_transaction_abort(event &e);

    PN_CPP_EXTERN virtual void on_timer(event &e);

    PN_CPP_EXTERN virtual void on_unhandled(event &e);
    PN_CPP_EXTERN virtual void on_unhandled_error(event &e);
    ///@}

  private:
    pn_unique_ptr<messaging_adapter> messaging_adapter_;
    friend class container;
    friend class connection_engine;
    friend class connection_options;
    friend class link_options;
};

}

#endif  /*!PROTON_CPP_MESSAGING_HANDLER_H*/
