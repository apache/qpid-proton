#ifndef PROTON_TRANSPORT_HPP
#define PROTON_TRANSPORT_HPP

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
#include "./internal/object.hpp"

/// @file
/// @copybrief proton::transport

struct pn_transport_t;

namespace proton {

/// A network channel supporting an AMQP connection.
class transport : public internal::object<pn_transport_t> {
    /// @cond INTERNAL
    transport(pn_transport_t* t) : internal::object<pn_transport_t>(t) {}
    /// @endcond 

  public:
    /// Create an empty transport.
    transport() : internal::object<pn_transport_t>(0) {}

    /// Get the connection associated with this transport.
    PN_CPP_EXTERN class connection connection() const;

    /// Get SSL information.
    PN_CPP_EXTERN class ssl ssl() const;

    /// Get SASL information.
    PN_CPP_EXTERN class sasl sasl() const;

    /// Get the error condition.
    PN_CPP_EXTERN class error_condition error() const;

    /// @cond INTERNAL
    friend class internal::factory<transport>;
    /// @endcond
};

} // proton

#endif // PROTON_TRANSPORT_HPP
