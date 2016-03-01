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

#include "proton/reconnect_timer.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "proton/types.h"
#include "proton/reactor.h"

namespace proton {

reconnect_timer::reconnect_timer(uint32_t first, int32_t max, uint32_t increment,
                                 bool doubling, int32_t max_retries, int32_t timeout) :
    first_delay_(first), max_delay_(max), increment_(increment), doubling_(doubling),
    max_retries_(max_retries), timeout_(timeout), retries_(0), next_delay_(-1), timeout_deadline_(0)
    {}

void reconnect_timer::reset() {
    retries_ = 0;
    next_delay_ = 0;
    timeout_deadline_ = 0;
}

int reconnect_timer::next_delay(timestamp now) {
    retries_++;
    if (max_retries_ >= 0 && retries_ > max_retries_)
        return -1;

    if (retries_ == 1) {
        if (timeout_ >= duration(0))
            timeout_deadline_ = now + timeout_;
        next_delay_ = first_delay_;
    } else if (retries_ == 2) {
        next_delay_ = next_delay_ + increment_;
    } else {
        next_delay_ = next_delay_ + ( doubling_ ? next_delay_ : increment_ );
    }
    if (timeout_deadline_ != timestamp(0) && now >= timeout_deadline_)
        return -1;
    if (max_delay_ >= duration(0) && next_delay_ > max_delay_)
        next_delay_ = max_delay_;
    if (timeout_deadline_ != timestamp(0) && (now + next_delay_ > timeout_deadline_))
        next_delay_ = timeout_deadline_ - now;
    return next_delay_.ms();
}

}
