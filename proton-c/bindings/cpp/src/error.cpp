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

#include "proton/error.hpp"

namespace proton {

static const std::string prefix("proton: ");

error::error(const std::string& msg) throw() : std::runtime_error(prefix+msg) {}

timeout_error::timeout_error(const std::string& msg) throw() : error(msg) {}

message_reject::message_reject(const std::string& msg) throw() : error(msg) {}

message_release::message_release(const std::string& msg) throw() : error(msg) {}

}
