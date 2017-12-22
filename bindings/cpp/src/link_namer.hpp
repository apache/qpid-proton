#ifndef PROTON_IO_LINK_NAMER_HPP
#define PROTON_IO_LINK_NAMER_HPP

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

#include "proton/internal/export.hpp"
#include <string>

namespace proton {

class connection;

namespace io {

/// **Unsettled API** - Generate default link names that are unique
/// within a container.  base_container provides a default
/// implementation.
class link_namer {
  public:
    virtual ~link_namer() {}

    /// Generate a unique link name.
    virtual std::string link_name() = 0;
};

/// **Unsettled API** - Set the link_namer to use on a connection.
PN_CPP_EXTERN void set_link_namer(connection&, link_namer&);

} // io
} // proton

#endif // PROTON_IO_LINK_NAMER_HPP
