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
#include "proton/delivery.hpp"
#include "proton/link.hpp"
#include "proton/message.hpp"

#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

/// A link for sending messages.
class
PN_CPP_CLASS_EXTERN sender : public link
{
    /// @cond INTERNAL
    sender(pn_link_t* s) : link(s) {}
    /// @endcond

  public:
    sender() : link(0) {}

    /// Send a message on the link.
    PN_CPP_EXTERN delivery send(const message &m);

    /// @cond INTERNAL
    /// XXX undiscussed

    /// The number of deliveries that might be able to be sent if
    /// sufficient credit were issued on the link.  See
    /// sender::offered().  Maintained by the application.
    PN_CPP_EXTERN int available();

    /// Set the availability of deliveries for a sender.
    PN_CPP_EXTERN void offered(int c);

    /// @endcond

  /// @cond INTERNAL
  friend class link;
  friend class session;
  /// @endcond
};

}

#endif // PROTON_CPP_SENDER_H
