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

#include <proton/raw_connection.h>
#include "proactor/raw_connection-internal.h"

#include "pn_test.hpp"

#ifdef _WIN32
#include <errno.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <string.h>

// WAKE tests require a running proactor.

#include "../src/proactor/proactor-internal.h"
#include "./pn_test_proactor.hpp"
#include <proton/event.h>
#include <proton/listener.h>

using namespace pn_test;

namespace {

class common_handler : public handler {
  bool close_on_wake_;
  pn_raw_connection_t *last_server_;

public:
  explicit common_handler() : close_on_wake_(false), last_server_(0) {}

  void set_close_on_wake(bool b) { close_on_wake_ = b; }

  pn_raw_connection_t *last_server() { return last_server_; }

  bool handle(pn_event_t *e) override {
    switch (pn_event_type(e)) {
      /* Always stop on these noteworthy events */
    case PN_LISTENER_OPEN:
    case PN_LISTENER_CLOSE:
    case PN_PROACTOR_INACTIVE:
      return true;

    case PN_LISTENER_ACCEPT: {
      listener = pn_event_listener(e);
      pn_raw_connection_t *rc = pn_raw_connection();
      pn_listener_raw_accept(listener, rc);
      last_server_ = rc;
      return false;
    } break;

    case PN_RAW_CONNECTION_WAKE: {
      if (close_on_wake_) {
        pn_raw_connection_t *rc = pn_event_raw_connection(e);
        pn_raw_connection_close(rc);
      }
      return true;
    } break;


    default:
      return false;
    }
  }
};


} // namespace

// Test waking up a connection that is idle
TEST_CASE("proactor_raw_connection_wake") {
  common_handler h;
  proactor p(&h);
  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);

  pn_raw_connection_t *rc = pn_raw_connection();
  std::string addr = ":" + pn_test::listening_port(l);
  pn_proactor_raw_connect(pn_listener_proactor(l), rc, addr.c_str());


  REQUIRE_RUN(p, PN_LISTENER_ACCEPT);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_NEED_READ_BUFFERS);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_NEED_READ_BUFFERS);
  CHECK(pn_proactor_get(p) == NULL); /* idle */
    pn_raw_connection_wake(rc);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_WAKE);
  CHECK(pn_proactor_get(p) == NULL); /* idle */

  h.set_close_on_wake(true);
  pn_raw_connection_wake(rc);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_DISCONNECTED);
  pn_raw_connection_wake(h.last_server());
  REQUIRE_RUN(p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(p, PN_RAW_CONNECTION_DISCONNECTED);
  pn_listener_close(l);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);
}
