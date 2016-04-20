#ifndef PROTON_CPP_SENDER_H
#define PROTON_CPP_SENDER_H

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
#include "proton/link.hpp"
#include "proton/message.hpp"
#include "proton/tracker.hpp"

#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

/// A link for sending messages.
class
PN_CPP_CLASS_EXTERN sender : public internal::link
{
    /// @cond INTERNAL
    sender(pn_link_t* s);
    /// @endcond

  public:
    sender() {}

    /// Locally open the sender.  The operation is not complete till
    /// handler::on_sender_open.
    PN_CPP_EXTERN void open(const sender_options &opts = sender_options());

    /// Send a message on the sender.
    PN_CPP_EXTERN tracker send(const message &m);

    /// Get the source node.
    PN_CPP_EXTERN class source source() const;

    /// Get the target node.
    PN_CPP_EXTERN class target target() const;

  /// @cond INTERNAL
  friend class internal::factory<sender>;
  friend class sender_iterator;
  friend class source;
  friend class target;
  /// @endcond
};

class sender_iterator : public internal::iter_base<sender, sender_iterator> {
    ///@cond INTERNAL
    sender_iterator(sender snd, pn_session_t* s = 0) :
        iter_base<sender, sender_iterator>(snd), session_(s) {}
    ///@endcond

  public:
    sender_iterator() :
        iter_base<sender, sender_iterator>(0), session_(0) {}
    /// Advance
    PN_CPP_EXTERN sender_iterator operator++();

  private:
    pn_session_t* session_;

    friend class connection;
    friend class session;
};

/// A range of senders.
typedef internal::iter_range<sender_iterator> sender_range;


}

#endif // PROTON_CPP_SENDER_H
