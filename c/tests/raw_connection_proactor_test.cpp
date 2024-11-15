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
#include <arpa/inet.h>
#endif

#include <string.h>

// Raw connection tests driven by a proactor.

// These tests often cheat by directly calling API functions that
// would normally be called in an event callback for thread safety
// reasons.  This can usually work because the proactors and API calls
// are all called from a single thread so there is no contention, but
// the raw connection may require a wake so that the state machine and
// polling mask can be updated.  Note that wakes stop working around
// the time the raw connection thinks it is about to be fully closed,
// so close operations may need to be done in event callbacks to
// avoid wake uncertainty.

#include "../src/proactor/proactor-internal.h"
#include "./pn_test_proactor.hpp"
#include <proton/event.h>
#include <proton/listener.h>

using namespace pn_test;

namespace {

class common_handler : public handler {
  bool close_on_wake_;
  bool write_close_on_wake_;
  bool stop_on_wake_;
  bool abort_on_wake_;
  int closed_read_count_;
  int closed_write_count_;
  int disconnect_count_;
  bool disconnect_error_;
  pn_raw_connection_t *last_server_;
  pn_raw_buffer_t write_buff_;

public:
  explicit common_handler() : close_on_wake_(false), write_close_on_wake_(0), stop_on_wake_(false),
                              abort_on_wake_(false), closed_read_count_(0), closed_write_count_(0),
                              disconnect_count_(0), disconnect_error_(false),
                              last_server_(0), write_buff_({0}) {}

  void set_close_on_wake(bool b) { close_on_wake_ = b; }
  void set_write_close_on_wake(bool b) { write_close_on_wake_ = b; }
  void set_stop_on_wake(bool b) { stop_on_wake_ = b; }
  void set_abort_on_wake(bool b) { abort_on_wake_ = b; }
  int closed_read_count() { return closed_read_count_; }
  int closed_write_count() { return closed_write_count_; }
  int disconnect_count() { return disconnect_count_; }
  bool disconnect_error() { return disconnect_error_; }
  pn_raw_connection_t *last_server() { return last_server_; }
  void set_write_on_wake(pn_raw_buffer_t *b) { write_buff_ = *b; }

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
      if (abort_on_wake_) abort();
      pn_raw_connection_t *rc = pn_event_raw_connection(e);

      if (write_buff_.size) {
        // Add the buff for writing before any close operation.
        CHECK(pn_raw_connection_write_buffers(rc, &write_buff_, 1) == 1);
        write_buff_.size = 0;
      }
      if (write_close_on_wake_)
        pn_raw_connection_write_close(rc);
      if (close_on_wake_)
        pn_raw_connection_close(rc);
      return stop_on_wake_;
    } break;

    case PN_RAW_CONNECTION_DISCONNECTED: {
      disconnect_count_++;
      pn_raw_connection_t *rc = pn_event_raw_connection(e);
      pn_condition_t *cond = pn_raw_connection_condition(rc);
      if (disconnect_count_ == 1 && pn_condition_is_set(cond)) {
        const char *nm = pn_condition_get_name(cond);
        const char *ds = pn_condition_get_description(cond);
        if (nm && strlen(nm) > 0 && ds && strlen(ds) > 0)
          disconnect_error_ = true;
      }
      return false;
    } break;

    case PN_RAW_CONNECTION_CLOSED_READ:
      closed_read_count_++;
      return false;

    case PN_RAW_CONNECTION_CLOSED_WRITE:
      closed_write_count_++;
      return false;

    default:
      return false;
    }
  }
};

static const size_t buffsz = 128;

// Basic test consisting of
//   client is an OS socket.
//   server is a pn_raw_connection_t with one shared read/write buffer.
//   pn_listener_t used to put the two together.
struct basic_test {
  common_handler h;
  proactor p;
  pn_listener_t *l;
  int sockfd; // client
  pn_raw_connection_t *server_rc;
  char buff[buffsz];
  bool buff_in_use;

