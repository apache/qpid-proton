#ifndef PROTON_CONNECT_CONFIG_HPP
#define PROTON_CONNECT_CONFIG_HPP

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

#include "./internal/export.hpp"

#include <iosfwd>
#include <string>

namespace proton {

class connection_options;

/// *Unsettled API*
///
/// Functions for locating and using a @ref connect-config file, or a
/// configuration string to set @ref connection_options
namespace connect_config {

/// @return name of the default @ref connect-config file.
/// @throw proton::error if no default file is found
PN_CPP_EXTERN std::string default_file();

/// Parse @ref connect-config from @p is and update @p opts
/// @param is input stream for configuration file/string
/// @param opts [out] connection options to update
/// @return address suitable for container::connect() from configuration
PN_CPP_EXTERN std::string parse(std::istream& is, connection_options& opts);

/// Parse @ref connect-config from default_file() and update @p opts
/// @param opts [out] connection options to update
/// @return address suitable for container::connect() from configuration
PN_CPP_EXTERN std::string parse_default(connection_options& opts);

}} // namespace proton::connect_config

#endif // CONNECT_CONFIG_HPP
