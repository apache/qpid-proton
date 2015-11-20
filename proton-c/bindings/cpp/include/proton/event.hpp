#ifndef PROTON_CPP_EVENT_H
#define PROTON_CPP_EVENT_H

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
#include "proton/export.hpp"
#include "proton/link.hpp"
#include "proton/connection.hpp"
#include "proton/message.hpp"
#include <vector>
#include <string>

namespace proton {

class handler;
class container;
class connection;

/** Context information about a proton event */
class event {
  public:
    virtual PN_CPP_EXTERN ~event();

    /// Dispatch this event to a handler.
    virtual PN_CPP_EXTERN void dispatch(handler &h) = 0;

    /// Return the name of the event type
    virtual PN_CPP_EXTERN std::string name() const = 0;

    /// Get the event_loop object, can be a container or an engine.
    virtual PN_CPP_EXTERN class event_loop& event_loop() const;

    /// Get the container, throw an exception if event_loop is not a container.
    virtual PN_CPP_EXTERN class container& container() const;

    /// Get the engine, , throw an exception if event_loop is not an engine.
    virtual PN_CPP_EXTERN class engine& engine() const;

    /// Get connection.
    virtual PN_CPP_EXTERN class connection connection() const;
    /// Get sender @throws error if no sender.
    virtual PN_CPP_EXTERN class sender sender() const;
    /// Get receiver @throws error if no receiver.
    virtual PN_CPP_EXTERN class receiver receiver() const;
    /// Get link @throws error if no link.
    virtual PN_CPP_EXTERN class link link() const;
    /// Get delivery @throws error if no delivery.
    virtual PN_CPP_EXTERN class delivery delivery() const;
    /** Get message @throws error if no message. */
    virtual PN_CPP_EXTERN class message &message() const;

  protected:
    PN_CPP_EXTERN event();

  private:
    event(const event&);
    event& operator=(const event&);
};


}

#endif  /*!PROTON_CPP_EVENT_H*/
