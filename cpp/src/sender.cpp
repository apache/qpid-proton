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

#include "proton/sender.hpp"

#include "proton/link.hpp"
#include "proton/sender_options.hpp"
#include "proton/source.hpp"
#include "proton/target.hpp"
#include "proton/tracker.hpp"

#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/types.h>

#include "proton_bits.hpp"
#include "contexts.hpp"

#include <assert.h>

namespace proton {

sender::sender(pn_link_t *l): link(make_wrapper(l)) {}

sender::~sender() {}

void sender::open() {
    attach();
}

void sender::open(const sender_options &opts) {
    opts.apply(*this);
    attach();
}

class source sender::source() const {
    return proton::source(*this);
}

class target sender::target() const {
    return proton::target(*this);
}

namespace {
// TODO: revisit if thread safety required
uint64_t tag_counter = 0;
}

tracker sender::send(const message &message) {
    uint64_t id = ++tag_counter;
    pn_delivery_t *dlv =
        pn_delivery(pn_object(), pn_dtag(reinterpret_cast<const char*>(&id), sizeof(id)));
    std::vector<char> buf;
    message.encode(buf);
    assert(!buf.empty());
    pn_link_send(pn_object(), &buf[0], buf.size());
    pn_link_advance(pn_object());
    if (pn_link_snd_settle_mode(pn_object()) == PN_SND_SETTLED)
        pn_delivery_settle(dlv);
    if (!pn_link_credit(pn_object()))
        link_context::get(pn_object()).draining = false;
    return make_wrapper<tracker>(dlv);
}

void sender::return_credit() {
    link_context &lctx = link_context::get(pn_object());
    lctx.draining = false;
    pn_link_drained(pn_object());
}

sender_iterator sender_iterator::operator++() {
    if (!!obj_) {
        pn_link_t *lnk = pn_link_next(obj_.pn_object(), 0);
        while (lnk) {
            if (pn_link_is_sender(lnk) && pn_link_session(lnk) == session_)
                break;
            lnk = pn_link_next(lnk, 0);
        }
        obj_ = lnk;
    }
    return *this;
}

}
