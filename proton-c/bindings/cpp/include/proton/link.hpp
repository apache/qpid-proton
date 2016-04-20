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
#include <proton/source.hpp>
#include <proton/target.hpp>
#include <proton/object.hpp>
#include <proton/sender_options.hpp>
#include <proton/receiver_options.hpp>

#include <proton/types.h>

#include <string>

namespace proton {

class sender;
class receiver;
class error_condition;
class link_context;
class proton_event;
class messaging_adapter;
class proton_handler;
class delivery;
class connection;
class container;
class session;
class sender_iterator;
class receiver_iterator;

namespace internal {

/// A named channel for sending or receiving messages.  It is the base
/// class for sender and receiver.
class
PN_CPP_CLASS_EXTERN link : public object<pn_link_t> , public endpoint {
    /// @cond INTERNAL
    link(pn_link_t* l) : object<pn_link_t>(l) {}
    /// @endcond

  public:
    link() : object<pn_link_t>(0) {}

    // Endpoint behaviours
    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    /// Locally close the link.  The operation is not complete till
    /// handler::on_link_close.
    PN_CPP_EXTERN void close();

    /// Initiate close with an error condition.
    /// The operation is not complete till handler::on_connection_close().
    PN_CPP_EXTERN void close(const error_condition&);

    /// Suspend the link without closing it.  A suspended link may be
    /// reopened with the same or different link options if supported by
    /// the peer. A suspended durable subscriptions becomes inactive
    /// without cancelling it.
    PN_CPP_EXTERN void detach();

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

    /// Get the link name.
    PN_CPP_EXTERN std::string name() const;

    /// Return the container for this link
    PN_CPP_EXTERN class container &container() const;

    /// Connection that owns this link.
    PN_CPP_EXTERN class connection connection() const;

    /// Session that owns this link.
    PN_CPP_EXTERN class session session() const;

    ///@cond INTERNAL
  protected:
    /// Initiate the AMQP attach frame.  The operation is not complete till
    /// handler::on_link_open.
    void attach();

  private:
    // Used by sender/receiver options
    void handler(proton_handler &);
    void detach_handler();
    void sender_settle_mode(sender_options::sender_settle_mode);
    void receiver_settle_mode(receiver_options::receiver_settle_mode);
    // Used by message to decode message from a delivery
    ssize_t recv(char* buffer, size_t size);
    bool advance();
    link_context &context();

    /// XXX local versus remote, mutability
    /// XXX - local_sender_settle_mode and local_receiver_settle_mode
    sender_options::sender_settle_mode sender_settle_mode();
    receiver_options::receiver_settle_mode receiver_settle_mode();
    sender_options::sender_settle_mode remote_sender_settle_mode();
    receiver_options::receiver_settle_mode remote_receiver_settle_mode();

    friend class factory<link>;
    friend class proton::message;
    friend class proton::receiver_options;
    friend class proton::sender_options;
    ///@endcond
};

}}

#endif // PROTON_CPP_LINK_H
