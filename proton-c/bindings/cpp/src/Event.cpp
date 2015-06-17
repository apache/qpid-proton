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

#include "proton/reactor.h"
#include "proton/event.h"

#include "proton/Event.hpp"
#include "proton/Handler.hpp"
#include "proton/Error.hpp"

#include "Msg.hpp"
#include "contexts.hpp"

namespace proton {
namespace reactor {

Event::Event() {}

Event::~Event() {}


Container &Event::getContainer() {
    // Subclasses to override as appropriate
    throw Error(MSG("No container context for event"));
}

Connection &Event::getConnection() {
    throw Error(MSG("No connection context for Event"));
}

Sender Event::getSender() {
    throw Error(MSG("No Sender context for event"));
}

Receiver Event::getReceiver() {
    throw Error(MSG("No Receiver context for event"));
}

Link Event::getLink() {
    throw Error(MSG("No Link context for event"));
}

Message Event::getMessage() {
    throw Error(MSG("No message associated with event"));
}

void Event::setMessage(Message &) {
    throw Error(MSG("Operation not supported for this type of event"));
}



}} // namespace proton::reactor
