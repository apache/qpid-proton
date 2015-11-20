#ifndef PROTON_CPP_BLOCKING_CONNECTION_H
#define PROTON_CPP_BLOCKING_CONNECTION_H

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

#include "proton/export.hpp"
#include "proton/duration.hpp"
#include "proton/pn_unique_ptr.hpp"

#include <string>

namespace proton {
class url;
class connection;
class blocking_connection_impl;

// TODO documentation
// Note: must not be deleted while there are proton::blocking_link instances that depend on it.
class blocking_connection
{
  public:
    PN_CPP_EXTERN blocking_connection(const proton::url &url, duration timeout = duration::FOREVER);
    PN_CPP_EXTERN ~blocking_connection();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN duration timeout() const;
    PN_CPP_EXTERN class connection connection() const;

  private:
    blocking_connection(const blocking_connection&);
    blocking_connection& operator=(const blocking_connection&);

    pn_unique_ptr<blocking_connection_impl> impl_;

  friend class blocking_link;
  friend class blocking_sender;
  friend class blocking_receiver;
  friend class request_response;
};

}

#endif  /*!PROTON_CPP_BLOCKING_CONNECTION_H*/
