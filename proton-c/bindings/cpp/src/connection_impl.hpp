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
class transport;
class container;

class connection_impl : public endpoint
{
  public:
    PN_CPP_EXTERN connection_impl(class container &c, pn_connection_t &pn_conn);
    PN_CPP_EXTERN connection_impl(class container &c, handler *h = 0);
    PN_CPP_EXTERN virtual ~connection_impl();
    PN_CPP_EXTERN class transport &transport();
    PN_CPP_EXTERN handler *override();
    PN_CPP_EXTERN void override(handler *h);
    PN_CPP_EXTERN void open();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN pn_connection_t *pn_connection();
    PN_CPP_EXTERN class container &container();
    PN_CPP_EXTERN std::string hostname();
    PN_CPP_EXTERN link link_head(endpoint::State mask);
    virtual PN_CPP_EXTERN class connection &connection();
    static class connection &reactor_reference(pn_connection_t *);
    static connection_impl *impl(const class connection &c) { return c.impl_; }
    void reactor_detach();
    static void incref(connection_impl *);
    static void decref(connection_impl *);
  private:
    friend class Connector;
    friend class container_impl;
    class container container_;
    int refcount_;
    handler *override_;
    class transport *transport_;
    pn_session_t *default_session_;  // Temporary, for session_per_connection style policy.
    pn_connection_t *pn_connection_;
    class connection reactor_reference_;   // Keep-alive reference, until PN_CONNECTION_FINAL.
};


}

#endif  /*!PROTON_CPP_CONNECTIONIMPL_H*/
