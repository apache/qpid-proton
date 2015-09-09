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

#include "proton/reactor.hpp"

#include <proton/reactor.h>

namespace proton {

pn_unique_ptr<reactor> reactor::create() {
    return pn_unique_ptr<reactor>(reactor::cast(pn_reactor()));
}

void reactor::run() { pn_reactor_run(pn_cast(this)); }
void reactor::start() { pn_reactor_start(pn_cast(this)); }
bool reactor::process() { return pn_reactor_process(pn_cast(this)); }
void reactor::stop() { pn_reactor_stop(pn_cast(this)); }
void reactor::wakeup() { pn_reactor_wakeup(pn_cast(this)); }
bool reactor::quiesced() { return pn_reactor_quiesced(pn_cast(this)); }
void reactor::yield() { pn_reactor_yield(pn_cast(this)); }

duration reactor::timeout() {
    pn_millis_t tmo = pn_reactor_get_timeout(pn_cast(this));
    if (tmo == PN_MILLIS_MAX)
        return duration::FOREVER;
    return duration(tmo);
}

void reactor::timeout(duration timeout) {
    if (timeout == duration::FOREVER || timeout.milliseconds > PN_MILLIS_MAX)
        pn_reactor_set_timeout(pn_cast(this), PN_MILLIS_MAX);
    else
        pn_reactor_set_timeout(pn_cast(this), timeout.milliseconds);
}


void reactor::operator delete(void* p) {
    pn_reactor_free(reinterpret_cast<pn_reactor_t*>(p));
}

}
