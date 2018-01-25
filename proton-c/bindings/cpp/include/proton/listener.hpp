#ifndef PROTON_LISTENER_HPP
#define PROTON_LISTENER_HPP

/*
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
 */

#include "./internal/export.hpp"

/// @file
/// @copybrief proton::listener

struct pn_listener_t;

namespace proton {

/// A listener for incoming connections.
class PN_CPP_CLASS_EXTERN listener {
    /// @cond INTERNAL
    listener(pn_listener_t*);
    /// @endcond

  public:
    /// Create an empty listener.
    PN_CPP_EXTERN listener();

    /// Copy a listener.
    PN_CPP_EXTERN listener(const listener&);

    PN_CPP_EXTERN ~listener();

    /// Copy a listener.
    PN_CPP_EXTERN listener& operator=(const listener&);

    /// Stop listening on the address provided to the call to
    /// container::listen that returned this listener.
    PN_CPP_EXTERN void stop();

    /// **Unsettedled API**
    ///
    /// Return the port used by the listener.
    /// If port 0 was passed to container::listen, this will be a dynamically allocated port.
    /// @throw proton::error if the listener does not have a port
    PN_CPP_EXTERN int port();

    /// **Unsettedled API**
    ///
    /// Get the container.
    /// @throw proton::error if this listener is not managed by a container.
    PN_CPP_EXTERN class container& container() const;

  private:
    pn_listener_t* listener_;

  friend class container;
};

} // proton

#endif // PROTON_LISTENER_HPP
