#ifndef PROTON_CPP_LINK_H
#define PROTON_CPP_LINK_H

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

#include "proton/endpoint.hpp"
#include "proton/export.hpp"
#include "proton/message.hpp"
#include "proton/terminus.hpp"
#include "proton/types.h"
#include "proton/object.hpp"
#include "proton/link_options.hpp"

#include <string>

namespace proton {

class sender;
class receiver;
class condition;

/// A named channel for sending or receiving messages.  It is the base
/// class for sender and receiver.
class link : public object<pn_link_t> , public endpoint {
  public:
    /// @cond INTERNAL
    link(pn_link_t* l=0) : object<pn_link_t>(l) {}
    /// @endcond

    // Endpoint behaviours

    /// Get the state of this link.
    PN_CPP_EXTERN endpoint::state state() const;

    PN_CPP_EXTERN condition local_condition() const;
    PN_CPP_EXTERN condition remote_condition() const;

    /// Locally open the link.  The operation is not complete till
    /// handler::on_link_open.
    PN_CPP_EXTERN void open(const link_options &opts = link_options());

    /// Locally close the link.  The operation is not complete till
    /// handler::on_link_close.
    PN_CPP_EXTERN void close();

    /// Return sender if this link is a sender, 0 if not.
    PN_CPP_EXTERN class sender sender();

    /// Return sender if this link is a sender, 0 if not.
    PN_CPP_EXTERN const class sender sender() const;

    /// Return receiver if this link is a receiver, 0 if not.
    PN_CPP_EXTERN class receiver receiver();

    /// Return receiver if this link is a receiver, 0 if not.
    PN_CPP_EXTERN const class receiver receiver() const;

    /// Credit available on the link.
    PN_CPP_EXTERN int credit() const;

    /// The number of deliveries queued on the link.
    PN_CPP_EXTERN int queued();

    /// @cond INTERNAL
    /// XXX ask about when this is used
    /// The number of unsettled deliveries on the link.
    PN_CPP_EXTERN int unsettled();
    /// @endcond

    /// @cond INTERNAL
    /// XXX revisit mind-melting API inherited from C
    PN_CPP_EXTERN int drained();
    /// @endcond

    /// Local source of the link.
    PN_CPP_EXTERN terminus local_source() const;
    
    /// Local target of the link.
    PN_CPP_EXTERN terminus local_target() const;
    
    /// Remote source of the link.
    PN_CPP_EXTERN terminus remote_source() const;
    
    /// Remote target of the link.
    PN_CPP_EXTERN terminus remote_target() const;

    /// Get the link name.
    PN_CPP_EXTERN std::string name() const;

    /// Connection that owns this link.
    PN_CPP_EXTERN class connection connection() const;

    /// Session that owns this link.
    PN_CPP_EXTERN class session session() const;

    /// @cond INTERNAL
    /// XXX settle open questions
    
    /// Set a custom handler for this link.
    PN_CPP_EXTERN void handler(proton_handler &);

    /// Unset any custom handler.
    PN_CPP_EXTERN void detach_handler();

    /// @cond INTERNAL

    /// XXX ask about use case, revisit names
    /// Get message data from current delivery on link.
    PN_CPP_EXTERN ssize_t recv(char* buffer, size_t size);

    /// XXX ask about use case, revisit names
    /// Advance the link one delivery.
    PN_CPP_EXTERN bool advance();

    /// XXX remove
    /// Navigate the links in a connection - get next link with state.
    PN_CPP_EXTERN link next(endpoint::state) const;

    /// XXX local versus remote, mutability
    PN_CPP_EXTERN link_options::sender_settle_mode sender_settle_mode();
    PN_CPP_EXTERN void sender_settle_mode(link_options::sender_settle_mode);
    PN_CPP_EXTERN link_options::receiver_settle_mode receiver_settle_mode();
    PN_CPP_EXTERN void receiver_settle_mode(link_options::receiver_settle_mode);
    PN_CPP_EXTERN link_options::sender_settle_mode remote_sender_settle_mode();
    PN_CPP_EXTERN link_options::receiver_settle_mode remote_receiver_settle_mode();

    /// @endcond
};

/// @cond INTERNAL
/// XXX important to expose?
/// An iterator for links.
class link_iterator : public iter_base<link> {
  public:
    explicit link_iterator(link p = 0, endpoint::state s = 0) :
        iter_base<link>(p, s), session_(0) {}
    explicit link_iterator(const link_iterator& i, const session& ssn) :
        iter_base<link>(i.ptr_, i.state_), session_(&ssn) {}
    PN_CPP_EXTERN link_iterator operator++();
    link_iterator operator++(int) { link_iterator x(*this); ++(*this); return x; }

  private:
    const session* session_;
};
/// @endcond

/// A range of links.
typedef range<link_iterator> link_range;

}

#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#endif // PROTON_CPP_LINK_H
