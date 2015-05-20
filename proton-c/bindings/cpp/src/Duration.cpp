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
#include "proton/cpp/Duration.h"
#include <limits>

namespace proton {
namespace reactor {

Duration::Duration(uint64_t ms) : milliseconds(ms) {}
uint64_t Duration::getMilliseconds() const { return milliseconds; }

Duration operator*(const Duration& duration, uint64_t multiplier)
{
    return Duration(duration.getMilliseconds() * multiplier);
}

Duration operator*(uint64_t multiplier, const Duration& duration)
{
    return Duration(duration.getMilliseconds() * multiplier);
}

bool operator==(const Duration& a, const Duration& b)
{
    return a.getMilliseconds() == b.getMilliseconds();
}

bool operator!=(const Duration& a, const Duration& b)
{
    return a.getMilliseconds() != b.getMilliseconds();
}

const Duration Duration::FOREVER(std::numeric_limits<uint64_t>::max());
const Duration Duration::IMMEDIATE(0);
const Duration Duration::SECOND(1000);
const Duration Duration::MINUTE(SECOND * 60);

}} // namespace proton::reactor
