/*
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
 */

#include "proton/connection_options.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"

#include <proton/listener.h>
#include <proton/netaddr.h>

#include "contexts.hpp"

#include <stdlib.h>

namespace proton {

listener::listener(): listener_(0) {}
listener::listener(pn_listener_t* l): listener_(l) {}
// Out-of-line big-3 with trivial implementations, in case we need them in future.
listener::listener(const listener& l) : listener_(l.listener_) {}
listener::~listener() {}
listener& listener::operator=(const listener& l) { listener_ = l.listener_; return *this; }

void listener::stop() {
    if (listener_) {
        pn_listener_close(listener_);
        listener_ = 0;          // Further calls to stop() are no-op
    }
}

int listener::port() {
    if (!listener_) throw error("listener is closed");
    char port[16] = "";
    pn_netaddr_host_port(pn_listener_addr(listener_), NULL, 0, port, sizeof(port));
    int i = atoi(port);
    if (!i) throw error("listener has no port");
    return i;
}

class container& listener::container() const {
    if (!listener_) throw error("listener is closed");
    void *c = pn_listener_get_context(listener_);
    if (!c) throw proton::error("no container");
    return *reinterpret_cast<class container*>(c);
}

// Listen handler
listen_handler::~listen_handler() {}
void listen_handler::on_open(listener&) {}
connection_options listen_handler::on_accept(listener&) { return connection_options(); }
void listen_handler::on_error(listener&, const std::string& what)  { throw proton::error(what); }
void listen_handler::on_close(listener&) {}
}
