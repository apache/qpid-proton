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
#include "proton/messaging_handler.hpp"
#include "proton/connection.hpp"
#include "proton/link.hpp"
#include "proton/duration.hpp"

#include "proton/reactor.h"

#include <string>
namespace proton {

class dispatch_helper;
class connection;
class connector;
class acceptor;

class container_impl
{
  public:
    PN_CPP_EXTERN container_impl(handler &h);
    PN_CPP_EXTERN container_impl();
    PN_CPP_EXTERN ~container_impl();
    PN_CPP_EXTERN connection connect(const url&, handler *h);
    PN_CPP_EXTERN void run();
    PN_CPP_EXTERN pn_reactor_t *reactor();
    PN_CPP_EXTERN sender create_sender(connection &connection, const std::string &addr, handler *h);
    PN_CPP_EXTERN sender create_sender(const url&);
    PN_CPP_EXTERN receiver create_receiver(connection &connection, const std::string &addr, bool dynamic, handler *h);
    PN_CPP_EXTERN receiver create_receiver(const url&);
    PN_CPP_EXTERN class acceptor listen(const url&);
    PN_CPP_EXTERN std::string container_id();
    PN_CPP_EXTERN duration timeout();
    PN_CPP_EXTERN void timeout(duration timeout);
    void start();
    bool process();
    void stop();
    void wakeup();
    bool is_quiesced();
    void yield();
    pn_handler_t *wrap_handler(handler *h);
    static void incref(container_impl *);
    static void decref(container_impl *);
  private:
    void dispatch(pn_event_t *event, pn_event_type_t type);
    class acceptor acceptor(const url&);
    void initialize_reactor();
    pn_reactor_t *reactor_;
    handler *handler_;
    messaging_adapter *messaging_adapter_;
    handler *override_handler_;
    handler *flow_controller_;
    std::string container_id_;
    int refcount_;
};


}

#endif  /*!PROTON_CPP_CONTAINERIMPL_H*/
