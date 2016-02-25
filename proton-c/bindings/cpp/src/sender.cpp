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
#include "proton/link.hpp"
#include "proton/sender.hpp"
#include "proton/error.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"
#include "proton/types.h"
#include "proton/codec.h"
#include "proton/message.h"
#include "proton/delivery.h"
#include <stdlib.h>
#include <string.h>

namespace proton {

namespace {
// TODO: revisit if thread safety required
uint64_t tag_counter = 0;
}

delivery sender::send(const message &message) {
    uint64_t id = ++tag_counter;
    pn_delivery_t *dlv =
        pn_delivery(pn_object(), pn_dtag(reinterpret_cast<const char*>(&id), sizeof(id)));
    std::vector<char> buf;
    message.encode(buf);
    pn_link_send(pn_object(), &buf[0], buf.size());
    pn_link_advance(pn_object());
    if (pn_link_snd_settle_mode(pn_object()) == PN_SND_SETTLED)
        pn_delivery_settle(dlv);
    return dlv;
}

int sender::available() { return pn_link_available(pn_object()); }
void sender::offered(int c) { pn_link_offered(pn_object(), c); }


}
