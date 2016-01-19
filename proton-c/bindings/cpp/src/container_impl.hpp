#ifndef PROTON_CPP_CONTAINERIMPL_H
#define PROTON_CPP_CONTAINERIMPL_H

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
#include "proton/handler.hpp"
#include "proton/connection.hpp"
#include "proton/link.hpp"
#include "proton/duration.hpp"
#include "proton/reactor.hpp"

#include "proton_handler.hpp"

#include "proton/reactor.h"

#include <string>

namespace proton {

class dispatch_helper;
class connection;
class connector;
class acceptor;
class container;
class url;
class task;

class container_impl
{
  public:
    PN_CPP_EXTERN container_impl(container&, messaging_adapter*, const std::string& id);
    PN_CPP_EXTERN ~container_impl();
    PN_CPP_EXTERN connection connect(const url&, const connection_options&);
    PN_CPP_EXTERN sender open_sender(const url&, const proton::link_options &, const connection_options &);
    PN_CPP_EXTERN receiver open_receiver(const url&, const proton::link_options &, const connection_options &);
    PN_CPP_EXTERN class acceptor listen(const url&, const connection_options &);
    PN_CPP_EXTERN duration timeout();
    PN_CPP_EXTERN void timeout(duration timeout);
    void client_connection_options(const connection_options &);
    const connection_options& client_connection_options() { return client_connection_options_; }
    void server_connection_options(const connection_options &);
    const connection_options& server_connection_options() { return server_connection_options_; }
    void link_options(const proton::link_options&);
    const proton::link_options& link_options() { return link_options_; }

    void configure_server_connection(connection &c);
    task schedule(int delay, proton_handler *h);
    pn_ptr<pn_handler_t> cpp_handler(proton_handler *h);

    std::string next_link_name();

  private:

    container& container_;
    reactor reactor_;
    proton_handler *handler_;
    pn_unique_ptr<proton_handler> override_handler_;
    pn_unique_ptr<proton_handler> flow_controller_;
    std::string id_;
    uint64_t link_id_;
    connection_options client_connection_options_;
    connection_options server_connection_options_;
    proton::link_options link_options_;

  friend class container;
};

}

#endif  /*!PROTON_CPP_CONTAINERIMPL_H*/
