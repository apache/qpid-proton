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

#include "proton/ImportExport.hpp"
#include "proton/types.h"

namespace proton {
namespace reactor {

/** @ingroup cpp
 * A duration is a time in milliseconds.
 */
class Duration
{
  public:
    PN_CPP_EXTERN explicit Duration(uint64_t milliseconds);
    PN_CPP_EXTERN uint64_t getMilliseconds() const;
    PN_CPP_EXTERN static const Duration FOREVER;
    PN_CPP_EXTERN static const Duration IMMEDIATE;
    PN_CPP_EXTERN static const Duration SECOND;
    PN_CPP_EXTERN static const Duration MINUTE;
  private:
    uint64_t milliseconds;
};

PN_CPP_EXTERN Duration operator*(const Duration& duration,
                                         uint64_t multiplier);
PN_CPP_EXTERN Duration operator*(uint64_t multiplier,
                                         const Duration& duration);
PN_CPP_EXTERN bool operator==(const Duration& a, const Duration& b);
PN_CPP_EXTERN bool operator!=(const Duration& a, const Duration& b);

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_DURATION_H*/
