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

#include "contexts.h"

namespace proton {
namespace cpp {
namespace reactor {

Event::Event() {}

Event::~Event() {}


Container &Event::getContainer() { 
    // Subclasses to override as appropriate
    throw "some real exception: No container context for event";
}

Connection &Event::getConnection() {
    throw "some real exception";
}

Sender Event::getSender() {
    throw "some real exception";
}

Receiver Event::getReceiver() {
    throw "some real exception";
}

Link Event::getLink() {
    throw "some real exception";
}

Message Event::getMessage() {
    throw "some real exception";
}

void Event::setMessage(Message &) {
    throw "some real exception";
}



}}} // namespace proton::cpp::reactor
