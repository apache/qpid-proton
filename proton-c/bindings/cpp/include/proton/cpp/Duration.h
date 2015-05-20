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

#include "proton/cpp/ImportExport.h"
#include "proton/types.h"

namespace proton {
namespace reactor {

/**   \ingroup C++
 * A duration is a time in milliseconds.
 */
class Duration
{
  public:
    PROTON_CPP_EXTERN explicit Duration(uint64_t milliseconds);
    PROTON_CPP_EXTERN uint64_t getMilliseconds() const;
    PROTON_CPP_EXTERN static const Duration FOREVER;
    PROTON_CPP_EXTERN static const Duration IMMEDIATE;
    PROTON_CPP_EXTERN static const Duration SECOND;
    PROTON_CPP_EXTERN static const Duration MINUTE;
  private:
    uint64_t milliseconds;
};

PROTON_CPP_EXTERN Duration operator*(const Duration& duration,
                                         uint64_t multiplier);
PROTON_CPP_EXTERN Duration operator*(uint64_t multiplier,
                                         const Duration& duration);
PROTON_CPP_EXTERN bool operator==(const Duration& a, const Duration& b);
PROTON_CPP_EXTERN bool operator!=(const Duration& a, const Duration& b);

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_DURATION_H*/
