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

#include <proton/endpoint.hpp>
#include <proton/export.hpp>
#include <proton/message.hpp>
#include <proton/terminus.hpp>

#include <proton/object.hpp>
#include <proton/link_options.hpp>

#include <proton/types.h>

#include <string>

namespace proton {

class sender;
class receiver;
class condition;

/// A named channel for sending or receiving messages.  It is the base
/// class for sender and receiver.
class
PN_CPP_CLASS_EXTERN link : public internal::object<pn_link_t> , public endpoint {
    /// @cond INTERNAL
    link(pn_link_t* l) : internal::object<pn_link_t>(l) {}
    /// @endcond

  public:
    link() : internal::object<pn_link_t>(0) {}

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

    /// Suspend the link without closing it.  A suspended link may be
    /// reopened with the same or different link options if supported by
    /// the peer. A suspended durable subscriptions becomes inactive
    /// without cancelling it.
    PN_CPP_EXTERN void detach();

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
    /// XXX flush() ? drain, and drain_completed (sender and receiver ends)
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

    /// XXX local versus remote, mutability
    /// XXX - local_sender_settle_mode and local_receiver_settle_mode
    PN_CPP_EXTERN link_options::sender_settle_mode sender_settle_mode();
    PN_CPP_EXTERN link_options::receiver_settle_mode receiver_settle_mode();
    PN_CPP_EXTERN link_options::sender_settle_mode remote_sender_settle_mode();
    PN_CPP_EXTERN link_options::receiver_settle_mode remote_receiver_settle_mode();

  private:
    // Used by link_options
    void handler(proton_handler &);
    void detach_handler();
    void sender_settle_mode(link_options::sender_settle_mode);
    void receiver_settle_mode(link_options::receiver_settle_mode);
    // Used by message to decode message from a delivery
    ssize_t recv(char* buffer, size_t size);
    bool advance();

  friend class connection;
  friend class delivery;
  friend class receiver;
  friend class sender;
  friend class message;
  friend class proton_event;
  friend class link_iterator;
  friend class link_options;
};

/// An iterator for links.
class link_iterator : public internal::iter_base<link, link_iterator> {
  public:
    explicit link_iterator(link l = 0, pn_session_t* s = 0) :
        internal::iter_base<link, link_iterator>(l), session_(s) {}
    PN_CPP_EXTERN link_iterator operator++();

  private:
    pn_session_t* session_;
};

/// A range of links.
typedef internal::iter_range<link_iterator> link_range;

}

#include <proton/sender.hpp>
#include <proton/receiver.hpp>

#endif // PROTON_CPP_LINK_H
