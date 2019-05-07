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

#include "proton/reconnect_options.hpp"
#include "reconnect_options_impl.hpp"

namespace proton {

reconnect_options::reconnect_options() : impl_(new impl()) {}
reconnect_options::reconnect_options(const reconnect_options& x) : impl_(new impl) {
    *this = x;
}
reconnect_options::~reconnect_options() {}

reconnect_options& reconnect_options::operator=(const reconnect_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

reconnect_options& reconnect_options::delay(duration d) { impl_->delay = d; return *this; }
reconnect_options& reconnect_options::delay_multiplier(float f) { impl_->delay_multiplier = f; return *this; }
reconnect_options& reconnect_options::max_delay(duration d) { impl_->max_delay = d; return *this; }
reconnect_options& reconnect_options::max_attempts(int i) { impl_->max_attempts = i; return *this; }
reconnect_options& reconnect_options::failover_urls(const std::vector<std::string>& urls) { impl_->failover_urls = urls; return *this; }

} // namespace proton
