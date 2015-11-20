#ifndef PROTON_CPP_CONTEXTS_H
#define PROTON_CPP_CONTEXTS_H

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

#include "proton/counted.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/reactor.h"
#include "proton/session.hpp"

#include <proton/message.h>

namespace proton {

class session;
class handler;
class container_impl;

counted* get_context(pn_record_t*, pn_handle_t handle);
void set_context(pn_record_t*, pn_handle_t, counted* value);

struct connection_context : public counted {
    static connection_context& get(pn_connection_t* c);

    connection_context();
    ~connection_context();

    pn_unique_ptr<class handler> handler;
    session default_session;   // Owned by connection
    class container_impl* container_impl;
    message event_message;  // re-used by messaging_adapter for performance
};

class container;
void container_context(pn_reactor_t *, container&);
container& container_context(pn_reactor_t *);

void event_context(pn_event_t *pn_event, pn_message_t *m);
pn_message_t *event_context(pn_event_t *pn_event);

}

#endif  /*!PROTON_CPP_CONTEXTS_H*/
