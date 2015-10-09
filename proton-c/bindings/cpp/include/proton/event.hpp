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

    /// Get container.
    virtual PN_CPP_EXTERN class container &container();
    /// Get connection.
    virtual PN_CPP_EXTERN class connection &connection();
    /// Get sender @throws error if no sender.
    virtual PN_CPP_EXTERN class sender& sender();
    /// Get receiver @throws error if no receiver.
    virtual PN_CPP_EXTERN class receiver& receiver();
    /// Get link @throws error if no link.
    virtual PN_CPP_EXTERN class link& link();
    /// Get delivey @throws error if no delivery.
    virtual PN_CPP_EXTERN class delivery& delivery();
    /** Get message @throws error if no message. */
    virtual PN_CPP_EXTERN class message &message();

  protected:
    PN_CPP_EXTERN event();

  private:
    event(const event&);
    event& operator=(const event&);
};


}

#endif  /*!PROTON_CPP_EVENT_H*/
