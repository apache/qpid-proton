#ifndef PROTON_CPP_ENDPOINT_H
#define PROTON_CPP_ENDPOINT_H

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
#include "proton/connection.h"

namespace proton {

class handler;
class connection;
class transport;

/** endpoint is a base class for session, connection and link */
class endpoint
{
  public:
    /** state is a bit mask of state_bit values.
     *
     * A state mask is matched against an endpoint as follows: If the state mask
     * contains both local and remote flags, then an exact match against those
     * flags is performed. If state contains only local or only remote flags,
     * then a match occurs if any of the local or remote flags are set
     * respectively.
     *
     * @see connection::link_head, connection::session_head, link::next, session::next
     */
    typedef int state;

    /// endpoint state bit values @{
    static const int LOCAL_UNINIT;    ///< Local endpoint is un-initialized
    static const int REMOTE_UNINIT;   ///< Remote endpoint is un-initialized
    static const int LOCAL_ACTIVE;    ///< Local endpoint is active
    static const int REMOTE_ACTIVE;   ///< Remote endpoint is active
    static const int LOCAL_CLOSED;    ///< Local endpoint has been closed
    static const int REMOTE_CLOSED;   ///< Remote endpoint has been closed
    static const int LOCAL_MASK;      ///< Mask including all LOCAL_ bits (UNINIT, ACTIVE, CLOSED)
    static const int REMOTE_MASK;     ///< Mask including all REMOTE_ bits (UNINIT, ACTIVE, CLOSED)
     ///@}

    // TODO: condition, remote_condition, update_condition, get/handler
};


}

#endif  /*!PROTON_CPP_ENDPOINT_H*/
