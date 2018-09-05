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
#include "proton/receiver.hpp"

#include "proton/error.hpp"
#include "proton/link.hpp"
#include "proton/receiver_options.hpp"
#include "proton/source.hpp"
#include "proton/target.hpp"

#include "msg.hpp"
#include "proton_bits.hpp"
#include "contexts.hpp"

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <proton/event.h>

namespace proton {

receiver::receiver(pn_link_t* r): link(make_wrapper(r)) {}

receiver::~receiver() {}

void receiver::open() {
    attach();
}

void receiver::open(const receiver_options &opts) {
    opts.apply(*this);
    attach();
}

class source receiver::source() const {
    return proton::source(*this);
}

class target receiver::target() const {
    return proton::target(*this);
}

void receiver::add_credit(uint32_t credit) {
    link_context &ctx = link_context::get(pn_object());
    if (ctx.draining)
        ctx.pending_credit += credit;
    else
        pn_link_flow(pn_object(), credit);
}

void receiver::drain() {
    link_context &ctx = link_context::get(pn_object());
    if (ctx.draining)
        throw proton::error("drain already in progress");
    else {
        ctx.draining = true;
        if (credit() > 0)
            pn_link_set_drain(pn_object(), true);
        else {
            // Drain is already complete.  No state to communicate over the wire.
            // Create dummy flow event where "drain finish" can be detected.
            pn_connection_t *pnc = pn_session_connection(pn_link_session(pn_object()));
            pn_collector_put(pn_connection_collector(pnc), PN_OBJECT, pn_object(), PN_LINK_FLOW);
        }
    }
}

receiver_iterator receiver_iterator::operator++() {
    if (!!obj_) {
        pn_link_t *lnk = pn_link_next(obj_.pn_object(), 0);
        while (lnk) {
            if (pn_link_is_receiver(lnk) && pn_link_session(lnk) == session_)
                break;
            lnk = pn_link_next(lnk, 0);
        }
        obj_ = lnk;
    }
    return *this;
}

}