  basic_test() : h(), p(&h) {
    l = p.listen();
    REQUIRE_RUN(p, PN_LISTENER_OPEN);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sockfd >= 0);
    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(pn_test::listening_port(l).c_str()));
    laddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    connect(sockfd, (const struct sockaddr*) &laddr, sizeof(laddr));

    REQUIRE_RUN(p, PN_LISTENER_ACCEPT);
    server_rc = h.last_server();
    REQUIRE_RUN(p, PN_RAW_CONNECTION_NEED_READ_BUFFERS);
    pn_raw_buffer_t rb = {0, buff, buffsz, 0, 0};
    CHECK(pn_raw_connection_give_read_buffers(server_rc, &rb, 1) == 1);
    buff_in_use = true;

    pn_raw_connection_wake(server_rc);
    REQUIRE_RUN(p, PN_RAW_CONNECTION_WAKE);
    CHECK(pn_proactor_get(p) == NULL); /* idle */
  }

  ~basic_test() {
    pn_listener_close(l);
    REQUIRE_RUN(p, PN_LISTENER_CLOSE);
    REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);
    if (sockfd >= 0) close(sockfd);
    bool sanity = h.closed_read_count() == 1 && h.closed_write_count() == 1 &&
      h.disconnect_count() == 1;
    REQUIRE(sanity == true);
  }

  void socket_write_close() {
    if (sockfd < 0) return;
    shutdown(sockfd, SHUT_WR);
  }

  void socket_graceful_close() {
    if (sockfd < 0) return;
    close(sockfd);
    sockfd = -1;
  }

  bool socket_hard_close() {
    // RST (not FIN), hard/abort close
    if (sockfd < 0) return false;
    struct linger lngr;
    lngr.l_onoff  = 1;
    lngr.l_linger = 0;
    if (sockfd < 0) return false;
    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &lngr, sizeof(lngr)) == 0) {
      if (close(sockfd) == 0) {
        sockfd = -1;
        return true;
      }
    }
    return false;
  }

  void drain_read_buffer() {
    assert(buff_in_use);
    send(sockfd, "FOO", 3, 0);
    REQUIRE_RUN(p, PN_RAW_CONNECTION_READ);
    pn_raw_buffer_t rb = {0};
    REQUIRE(pn_raw_connection_take_read_buffers(server_rc, &rb, 1) == 1);
    REQUIRE(rb.size == 3);
    buff_in_use = false;
  }

  void give_read_buffer() {
    assert(!buff_in_use);
    pn_raw_buffer_t rb = {0, buff, buffsz, 0, 0};
    CHECK(pn_raw_connection_give_read_buffers(server_rc, &rb, 1) == 1);
    buff_in_use = true;
  }

  void write_next_wake(const char *m) {
    assert(!buff_in_use);
    pn_raw_buffer_t rb = {0, buff, buffsz, 0, 0};
    size_t l = strlen(m);
    assert(l < buffsz);
    strcpy(rb.bytes, m);
    rb.size = l;
    h.set_write_on_wake(&rb);
  }

  int drain_events() {
    int ec = 0;
    pn_event_batch_t *batch = NULL;
    while ((batch = pn_proactor_get(p.get()))) {
      pn_event_t *e;
      while ((e = pn_event_batch_next(batch))) {
        ec++;
        h.dispatch(e);
      }
      pn_proactor_done(p.get(), batch);
    }
    return ec;
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

// Normal close
TEST_CASE("raw_connection_graceful_close") {
  struct basic_test x;
  x.socket_graceful_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == false);
}

// HARD close
TEST_CASE("raw_connection_hardclose") {
  struct basic_test x;
  x.socket_hard_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == true);
}

// HARD close, no read buffer
TEST_CASE("raw_connection_hardclose_nrb") {
  struct basic_test x;
  // Drain read buffer without replenishing
  x.drain_read_buffer();
  x.drain_events();
  CHECK(pn_proactor_get(x.p) == NULL); /* idle */
  x.socket_hard_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == true);
}

// HARD close after read close
TEST_CASE("raw_connection_readclose_then_hardclose") {
  struct basic_test x;
  x.socket_write_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  x.drain_events();
  REQUIRE(x.h.disconnect_count() == 0);
  x.socket_hard_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == true);
}

// HARD close after read close, no read buffer
TEST_CASE("raw_connection_readclose_then_hardclose_nrb") {
  struct basic_test x;
  // Drain read buffer without replenishing
  x.drain_read_buffer();
  x.drain_events();
  CHECK(pn_proactor_get(x.p) == NULL); /* idle */
  // Shut of read side should be ignored with no read buffer.
  x.socket_write_close();
  CHECK(pn_proactor_get(x.p) == NULL); /* still idle */

  // Confirm raw connection shuts down, even with no read buffer
  x.socket_hard_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == true);
}

