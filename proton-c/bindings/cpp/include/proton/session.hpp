#ifndef PROTON_CPP_SESSION_H
#define PROTON_CPP_SESSION_H

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
#include "proton/endpoint.hpp"
#include "proton/receiver.hpp"
#include "proton/sender.hpp"

#include "proton/types.h"
#include "proton/link.h"
#include <string>

struct pn_connection_t;
struct pn_session_t;

namespace proton {

class container;
class handler;

/// A container of senders and receivers.
class
PN_CPP_CLASS_EXTERN session : public internal::object<pn_session_t>, public endpoint
{
  public:
    /// @cond INTERNAL
    session(pn_session_t* s=0) : internal::object<pn_session_t>(s) {}
    /// @endcond

    // Endpoint behaviours

    /// Get the state of this session.
    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    /// @cond INTERNAL
    /// XXX needs to take connection options
    /// Initiate local open.  The operation is not complete till
    /// handler::on_session_open().
    PN_CPP_EXTERN void open();
    /// @endcond

    /// Initiate local close.  The operation is not complete till
    /// handler::on_session_close().
    PN_CPP_EXTERN void close();

    /// Initiate close with an error condition.
    /// The operation is not complete till handler::on_connection_close().
    PN_CPP_EXTERN void close(const error_condition&);


    /// Return the container for this session
    PN_CPP_EXTERN class container &container() const;

    /// Get the connection this session belongs to.
    PN_CPP_EXTERN class connection connection() const;

    /// Open a sender for `addr`.
    PN_CPP_EXTERN sender open_sender(const std::string &addr, const sender_options &opts = sender_options());

    /// Open a receiver for `addr`.
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr, const receiver_options &opts = receiver_options());

    /// Return the senders on this session.
    PN_CPP_EXTERN sender_range senders() const;

    /// Return the receivers on this session.
    PN_CPP_EXTERN receiver_range receivers() const;

  friend class sender_iterator;
  friend class receiver_iterator;
  friend class session_iterator;
};


/// An iterator for sessions.
class session_iterator : public internal::iter_base<session, session_iterator> {
 public:
    ///@cond INTERNAL
    explicit session_iterator(session s = 0) : internal::iter_base<session, session_iterator>(s) {}
    ///@endcond
    PN_CPP_EXTERN session_iterator operator++();     ///< Advance
};

/// A range of sessions.
typedef internal::iter_range<session_iterator> session_range;

}

#endif // PROTON_CPP_SESSION_H
