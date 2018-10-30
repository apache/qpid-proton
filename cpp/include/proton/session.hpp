#ifndef PROTON_SESSION_HPP
#define PROTON_SESSION_HPP

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

#include "./fwd.hpp"
#include "./internal/export.hpp"
#include "./endpoint.hpp"
#include "./receiver.hpp"
#include "./sender.hpp"

#include <string>

/// @file
/// @copybrief proton::session

struct pn_session_t;

namespace proton {

/// A container of senders and receivers.
class
PN_CPP_CLASS_EXTERN session : public internal::object<pn_session_t>, public endpoint {
  public:
    /// @cond INTERNAL
    PN_CPP_EXTERN session(pn_session_t* s) : internal::object<pn_session_t>(s) {}
    /// @endcond

  public:
    /// Create an empty session.
    session() : internal::object<pn_session_t>(0) {}

    PN_CPP_EXTERN ~session();

    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    /// Open the session.
    PN_CPP_EXTERN void open();

    /// @copydoc open
    PN_CPP_EXTERN void open(const session_options &opts);

    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN void close(const error_condition&);

    /// Get the container for this session.
    PN_CPP_EXTERN class container &container() const;

    /// Get the work_queue for the session.
    PN_CPP_EXTERN class work_queue& work_queue() const;

    /// Get the connection this session belongs to.
    PN_CPP_EXTERN class connection connection() const;

    /// Open a sender for `addr`.
    PN_CPP_EXTERN sender open_sender(const std::string &addr);

    /// @copydoc open_sender
    PN_CPP_EXTERN sender open_sender(const std::string &addr, const sender_options &opts);

    /// Open a receiver for `addr`.
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr);

    /// @copydoc open_receiver
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr, const receiver_options &opts);

    /// The number of incoming bytes currently buffered.
    PN_CPP_EXTERN size_t incoming_bytes() const;

    /// The number of outgoing bytes currently buffered.
    PN_CPP_EXTERN size_t outgoing_bytes() const;

    /// Return the senders on this session.
    PN_CPP_EXTERN sender_range senders() const;

    /// Return the receivers on this session.
    PN_CPP_EXTERN receiver_range receivers() const;

    /// @cond INTERNAL
  friend class internal::factory<session>;
  friend class session_iterator;
    /// @endcond
};

/// @cond INTERNAL
    
/// An iterator of sessions.
class session_iterator : public internal::iter_base<session, session_iterator> {
 public:
    explicit session_iterator(session s = 0) : internal::iter_base<session, session_iterator>(s) {}

    /// Advance to the next session.
    PN_CPP_EXTERN session_iterator operator++();
};

/// A range of sessions.
typedef internal::iter_range<session_iterator> session_range;

/// @endcond
    
} // proton

#endif // PROTON_SESSION_HPP
