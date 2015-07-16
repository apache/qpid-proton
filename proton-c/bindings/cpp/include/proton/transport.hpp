#ifndef PROTON_CPP_TRANSPORT_H
#define PROTON_CPP_TRANSPORT_H

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
#include "proton/transport.h"
#include <string>

struct pn_connection_t;

namespace proton {

class connection;

/** Represents a connection transport */
class transport
{
  public:
    PN_CPP_EXTERN transport();
    PN_CPP_EXTERN ~transport();

    /** Bind the transport to an AMQP connection */
    PN_CPP_EXTERN void bind(connection &c);

    class connection* connection() const { return connection_; }
    pn_transport_t* pn_transport() const { return pn_transport_; }

  private:
    class connection *connection_;
    pn_transport_t *pn_transport_;
};


}

#endif  /*!PROTON_CPP_TRANSPORT_H*/
