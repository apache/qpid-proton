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

#include "id_generator.hpp"

#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/duration.hpp"
#include "proton/export.hpp"
#include "proton/handler.hpp"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/reactor.h"
#include "reactor.hpp"
#include "proton_handler.hpp"

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
    container_impl(container&, messaging_adapter*, const std::string& id);
    ~container_impl();
    connection connect(const url&, const connection_options&);
    sender open_sender(const url&, const proton::sender_options &, const connection_options &);
    receiver open_receiver(const url&, const proton::receiver_options &, const connection_options &);
    class acceptor listen(const url&, const connection_options &);
    duration timeout();
    void timeout(duration timeout);
    void client_connection_options(const connection_options &);
    const connection_options& client_connection_options() { return client_connection_options_; }
    void server_connection_options(const connection_options &);
    const connection_options& server_connection_options() { return server_connection_options_; }
    void sender_options(const proton::sender_options&);
    const proton::sender_options& sender_options() { return sender_options_; }
    void receiver_options(const proton::receiver_options&);
    const proton::receiver_options& receiver_options() { return receiver_options_; }

    void configure_server_connection(connection &c);
    task schedule(int delay, proton_handler *h);
    internal::pn_ptr<pn_handler_t> cpp_handler(proton_handler *h);

    std::string next_link_name();

  private:

    container& container_;
    reactor reactor_;
    proton_handler *handler_;
    internal::pn_unique_ptr<proton_handler> override_handler_;
    internal::pn_unique_ptr<proton_handler> flow_controller_;
    std::string id_;
    id_generator id_gen_;
    connection_options client_connection_options_;
    connection_options server_connection_options_;
    proton::sender_options sender_options_;
    proton::receiver_options receiver_options_;

  friend class container;
  friend class messaging_adapter;
};

}

#endif  /*!PROTON_CPP_CONTAINERIMPL_H*/
