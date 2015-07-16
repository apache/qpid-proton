#ifndef PROTON_CPP_ACCEPTOR_H
#define PROTON_CPP_ACCEPTOR_H

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

#include "proton/reactor.h"
#include "proton/export.hpp"
#include "proton/proton_handle.hpp"

struct pn_connection_t;

namespace proton {

/** acceptor accepts connections. @see container::listen */
class acceptor : public proton_handle<pn_acceptor_t>
{
  public:
    PN_CPP_EXTERN acceptor();
    PN_CPP_EXTERN acceptor(pn_acceptor_t *);
    PN_CPP_EXTERN acceptor(const acceptor&);
    PN_CPP_EXTERN acceptor& operator=(const acceptor&);
    PN_CPP_EXTERN ~acceptor();

    /** close the acceptor */
    PN_CPP_EXTERN void close();

  private:
    friend class proton_impl_ref<acceptor>;
};

}

#endif  /*!PROTON_CPP_ACCEPTOR_H*/
