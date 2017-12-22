#ifndef PROTON_LINK_HPP
#define PROTON_LINK_HPP

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
#include "./internal/object.hpp"

#include <string>

/// @file
/// @copybrief proton::link

struct pn_link_t;

namespace proton {

/// A named channel for sending or receiving messages.  It is the base
/// class for sender and receiver.
class
PN_CPP_CLASS_EXTERN link : public internal::object<pn_link_t> , public endpoint {
    /// @cond INTERNAL
    link(pn_link_t* l) : internal::object<pn_link_t>(l) {}
    /// @endcond

  public:
    /// Create an empty link.
    link() : internal::object<pn_link_t>(0) {}

    PN_CPP_EXTERN bool uninitialized() const;
    PN_CPP_EXTERN bool active() const;
    PN_CPP_EXTERN bool closed() const;

    PN_CPP_EXTERN class error_condition error() const;

    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN void close(const error_condition&);

    /// Suspend the link without closing it.  A suspended link may be
    /// reopened with the same or different link options if supported
    /// by the peer. A suspended durable subscription becomes inactive
    /// without cancelling it.
    // XXX Should take error condition
    PN_CPP_EXTERN void detach();

    /// Credit available on the link.
    PN_CPP_EXTERN int credit() const;

    /// **Unsettled API** - True for a receiver if a drain cycle has
    /// been started and the corresponding `on_receiver_drain_finish`
    /// event is still pending.  True for a sender if the receiver has
    /// requested a drain of credit and the sender has unused credit.
    ///
    /// @see @ref receiver::drain. 
    PN_CPP_EXTERN bool draining();

    /// Get the link name.
    PN_CPP_EXTERN std::string name() const;

    /// The container for this link.
    PN_CPP_EXTERN class container &container() const;

    /// Get the work_queue for the link.
    PN_CPP_EXTERN class work_queue& work_queue() const;

    /// The connection that owns this link.
    PN_CPP_EXTERN class connection connection() const;

    /// The session that owns this link.
    PN_CPP_EXTERN class session session() const;

  protected:
    /// @cond INTERNAL
    
    // Initiate the AMQP attach frame.
    void attach();

  friend class internal::factory<link>;

    /// @endcond
};

}

#endif // PROTON_LINK_HPP
