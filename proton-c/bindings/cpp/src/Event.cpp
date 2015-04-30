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

#include "proton/cpp/Event.h"
#include "proton/cpp/Handler.h"
#include "proton/cpp/exceptions.h"

#include "Msg.h"
#include "contexts.h"

namespace proton {
namespace reactor {

Event::Event() {}

Event::~Event() {}


Container &Event::getContainer() {
    // Subclasses to override as appropriate
    throw ProtonException(MSG("No container context for event"));
}

Connection &Event::getConnection() {
    throw ProtonException(MSG("No connection context for Event"));
}

Sender Event::getSender() {
    throw ProtonException(MSG("No Sender context for event"));
}

Receiver Event::getReceiver() {
    throw ProtonException(MSG("No Receiver context for event"));
}

Link Event::getLink() {
    throw ProtonException(MSG("No Link context for event"));
}

Message Event::getMessage() {
    throw ProtonException(MSG("No message associated with event"));
}

void Event::setMessage(Message &) {
    throw ProtonException(MSG("Operation not supported for this type of event"));
}



}} // namespace proton::reactor
