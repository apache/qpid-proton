#ifndef PROTON_SENDER_HPP
#define PROTON_SENDER_HPP

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
#include "./link.hpp"
#include "./tracker.hpp"

/// @file
/// @copybrief proton::sender

struct pn_link_t;
struct pn_session_t;

namespace proton {

/// A channel for sending messages.
class
PN_CPP_CLASS_EXTERN sender : public link {
    /// @cond INTERNAL
    PN_CPP_EXTERN sender(pn_link_t* s);
    /// @endcond

  public:
    /// Create an empty sender.
    sender() {}

    PN_CPP_EXTERN ~sender();

    /// Open the sender.
    PN_CPP_EXTERN void open();

    /// @copydoc open
    PN_CPP_EXTERN void open(const sender_options &opts);

    /// Send a message on the sender.
    PN_CPP_EXTERN tracker send(const message &m);

    /// Get the source node.
    PN_CPP_EXTERN class source source() const;

    /// Get the target node.
    PN_CPP_EXTERN class target target() const;

    /// **Unsettled API** - Return all unused credit to the receiver in
    /// response to a drain request.  Has no effect unless there has
    /// been a drain request and there is remaining credit to use or
    /// return.
    ///
    /// @see receiver::drain
    PN_CPP_EXTERN void return_credit();

    /// @cond INTERNAL
  friend class internal::factory<sender>;
  friend class sender_iterator;
    /// @endcond
};

/// @cond INTERNAL

/// An iterator of senders.
class sender_iterator : public internal::iter_base<sender, sender_iterator> {
    sender_iterator(sender snd, pn_session_t* s = 0) :
        internal::iter_base<sender, sender_iterator>(snd), session_(s) {}

  public:
    /// Create an iterator of senders.
    sender_iterator() :
        internal::iter_base<sender, sender_iterator>(0), session_(0) {}
    /// Advance to the next sender.
    PN_CPP_EXTERN sender_iterator operator++();

  private:
    pn_session_t* session_;

  friend class connection;
  friend class session;
};

/// A range of senders.
typedef internal::iter_range<sender_iterator> sender_range;

/// @endcond

}

#endif // PROTON_SENDER_HPP