// Normal close on socket delays CLOSED_READ event until application makes read buffers available
TEST_CASE("raw_connection_delay_readclose") {
  struct basic_test x;
  x.drain_read_buffer();
  x.socket_graceful_close();
  x.drain_events();
  REQUIRE(x.h.closed_read_count() == 0);

  x.give_read_buffer();
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_READ);
  REQUIRE(x.h.closed_read_count() == 1);

  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
}

TEST_CASE("raw_connection_rst_on_write") {
  struct basic_test x;
  x.drain_read_buffer();

  // Send some data
  x.write_next_wake("foo");
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WRITTEN);
  pn_raw_buffer_t rb = {0};
  CHECK(pn_raw_connection_take_written_buffers(x.server_rc, &rb, 1) == 1);
  char b[buffsz];
  REQUIRE(recv(x.sockfd, b, buffsz, 0) == 3);

  // Repeat, with closed peer socket.
  x.socket_graceful_close();
  x.write_next_wake("bar");
  pn_raw_connection_wake(x.server_rc);
  // Write or subsequent poll should fail EPIPE
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.disconnect_error() == true);
}

// One sided close.  No cooperation from peer.
TEST_CASE("raw_connection_full_close") {
  struct basic_test x;
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
}

// As above.  No read buffer.
TEST_CASE("raw_connection_full_close_nrb") {
  struct basic_test x;
  x.drain_read_buffer();
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
}

// One sided close, pending write.
TEST_CASE("raw_connection_close_wdrain") {
  struct basic_test x;
  x.drain_read_buffer();
  // write and then close on next wake
  x.write_next_wake("fubar");
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  // Now check fubar made it
  char b[buffsz];
  REQUIRE(recv(x.sockfd, b, buffsz, 0) == 5);
  REQUIRE(strncmp("fubar", b, 5) == 0);
}

// One sided write_close then close.
TEST_CASE("raw_connection_wclose_full_close") {
  struct basic_test x;
  x.h.set_write_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_WRITE);
  x.drain_events();
  REQUIRE(x.h.closed_read_count() == 0);

  x.h.set_write_close_on_wake(false);
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
}

TEST_CASE("raw_connection_wclose_full_close_nrb") {
  struct basic_test x;
  x.drain_read_buffer();
  x.h.set_write_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_CLOSED_WRITE);
  x.drain_events();
  REQUIRE(x.h.closed_read_count() == 0);

  x.h.set_write_close_on_wake(false);
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
}

TEST_CASE("raw_connection_wclose_full_close_wdrain") {
  struct basic_test x;
  x.drain_read_buffer();
  // write and then wclose then close on next wake
  x.write_next_wake("bar");
  x.h.set_write_close_on_wake(true);
  x.h.set_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_WAKE);
  // No send/recv/close/shutdown activity from peer socket.
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  // Now check bar made it
  char b[buffsz];
  REQUIRE(recv(x.sockfd, b, buffsz, 0) == 3);
  REQUIRE(strncmp("bar", b, 3) == 0);
}

// Half closes each direction.  Raw connection then peer.
TEST_CASE("raw_connection_wclose_then_rclose") {
  struct basic_test x;
  x.h.set_write_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  x.drain_events();
  REQUIRE(x.h.closed_write_count() == 1);
  REQUIRE(x.h.closed_read_count() == 0);

  char b[buffsz];
  REQUIRE(recv(x.sockfd, b, buffsz, 0) == 0); // EOF
  x.socket_write_close();
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(x.h.closed_read_count() == 1);
}

// As above but peer first then raw connection.
TEST_CASE("raw_connection_rclose_then_wclose") {
  struct basic_test x;
  x.socket_write_close();
  x.drain_events();
  REQUIRE(x.h.closed_read_count() == 1);
  REQUIRE(x.h.closed_write_count() == 0);

  x.h.set_write_close_on_wake(true);
  pn_raw_connection_wake(x.server_rc);
  REQUIRE_RUN(x.p, PN_RAW_CONNECTION_DISCONNECTED);
  char b[buffsz];
  REQUIRE(recv(x.sockfd, b, buffsz, 0) == 0); // EOF
  REQUIRE(x.h.closed_write_count() == 1);
}

