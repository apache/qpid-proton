#ifndef PROTON_CPP_RECONNECT_H
#define PROTON_CPP_RECONNECT_H

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

/// @cond INTERNAL
/// XXX more discussion

#include "proton/export.hpp"
#include "proton/duration.hpp"
#include "proton/timestamp.hpp"
#include "proton/types.hpp"
#include <string>

namespace proton {

/** A class that generates a series of delays to coordinate reconnection attempts.  They may be open ended or limited in time.  They may be evenly spaced or doubling at an exponential rate. */
class reconnect_timer
{
  public:
    /** TODO:
     */
    PN_CPP_EXTERN reconnect_timer(uint32_t first = 0, int32_t max = -1, uint32_t increment = 100,
                                  bool doubling = true, int32_t max_retries = -1, int32_t timeout = -1);

    /** Indicate a successful connection, resetting the internal timer values */
    PN_CPP_EXTERN void reset();

    /** Obtain the timer's computed time to delay before attempting a reconnection attempt (in milliseconds).  -1 means that the retry limit or timeout has been exceeded and reconnection attempts should cease. */
    PN_CPP_EXTERN int next_delay(timestamp now);

  private:
    duration first_delay_;
    duration max_delay_;
    duration increment_;
    bool doubling_;
    int32_t max_retries_;
    duration timeout_;
    int32_t retries_;
    duration next_delay_;
    timestamp timeout_deadline_;

    friend class connector;
};

/// @endcond
}

#endif // PROTON_CPP_RECONNECT_H
