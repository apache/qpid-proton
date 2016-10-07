#ifndef PROTON_ACCEPTOR_HPP
#define PROTON_ACCEPTOR_HPP

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

#include "proton/internal/export.hpp"
#include "proton/internal/object.hpp"

#include <proton/reactor.h>

struct pn_acceptor_t;

namespace proton {

/// A context for accepting inbound connections.
///
/// @see container::listen
class acceptor : public internal::object<pn_acceptor_t> {
    /// @cond INTERNAL
    acceptor(pn_acceptor_t* a) : internal::object<pn_acceptor_t>(a) {}
    /// @endcond

  public:
    acceptor() : internal::object<pn_acceptor_t>(0) {}

    /// Close the acceptor.
    PN_CPP_EXTERN void close();

    /// Return the current set of connection options applied to
    /// inbound connectons by the acceptor.
    ///
    /// Note that changes made to the connection options only affect
    /// connections accepted after this call returns.
    PN_CPP_EXTERN class connection_options &connection_options();

    /// @cond INTERNAL
  friend class internal::factory<acceptor>;
    /// @endcond
};

} // proton

#endif // PROTON_ACCEPTOR_HPP
