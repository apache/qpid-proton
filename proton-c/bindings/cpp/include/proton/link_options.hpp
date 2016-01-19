#ifndef PROTON_CPP_LINK_OPTIONS_H
#define PROTON_CPP_LINK_OPTIONS_H

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
#include "proton/config.hpp"
#include "proton/export.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/types.hpp"
#include "proton/terminus.hpp"

#include <vector>
#include <string>

namespace proton {

/** The message delivery policy to establish when opening the link. */
enum link_delivery_mode_t {
    // No set policy.  The application must settle messages itself according to its own policy.
    NONE = 0,
    // Outgoing messages are settled immediately by the link.  There are no duplicates.
    AT_MOST_ONCE,
    // The receiver settles the delivery first with an accept/reject/release disposition.
    // The sender waits to settle until after the disposition notification is received.
    AT_LEAST_ONCE
};

/** The lifetime of dynamically created nodes. */
enum lifetime_policy_t {
    // The policy is unspecified.
    UNSPECIFIED = 0,
    // The lifetime of the dynamic node is scoped to lifetime of the creating link.
    DELETE_ON_CLOSE = 0x2B,
    // The node will be deleted when it is neither the source nor the target of any link.
    DELETE_ON_NO_LINKS = 0x2C,
    // The node will be deleted when the creating link no longer exists and no messages remain at the node.
    DELETE_ON_NO_MESSAGES = 0x2D,
    // The node will be deleted when there are no links which have this node as
    // their source or target, and there remain no messages at the node.
    DELETE_ON_NO_LINKS_OR_MESSAGES = 0x2E
};

class proton_handler;
class link;

/** Options for creating a link.
 *
 * Options can be "chained" like this:
 *
 * l = container.create_sender(url, link_options().handler(h).browsing(true));
 *
 * You can also create an options object with common settings and use it as a base
 * for different connections that have mostly the same settings:
 *
 * link_options opts;
 * opts.browsing(true);
 * l1 = container.open_sender(url1, opts.handler(h1));
 * c2 = container.open_receiver(url2, opts.handler(h2));
 *
 * Normal value semantics, copy or assign creates a separate copy of the options.
 */
class link_options {
  public:
    PN_CPP_EXTERN link_options();
    PN_CPP_EXTERN link_options(const link_options&);
    PN_CPP_EXTERN ~link_options();
    PN_CPP_EXTERN link_options& operator=(const link_options&);

    /// Override with options from other.
    PN_CPP_EXTERN void override(const link_options& other);

    /** Set a handler for events scoped to the link.  If NULL, link-scoped events on the link are discarded. */
    PN_CPP_EXTERN link_options& handler(class handler *);
    /** Receiver-only option to specify whether messages are browsed or
        consumed.  Setting browsing to true is Equivalent to setting
        distribution_mode(COPY).  Setting browsing to false is equivalent to
        setting distribution_mode(MOVE). */
    PN_CPP_EXTERN link_options& browsing(bool);
    /** Set the distribution mode for message transfer.  See terminus::distribution_mode_t. */
    PN_CPP_EXTERN link_options& distribution_mode(terminus::distribution_mode_t);
    /* Receiver-only option to create a durable subsription on the receiver.
       Equivalent to setting the terminus durability to termins::DELIVERIES and
       the expiry policy to terminus::EXPIRE_NEVER. */
    PN_CPP_EXTERN link_options& durable_subscription(bool);
    /* Set the delivery mode on the link. */
    PN_CPP_EXTERN link_options& delivery_mode(link_delivery_mode_t);
    /* Request a dynamically generated node at the peer. */
    PN_CPP_EXTERN link_options& dynamic_address(bool);
    /* Set the lifetime policy for a receiver to a dynamically created node. */
    PN_CPP_EXTERN link_options& lifetime_policy(lifetime_policy_t);
    /* Set the local address for the link. */
    PN_CPP_EXTERN link_options& local_address(const std::string &addr);
    /* Set a selector on the receiver to str.  This sets a single registered filter on the link of
       type apache.org:selector-filter with value str. */
    PN_CPP_EXTERN link_options& selector(const std::string &str);

  private:
    friend class link;
    void apply(link&) const;
    proton_handler* handler() const;

    class impl;
    pn_unique_ptr<impl> impl_;
};

} // namespace

#endif  /*!PROTON_CPP_LINK_OPTIONS_H*/
