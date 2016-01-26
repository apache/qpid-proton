#ifndef PROTON_CPP_TRANSPORT_H
#define PROTON_CPP_TRANSPORT_H

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

#include "proton/object.hpp"
#include "proton/types.hpp"
#include "proton/export.hpp"

struct pn_transport_t;

namespace proton {

class connection;
class condition;
class sasl;

/// A network layer supporting an AMQP connection.
class transport : public object<pn_transport_t> {
  public:
    /// @cond INTERNAL
    transport(pn_transport_t* t) : object<pn_transport_t>(t) {}
    /// @endcond 

    /// @cond INTERNAL
    /// XXX what if a transport is associated with multiple connections?
    /// Get the connection associated with this transport.
    PN_CPP_EXTERN class connection connection() const;
    /// @endcond

    /// Get SSL information.
    PN_CPP_EXTERN class ssl ssl() const;

    /// Get SASL information.
    PN_CPP_EXTERN class sasl sasl() const;

    /// Get the error condition.
    PN_CPP_EXTERN class condition condition() const;

    /// @cond INTERNAL
    /// XXX need to discuss, local versus remote
    PN_CPP_EXTERN void unbind();
    PN_CPP_EXTERN void bind(class connection &);
    PN_CPP_EXTERN uint32_t max_frame_size() const;
    PN_CPP_EXTERN uint32_t remote_max_frame_size() const;
    PN_CPP_EXTERN uint16_t max_channels() const;
    PN_CPP_EXTERN uint16_t remote_max_channels() const;
    PN_CPP_EXTERN uint32_t idle_timeout() const;
    PN_CPP_EXTERN uint32_t remote_idle_timeout() const;
    /// @endcond

    /// @cond INTERNAL
    friend class connection_options;
    /// @endcond
};

}

#endif // PROTON_CPP_TRANSPORT_H
