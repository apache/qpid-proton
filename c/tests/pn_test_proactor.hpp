#ifndef TESTS_PN_TEST_PROACTOR_HPP
#define TESTS_PN_TEST_PROACTOR_HPP

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

/// @file
///
/// Wrapper for driving proactor tests.

#include "./pn_test.hpp"
#include <proton/proactor.h>

namespace pn_test {

// Get the listening port, l must be open.
std::string listening_port(pn_listener_t *l);

// Test proactor with an optional global handler.
// For connection and listener events, if pn_*_get_context() is non-NULL
// then it is cast to a handler and used instead of the global one.
struct proactor : auto_free<pn_proactor_t, pn_proactor_free> {
  struct handler *handler;

  proactor(struct handler *h = 0);

  // Listen on addr using optional listener handler lh
  pn_listener_t *listen(const std::string &addr = ":0", struct handler *lh = 0);

  // Connect to addr use optional handler, and optionally providing the
  // connection object.
  pn_connection_t *connect(const std::string &addr, struct handler *h = 0,
                           pn_connection_t *c = 0);

  // Connect to listenr's address useing optional handler.
  pn_connection_t *connect(pn_listener_t *l, struct handler *h = 0,
                           pn_connection_t *c = 0) {
    return connect(":" + pn_test::listening_port(l), h, c);
  }

  // Accept a connection, associate with optional connection handler.
  pn_connection_t *accept(pn_listener_t *l, struct handler *h = 0);

  // Wait for events and dispatch them until:
  // * A handler returns true.
  // * The `stop` event type is handled.
  // Return the event-type of the last event handled or PN_EVENT_NONE
  // if something went wrong.
  pn_event_type_t run(pn_event_type_t stop = PN_EVENT_NONE);

  // Dispatch immediately-available events until:
  // * A handler returns true.
  // * The `stop` event type is handled.
  // * All available events are flushed.
  //
  // Returns the number of events processed and
  // - PN_EVENT_NONE if all events were handled without stopping
  // - The type of the last event handled otherwise
  //
  std::pair<int, pn_event_type_t> flush(pn_event_type_t stop = PN_EVENT_NONE);

  // Alternate flushing this proactor and `other` until
  // * A handler on this proactor returns true.
  // * The `stop` event type is handled by this proactor.
  // * Both proactors become idle.
  //
  // Return the event-type of the last event handled or PN_EVENT_NONE
  // if the proactors are idle.
  pn_event_type_t corun(proactor &other, pn_event_type_t stop = PN_EVENT_NONE);

  // Wait for and handle a single event, return it's type.
  pn_event_type_t wait_next();

private:
  bool dispatch(pn_event_t *e);
};

// CHECK/REQUIRE macros to run a proactor up to an expected event and
// include the last condition in the error message if the expected event is not
// returned.

#define CHECK_RUN(P, E)                                                        \
  CHECKED_IF((E) == (P).run(E)) {}                                             \
  else if ((P).handler) {                                                      \
    FAIL_CHECK(*(P).handler->last_condition);                                  \
  }

#define REQUIRE_RUN(P, E)                                                      \
  CHECKED_IF((E) == (P).run(E)) {}                                             \
  else if ((P).handler) {                                                      \
    FAIL(*(P).handler->last_condition);                                        \
  }

#define CHECK_CORUN(P, O, E)                                                   \
  CHECKED_IF((E) == (P).corun(O, E)) {}                                        \
  else if ((P).handler) {                                                      \
    FAIL_CHECK(*(P).handler->last_condition);                                  \
  }

#define REQUIRE_CORUN(P, E)                                                    \
  CHECKED_IF((E) == (P).corun(O, E)) {}                                        \
  else if ((P).handler_) {                                                     \
    FAIL(*(P).handler_->last_condition);                                       \
  }

} // namespace pn_test

#endif // TESTS_PN_TEST_PROACTOR_HPP
