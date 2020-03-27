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

#include <proton/connect_config.hpp>
#include "connect_config.hpp"

#include <proton/error.hpp>

namespace proton {
namespace {
const error nope("connection configuration is not supported");
}

class connection_options;

std::string apply_config(connection_options&) { throw nope; }

namespace connect_config {
std::string default_file() { throw nope; }
std::string parse(std::istream& is, connection_options& opts)  { throw nope; }
std::string parse_default(proton::connection_options&)  { throw nope; }

}} // namespace proton::connect_config
