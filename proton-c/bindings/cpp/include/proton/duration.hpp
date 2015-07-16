#ifndef PROTON_CPP_DURATION_H
#define PROTON_CPP_DURATION_H

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
#include "proton/types.hpp"

namespace proton {

/** Duration in milliseconds. */
class duration : public comparable<duration>
{
  public:
    std::uint64_t milliseconds;
    explicit duration(std::uint64_t ms) : milliseconds(ms) {}

    bool operator<(duration d) { return milliseconds < d.milliseconds; }
    bool operator==(duration d) { return milliseconds == d.milliseconds; }

    PN_CPP_EXTERN static const duration FOREVER; ///< Wait for ever
    PN_CPP_EXTERN static const duration IMMEDIATE; ///< Don't wait at all
    PN_CPP_EXTERN static const duration SECOND; ///< One second
    PN_CPP_EXTERN static const duration MINUTE; ///< One minute
};

inline duration operator*(duration d, std::uint64_t n) { return duration(d.milliseconds*n); }
inline duration operator*(std::uint64_t n, duration d) { return d * n; }

inline amqp_timestamp operator+(amqp_timestamp ts, duration d) { return amqp_timestamp(ts.milliseconds+d.milliseconds); }
inline amqp_timestamp operator+(duration d, amqp_timestamp ts) { return ts + d; }
}

#endif  /*!PROTON_CPP_DURATION_H*/
