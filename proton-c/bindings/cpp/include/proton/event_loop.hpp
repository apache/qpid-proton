#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP
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

#include "proton/export.hpp"

#include <string>

namespace proton {

/**
 * An event_loop dispatches events to event handlers.  event_loop is an abstract
 * class, concrete subclasses are proton::container and prton::engine.
 */
class event_loop {
  public:
    PN_CPP_EXTERN virtual ~event_loop() {}

    /// The AMQP container-id associated with this event loop.
    /// Any connections managed by this event loop will have this container id.
    PN_CPP_EXTERN  virtual std::string id() const = 0;

    // TODO aconway 2015-11-02: injecting application events.
};

}
#endif // EVENT_LOOP_HPP
