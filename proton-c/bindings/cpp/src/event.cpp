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

#include "proton/delivery.hpp"
#include "proton/engine.hpp"
#include "proton/error.hpp"
#include "proton/event.hpp"
#include "proton/handler.hpp"
#include "proton/receiver.hpp"
#include "proton/sender.hpp"

#include "msg.hpp"
#include "contexts.hpp"

namespace proton {

event::event() {}

event::~event() {}

event_loop& event::event_loop() const {
    throw error(MSG("No event_loop context for event"));
}

container& event::container() const {
    // Subclasses to override as appropriate
    throw error(MSG("No container context for event"));
}

engine& event::engine() const {
    // Subclasses to override as appropriate
    throw error(MSG("No engine context for event"));
}

connection event::connection() const {
    throw error(MSG("No connection context for event"));
}

sender event::sender() const {
    throw error(MSG("No sender context for event"));
}

receiver event::receiver() const {
    throw error(MSG("No receiver context for event"));
}

link event::link() const {
    throw error(MSG("No link context for event"));
}

delivery event::delivery() const {
    throw error(MSG("No link context for event"));
}

class message &event::message() const {
    throw error(MSG("No message associated with event"));
}

}
