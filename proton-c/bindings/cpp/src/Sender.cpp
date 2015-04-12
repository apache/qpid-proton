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
#include "proton/cpp/Link.h"
#include "proton/cpp/Sender.h"
#include "contexts.h"

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
namespace cpp {
namespace reactor {


Sender::Sender(pn_link_t *lnk) : Link(lnk, true) {}

namespace{
// revisit if thread safety required
uint64_t tagCounter = 0;
}

void Sender::send(Message &message) {
    char tag[8];
    void *ptr = &tag;
    uint64_t id = tagCounter++;
    *((uint64_t *) ptr) = id;
    pn_delivery_t *dlv = pn_delivery(getPnLink(), pn_dtag(tag, 8));
    std::string buf;
    message.encode(buf);
    pn_link_send(getPnLink(), buf.data(), buf.size());
    pn_delivery_settle(dlv);
}

}}} // namespace proton::cpp::reactor
