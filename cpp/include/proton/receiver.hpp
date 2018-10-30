#ifndef PROTON_RECEIVER_HPP
#define PROTON_RECEIVER_HPP

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

#include <proton/type_compat.h>

struct pn_link_t;
struct pn_session_t;

/// @file
/// @copybrief proton::receiver

namespace proton {

/// A channel for receiving messages.
class
PN_CPP_CLASS_EXTERN receiver : public link {
    /// @cond INTERNAL
    PN_CPP_EXTERN receiver(pn_link_t* r);
    /// @endcond

  public:
    /// Create an empty receiver.
    receiver() {}

    PN_CPP_EXTERN ~receiver();

    /// Open the receiver.
    PN_CPP_EXTERN void open();

    /// @copydoc open
    PN_CPP_EXTERN void open(const receiver_options &opts);

    /// Get the source node.
    PN_CPP_EXTERN class source source() const;

    /// Get the target node.
    PN_CPP_EXTERN class target target() const;

    /// Increment the credit available to the sender.  Credit granted
    /// during a drain cycle is not communicated to the receiver until
    /// the drain completes.
    PN_CPP_EXTERN void add_credit(uint32_t);

    /// **Unsettled API** - Commence a drain cycle.  If there is
    /// positive credit, a request is sent to the sender to
    /// immediately use up all of the existing credit balance by
    /// sending messages that are immediately available and releasing
    /// any unused credit (see sender::return_credit).  Throws
    /// proton::error if a drain cycle is already in progress.  An
    /// on_receiver_drain_finish event will be generated when the
    /// outstanding drained credit reaches zero.
    PN_CPP_EXTERN void drain();

    /// @cond INTERNAL
  friend class internal::factory<receiver>;
  friend class receiver_iterator;
    /// @endcond
};

/// @cond INTERNAL

/// An iterator of receivers.
class receiver_iterator : public internal::iter_base<receiver, receiver_iterator> {
    explicit receiver_iterator(receiver r, pn_session_t* s = 0) :
        internal::iter_base<receiver, receiver_iterator>(r), session_(s) {}

  public:
    /// Create an iterator of receivers.
    explicit receiver_iterator() :
        internal::iter_base<receiver, receiver_iterator>(0), session_(0) {}

    /// Advance to the next receiver.
    PN_CPP_EXTERN receiver_iterator operator++();

  private:
    pn_session_t* session_;

  friend class connection;
  friend class session;
};

/// A range of receivers.
typedef internal::iter_range<receiver_iterator> receiver_range;

/// @endcond

} // proton

#endif // PROTON_RECEIVER_HPP
