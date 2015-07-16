#ifndef PROTON_CPP_CONNECTIONIMPL_H
#define PROTON_CPP_CONNECTIONIMPL_H

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
#include "proton/endpoint.hpp"
#include "proton/container.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

class handler;
class container;
class ssl_domain;

 class blocking_connection_impl : public messaging_handler
{
  public:
    PN_CPP_EXTERN blocking_connection_impl(const url &url, duration d, ssl_domain *ssld, container *c);
    PN_CPP_EXTERN ~blocking_connection_impl();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN void wait(wait_condition &condition);
    PN_CPP_EXTERN void wait(wait_condition &condition, std::string &msg, duration timeout);
    PN_CPP_EXTERN pn_connection_t *pn_blocking_connection();
    duration timeout() { return timeout_; }
    static void incref(blocking_connection_impl *);
    static void decref(blocking_connection_impl *);
  private:
    friend class blocking_connection;
    container container_;
    connection connection_;
    url url_;
    duration timeout_;
    int refcount_;
};


}

#endif  /*!PROTON_CPP_CONNECTIONIMPL_H*/
