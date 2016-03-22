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

namespace io {
class connection_engine;
}

class transport : public internal::object<pn_transport_t> {
    /// @cond INTERNAL
    transport(pn_transport_t* t) : internal::object<pn_transport_t>(t) {}
    /// @endcond 

  public:
    transport() : internal::object<pn_transport_t>(0) {}

    /// Get the connection associated with this transport.
    PN_CPP_EXTERN class connection connection() const;

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
    friend class connection;
    friend class connection_options;
    friend class connector;
    friend class proton_event;
    friend class io::connection_engine;
    friend class messaging_adapter;
    /// @endcond
};

}

#endif // PROTON_CPP_TRANSPORT_H
