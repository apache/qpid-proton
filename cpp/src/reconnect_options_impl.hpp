#ifndef PROTON_CPP_RECONNECT_OPTIONSIMPL_H
#define PROTON_CPP_RECONNECT_OPTIONSIMPL_H

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

#include "proton/duration.hpp"
#include "proton/internal/pn_unique_ptr.hpp"
#include "proton/reconnect_options.hpp"

#include <string>
#include <vector>

namespace proton {
class reconnect_options_base {
  public:
    reconnect_options_base() : delay(10), delay_multiplier(2.0), max_delay(duration::FOREVER), max_attempts(0) {}

    duration delay;
    float    delay_multiplier;
    duration max_delay;
    int      max_attempts;
};

class reconnect_options::impl : public reconnect_options_base {
  public:
    std::vector<std::string> failover_urls;
};

}

#endif  /*!PROTON_CPP_RECONNECT_OPTIONSIMPL_H*/
