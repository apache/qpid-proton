#ifndef PROTON_CPP_HANDLER_H
#define PROTON_CPP_HANDLER_H

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
#include "proton/counted_ptr.hpp"
#include "proton/event.hpp"
#include "proton/event.h"
#include "proton/reactor.h"
#include <vector>

namespace proton {

/** Base class for event handlers.
 *
 * A handler can have child handlers which are called in order, after the parent handler.
 *
 * Note: handlers are not deleted automatically. They must not be deleted while
 * they are still in use.
 *
 * There are two simple strategies you can use:
 *
 * 1. Destroy handlers only after the container that uses them is closed.
 *
 * 2. Allocate handlers with `new` and call `delete this` in the appropriate
 * `on_*_closed` or `on_*_final` event that indicates the handler is no longer needed.
 *
 */
class handler : public std::vector<handler*> {
  public:
    PN_CPP_EXTERN handler();
    PN_CPP_EXTERN virtual ~handler();

    /// Called if a handler function is not over-ridden to handle an event.
    PN_CPP_EXTERN virtual void on_unhandled(event &e);

    /// Add a child handler, equivalent to this->push_back(&h)
    /// h must not be deleted before this handler.
    PN_CPP_EXTERN virtual void add_child_handler(handler &h);

  private:
    counted_ptr<pn_handler_t> pn_handler_;
    friend class container_impl;
};

}

#endif  /*!PROTON_CPP_HANDLER_H*/
