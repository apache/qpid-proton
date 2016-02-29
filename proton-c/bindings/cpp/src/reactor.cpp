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

#include "reactor.hpp"

#include "proton/acceptor.hpp"
#include "proton/connection.hpp"
#include "proton/task.hpp"
#include "proton/url.hpp"

#include "contexts.hpp"

#include <proton/reactor.h>

namespace proton {

reactor reactor::create() {
    return internal::take_ownership(pn_reactor()).get();
}

void reactor::run() { pn_reactor_run(pn_object()); }
void reactor::start() { pn_reactor_start(pn_object()); }
bool reactor::process() { return pn_reactor_process(pn_object()); }
void reactor::stop() { pn_reactor_stop(pn_object()); }
void reactor::wakeup() { pn_reactor_wakeup(pn_object()); }
bool reactor::quiesced() { return pn_reactor_quiesced(pn_object()); }
void reactor::yield() { pn_reactor_yield(pn_object()); }
timestamp reactor::mark() { return timestamp(pn_reactor_mark(pn_object())); }
timestamp reactor::now() { return timestamp(pn_reactor_now(pn_object())); }

acceptor reactor::listen(const url& url){
    return pn_reactor_acceptor(pn_object(), url.host().c_str(), url.port().c_str(), 0);
}

task reactor::schedule(int delay, pn_handler_t* handler) {
    return pn_reactor_schedule(pn_object(), delay, handler);
}

connection reactor::connection(pn_handler_t* h) const {
    return pn_reactor_connection(pn_object(), h);
}

pn_io_t* reactor::pn_io() const {
    return pn_reactor_io(pn_object());
}

void reactor::pn_handler(pn_handler_t* h) {
    pn_reactor_set_handler(pn_object(), h);
}

pn_handler_t* reactor::pn_handler() const {
    return pn_reactor_get_handler(pn_object());
}

void reactor::pn_global_handler(pn_handler_t* h) {
    pn_reactor_set_global_handler(pn_object(), h);
}

pn_handler_t* reactor::pn_global_handler() const {
    return pn_reactor_get_global_handler(pn_object());
}

duration reactor::timeout() {
    pn_millis_t tmo = pn_reactor_get_timeout(pn_object());
    if (tmo == PN_MILLIS_MAX)
        return duration::FOREVER;
    return duration(tmo);
}

void reactor::timeout(duration timeout) {
    if (timeout == duration::FOREVER || timeout.ms() > PN_MILLIS_MAX)
        pn_reactor_set_timeout(pn_object(), PN_MILLIS_MAX);
    else
        pn_reactor_set_timeout(pn_object(), timeout.ms());
}

}
