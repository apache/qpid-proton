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

#include "../src/proactor/proactor-internal.h"
#include "./pn_test_proactor.hpp"
#include "./test_config.h"

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/session.h>
#include <proton/ssl.h>
#include <proton/transport.h>

#include <string.h>

#include <iostream>

using namespace pn_test;
using Catch::Matchers::Contains;
using Catch::Matchers::Equals;

/* Test that interrupt and timeout events cause pn_proactor_wait() to return. */
TEST_CASE("proactor_interrupt_timeout") {
  proactor p;

  CHECK(pn_proactor_get(p) == NULL); /* idle */
  pn_proactor_interrupt(p);
  CHECK(PN_PROACTOR_INTERRUPT == p.wait_next());
  CHECK(pn_proactor_get(p) == NULL); /* idle */

  /* Set an immediate timeout */
  pn_proactor_set_timeout(p, 0);
  CHECK(PN_PROACTOR_TIMEOUT == p.wait_next());
  CHECK(PN_PROACTOR_INACTIVE == p.wait_next());

  /* Set a (very short) timeout */
  pn_proactor_set_timeout(p, 1);
  CHECK(PN_PROACTOR_TIMEOUT == p.wait_next());
  CHECK(PN_PROACTOR_INACTIVE == p.wait_next());

  /* Set and cancel a timeout, make sure we don't get the timeout event */
  pn_proactor_set_timeout(p, 10000000);
  pn_proactor_cancel_timeout(p);
  CHECK(PN_PROACTOR_INACTIVE == p.wait_next());
  CHECK(pn_proactor_get(p) == NULL); /* idle */
}

namespace {

class common_handler : public handler {
  handler *accept_; // Handler for accepted connections

public:
  common_handler(handler *accept = 0) : accept_(accept) {}

  bool handle(pn_event_t *e) CATCH_OVERRIDE {
    switch (pn_event_type(e)) {
      /* Always stop on these noteworthy events */
    case PN_TRANSPORT_ERROR:
    case PN_LISTENER_OPEN:
    case PN_LISTENER_CLOSE:
    case PN_PROACTOR_INACTIVE:
      return true;

    case PN_LISTENER_ACCEPT:
      listener = pn_event_listener(e);
      connection = pn_connection();
      if (accept_) pn_connection_set_context(connection, accept_);
      pn_listener_accept2(listener, connection, NULL);
      return false;

      // Return remote opens
    case PN_CONNECTION_REMOTE_OPEN:
      pn_connection_open(pn_event_connection(e));
      return false;
    case PN_SESSION_REMOTE_OPEN:
      pn_session_open(pn_event_session(e));
      return false;
    case PN_LINK_REMOTE_OPEN:
      pn_link_open(pn_event_link(e));
      return false;

      // Return remote closes
    case PN_CONNECTION_REMOTE_CLOSE:
      pn_connection_close(pn_event_connection(e));
      return false;
    case PN_SESSION_REMOTE_CLOSE:
      pn_session_close(pn_event_session(e));
      return false;
    case PN_LINK_REMOTE_CLOSE:
      pn_link_close(pn_event_link(e));
      return false;

    default:
      return false;
    }
  }
};

/* close a connection when it is remote open */
struct close_on_open_handler : public common_handler {
  bool handle(pn_event_t *e) CATCH_OVERRIDE {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_REMOTE_OPEN:
      pn_connection_close(pn_event_connection(e));
      return false;
    default:
      return common_handler::handle(e);
    }
  }
};

} // namespace

/* Test simple client/server connection that opens and closes */
TEST_CASE("proactor_connect") {
  close_on_open_handler h;
  proactor p(&h);
  /* Connect and wait for close at both ends */
  pn_listener_t *l = p.listen(":0", &h);
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  p.connect(l);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
}

namespace {
/* Return on connection open, close and return on wake */
struct close_on_wake_handler : public common_handler {
  bool handle(pn_event_t *e) CATCH_OVERRIDE {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_WAKE:
      pn_connection_close(pn_event_connection(e));
      return true;
    default:
      return common_handler::handle(e);
    }
  }
};
} // namespace

// Test waking up a connection that is idle
TEST_CASE("proactor_connection_wake") {
  common_handler h;
  proactor p(&h);
  close_on_wake_handler wh;
  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  pn_connection_t *c = p.connect(l, &wh);
  pn_incref(c); /* Keep a reference for wake() after free */

  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  while (p.flush().first != 0);
  pn_connection_wake(c);
  REQUIRE_RUN(p, PN_CONNECTION_WAKE);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED); /* Both ends */

  /* Verify we don't get a wake after close even if they happen together */
  pn_connection_t *c2 = p.connect(l, &wh);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  pn_connection_wake(c2);
  pn_proactor_disconnect(p, NULL);
  pn_connection_wake(c2);

  for (pn_event_type_t et = p.run(); et != PN_PROACTOR_INACTIVE; et = p.run()) {
    switch (et) {
    case PN_TRANSPORT_ERROR:
    case PN_TRANSPORT_CLOSED:
    case PN_LISTENER_CLOSE:
      break; // expected
    default:
      FAIL("Unexpected event type: " << et);
    }
  }
  // The pn_connection_t is still valid so wake is legal but a no-op.
  // Make sure there's no memory error.
  pn_connection_wake(c);
  pn_decref(c);
}

namespace {
struct abort_handler : public common_handler {
  bool handle(pn_event_t *e) {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_REMOTE_OPEN:
      /* Close the transport - abruptly closes the socket */
      pn_transport_close_tail(pn_connection_transport(pn_event_connection(e)));
      pn_transport_close_head(pn_connection_transport(pn_event_connection(e)));
      return false;

    default:
      return common_handler::handle(e);
    }
  }
};
} // namespace

/* Verify that pn_transport_close_head/tail aborts a connection without an AMQP
 * protocol close */
TEST_CASE("proactor_abort") {
  abort_handler sh; // Handle listener and server side of connection
  proactor p(&sh);
  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  common_handler ch; // Handle client side of connection
  pn_connection_t *c = p.connect(l, &ch);

  /* server transport closes */
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*sh.last_condition,
             cond_matches("amqp:connection:framing-error", "abort"));

  /* client transport closes */
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*ch.last_condition,
             cond_matches("amqp:connection:framing-error", "abort"));
  pn_listener_close(l);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  /* Verify expected event sequences, no unexpected events */
  CHECK_THAT(ETYPES(PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN,
                    PN_CONNECTION_BOUND, PN_TRANSPORT_TAIL_CLOSED,
                    PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED,
                    PN_TRANSPORT_CLOSED),
             Equals(ch.log_clear()));
  CHECK_THAT(ETYPES(PN_LISTENER_OPEN, PN_LISTENER_ACCEPT, PN_CONNECTION_INIT,
                    PN_CONNECTION_BOUND, PN_CONNECTION_REMOTE_OPEN,
                    PN_TRANSPORT_TAIL_CLOSED, PN_TRANSPORT_ERROR,
                    PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
                    PN_LISTENER_CLOSE, PN_PROACTOR_INACTIVE),
             Equals(sh.log_clear()));
}

/* Test that INACTIVE event is generated when last connections/listeners closes.
 */
TEST_CASE("proactor_inactive") {
  close_on_wake_handler h;
  proactor p(&h);

  /* Listen, connect, disconnect */
  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  pn_connection_t *c = p.connect(l, &h);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  pn_connection_wake(c);
  REQUIRE_RUN(p, PN_CONNECTION_WAKE);
  /* Expect TRANSPORT_CLOSED both ends */
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  pn_listener_close(l);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  /* Immediate timer generates INACTIVE (no connections) */
  pn_proactor_set_timeout(p, 0);
  REQUIRE_RUN(p, PN_PROACTOR_TIMEOUT);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  /* Connect, set-timer, disconnect */
  l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  c = p.connect(l, &h);
  pn_proactor_set_timeout(p, 1000000);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  pn_connection_wake(c);
  REQUIRE_RUN(p, PN_CONNECTION_WAKE);
  /* Expect TRANSPORT_CLOSED from client and server */
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  pn_listener_close(l);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  /* No INACTIVE till timer is cancelled */
  CHECK(!pn_proactor_get(p)); // idle
  pn_proactor_cancel_timeout(p);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);
}

/* Tests for error handling */
TEST_CASE("proactor_errors") {
  close_on_wake_handler h;
  proactor p(&h);
  /* Invalid connect/listen service name */
  p.connect("127.0.0.1:xxx");
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", "xxx"));
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  pn_listener_t *l = pn_listener();
  pn_proactor_listen(p, l, "127.0.0.1:xxx", 1);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", "xxx"));
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  /* Invalid connect/listen host name */
  p.connect("nosuch.example.com:");
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", "nosuch"));
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  pn_proactor_listen(p, pn_listener(), "nosuch.example.com:", 1);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", "nosuch"));
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  /* Listen on a port already in use */
  l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  std::string laddr = ":" + listening_port(l);
  p.listen(laddr);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io"));

  pn_listener_close(l);
  REQUIRE_RUN(p, PN_LISTENER_CLOSE);
  REQUIRE_RUN(p, PN_PROACTOR_INACTIVE);

  /* Connect with no listener */
  p.connect(laddr);
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", "refused"));
}

namespace {
/* Closing the connection during PN_TRANSPORT_ERROR should be a no-op
 * Regression test for: https://issues.apache.org/jira/browse/PROTON-1586
 */
struct transport_close_connection_handler : public common_handler {
  bool handle(pn_event_t *e) {
    switch (pn_event_type(e)) {
    case PN_TRANSPORT_ERROR:
      pn_connection_close(pn_event_connection(e));
      break;
    default:
      return common_handler::handle(e);
    }
    return PN_EVENT_NONE;
  }
};
} // namespace

/* Closing the connection during PN_TRANSPORT_ERROR due to connection failure
 * should be a no-op. Regression test for:
 * https://issues.apache.org/jira/browse/PROTON-1586
 */
TEST_CASE("proactor_proton_1586") {
  transport_close_connection_handler h;
  proactor p(&h);
  p.connect(":yyy");
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  CHECK_THAT(*h.last_condition, cond_matches("proton:io", ":yyy"));

  // No events expected after PN_TRANSPORT_CLOSED, proactor is inactive.
  CHECK(PN_PROACTOR_INACTIVE == p.wait_next());
}

/* Test that we can control listen/select on ipv6/v4 and listen on both by
 * default */
TEST_CASE("proactor_ipv4_ipv6") {
  close_on_open_handler h;
  proactor p(&h);

  /* Listen on all interfaces for IPv4 only. */
  pn_listener_t *l4 = p.listen("0.0.0.0:0");
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  /* Empty address listens on both IPv4 and IPv6 on all interfaces */
  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);

#define EXPECT_CONNECT(LISTENER, HOST)                                         \
  do {                                                                         \
    p.connect(std::string(HOST) + ":" + listening_port(LISTENER));             \
    REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);                                       \
    CHECK_THAT(*h.last_condition, cond_empty());                               \
  } while (0)

  EXPECT_CONNECT(l4, "127.0.0.1"); /* v4->v4 */
  EXPECT_CONNECT(l4, "");          /* local->v4*/

  EXPECT_CONNECT(l, "127.0.0.1"); /* v4->all */
  EXPECT_CONNECT(l, "");          /* local->all */

  /* Listen on ipv6 loopback, if it fails skip ipv6 tests.

     NOTE: Don't use the unspecified address "::" here - ipv6-disabled platforms
     may allow listening on "::" without complaining. However they won't have a
     local ipv6 loopback configured, so "::1" will force an error.
  */
  pn_listener_t *l6 = pn_listener();
  pn_proactor_listen(p, l6, "::1:0", 4);
  pn_event_type_t e = p.run();
  if (e == PN_LISTENER_OPEN && !pn_condition_is_set(h.last_condition)) {
    EXPECT_CONNECT(l6, "::1"); /* v6->v6 */
    EXPECT_CONNECT(l6, "");    /* local->v6 */
    EXPECT_CONNECT(l, "::1");  /* v6->all */
    pn_listener_close(l6);
  } else {
    WARN("skip IPv6 tests: %s %s" << e << *h.last_condition);
  }

  pn_listener_close(l);
  pn_listener_close(l4);
}

/* Make sure we clean up released connections and open sockets
 * correctly */
TEST_CASE("proactor_release_free") {
  common_handler h;
  proactor p(&h);

  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  /* leave one connection to the proactor  */
  pn_connection_t *c = p.connect(l);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);

  {
    /* release c1 and free immediately */
    auto_free<pn_connection_t, pn_connection_free> c1(p.connect(l));
    REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
    REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
    pn_proactor_release_connection(c1);
  }
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);

  /* release c2 and but don't free till after proactor free */
  auto_free<pn_connection_t, pn_connection_free> c2(p.connect(l));
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  pn_proactor_release_connection(c2);
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);

  // OK to free a listener/connection that was never used by a
  // proactor.
  pn_listener_free(pn_listener());
  pn_connection_free(pn_connection());
}

#define SSL_FILE(NAME) CMAKE_CURRENT_SOURCE_DIR "/ssl-certs/" NAME
#define SSL_PW "tserverpw"
/* Windows vs. OpenSSL certificates */
#if defined(_WIN32)
#define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.p12")
#define SET_CREDENTIALS(DOMAIN, NAME)                                          \
  pn_ssl_domain_set_credentials(DOMAIN, SSL_FILE(NAME "-full.p12"), "", SSL_PW)
#else
#define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.pem")
#define SET_CREDENTIALS(DOMAIN, NAME)                                          \
  pn_ssl_domain_set_credentials(DOMAIN, CERTIFICATE(NAME),                     \
                                SSL_FILE(NAME "-private-key.pem"), SSL_PW)
#endif

namespace {

struct ssl_handler : public common_handler {
  auto_free<pn_ssl_domain_t, pn_ssl_domain_free> ssl_domain;

  ssl_handler(pn_ssl_domain_t *d) : ssl_domain(d) {}

  bool handle(pn_event_t *e) {
    switch (pn_event_type(e)) {

    case PN_CONNECTION_BOUND:
      CHECK(0 == pn_ssl_init(pn_ssl(pn_event_transport(e)), ssl_domain, NULL));
      return false;

    case PN_CONNECTION_REMOTE_OPEN: {
      pn_ssl_t *ssl = pn_ssl(pn_event_transport(e));
      CHECK(ssl);
      char protocol[256];
      CHECK(pn_ssl_get_protocol_name(ssl, protocol, sizeof(protocol)));
      CHECK_THAT(protocol, Contains("TLS"));
      pn_connection_t *c = pn_event_connection(e);
      if (pn_connection_state(c) & PN_LOCAL_ACTIVE) {
        pn_connection_close(c); // Client closes on completion.
      } else {
        pn_connection_open(c); // Server returns the OPEN
      }
      return true;
    }
    default:
      return common_handler::handle(e);
    }
  }
};

} // namespace

/* Test various SSL connections between proactors*/
TEST_CASE("proactor_ssl") {
  if (!pn_ssl_present()) {
    WARN("Skip SSL tests, not available");
    return;
  }

  ssl_handler client(pn_ssl_domain(PN_SSL_MODE_CLIENT));
  ssl_handler server(pn_ssl_domain(PN_SSL_MODE_SERVER));
  CHECK(0 == SET_CREDENTIALS(server.ssl_domain, "tserver"));
  proactor p;
  common_handler listener(&server); // Use server for accepted connections
  pn_listener_t *l = p.listen(":0", &listener);
  REQUIRE_RUN(p, PN_LISTENER_OPEN);

  /* Basic SSL connection */
  p.connect(l, &client);
  /* Open ok at both ends */
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  CHECK_THAT(*server.last_condition, cond_empty());
  CHECK_THAT(*client.last_condition, cond_empty());
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);

  /* Verify peer with good hostname */
  pn_ssl_domain_t *cd = client.ssl_domain;
  REQUIRE(0 == pn_ssl_domain_set_trusted_ca_db(cd, CERTIFICATE("tserver")));
  REQUIRE(0 == pn_ssl_domain_set_peer_authentication(
                   cd, PN_SSL_VERIFY_PEER_NAME, NULL));
  pn_connection_t *c = pn_connection();
  pn_connection_set_hostname(c, "test_server");
  p.connect(l, &client, c);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  CHECK_THAT(*server.last_condition, cond_empty());
  CHECK_THAT(*client.last_condition, cond_empty());
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);
  REQUIRE_RUN(p, PN_TRANSPORT_CLOSED);

  /* Verify peer with bad hostname */
  c = pn_connection();
  pn_connection_set_hostname(c, "wrongname");
  p.connect(l, &client, c);
  REQUIRE_RUN(p, PN_TRANSPORT_ERROR);
  CHECK_THAT(*client.last_condition,
             cond_matches("amqp:connection:framing-error", "SSL"));
}

TEST_CASE("proactor_addr") {
  /* Test the address formatter */
  CHECK(1 == pn_proactor_addr(NULL, 0, "", ""));
  CHECK(7 == pn_proactor_addr(NULL, 0, "foo", "bar"));

  char addr[PN_MAX_ADDR];
  pn_proactor_addr(addr, sizeof(addr), "foo", "bar");
  CHECK_THAT("foo:bar", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "foo", "");
  CHECK_THAT("foo:", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "foo", NULL);
  CHECK_THAT("foo:", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "", "bar");
  CHECK_THAT(":bar", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), NULL, "bar");
  CHECK_THAT(":bar", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", "5");
  CHECK_THAT("1:2:3:4:5", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", "");
  CHECK_THAT("1:2:3:4:", Equals(addr));
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", NULL);
  CHECK_THAT("1:2:3:4:", Equals(addr));
}

/* Test pn_proactor_addr functions */

TEST_CASE("proactor_netaddr") {
  common_handler h;
  proactor p(&h);
  /* Use IPv4 to get consistent results all platforms */
  pn_listener_t *l = p.listen("127.0.0.1:0");
  REQUIRE_RUN(p, PN_LISTENER_OPEN);
  pn_connection_t *c = p.connect(l);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);
  REQUIRE_RUN(p, PN_CONNECTION_REMOTE_OPEN);

  // client remote, client local, server remote and server
  // local address strings
  char cr[1024], cl[1024], sr[1024], sl[1024];

  pn_transport_t *ct = pn_connection_transport(c);
  const pn_netaddr_t *na = pn_transport_remote_addr(ct);
  pn_netaddr_str(na, cr, sizeof(cr));
  CHECK_THAT(cr, Contains(listening_port(l)));

  pn_connection_t *s = h.connection; /* server side of the connection */

  pn_transport_t *st = pn_connection_transport(s);
  pn_netaddr_str(pn_transport_local_addr(st), sl, sizeof(sl));
  CHECK_THAT(cr, Equals(sl)); /* client remote == server local */

  pn_netaddr_str(pn_transport_local_addr(ct), cl, sizeof(cl));
  pn_netaddr_str(pn_transport_remote_addr(st), sr, sizeof(sr));
  CHECK_THAT(cl, Equals(sr)); /* client local == server remote */

  char host[PN_MAX_ADDR] = "";
  char serv[PN_MAX_ADDR] = "";
  CHECK(0 == pn_netaddr_host_port(na, host, sizeof(host), serv, sizeof(serv)));
  CHECK_THAT("127.0.0.1", Equals(host));
  CHECK(listening_port(l) == serv);

  /* Make sure you can use NULL, 0 to get length of address
   * string without a crash */
  size_t len = pn_netaddr_str(pn_transport_local_addr(ct), NULL, 0);
  CHECK(strlen(cl) == len);
}

TEST_CASE("proactor_parse_addr") {
  char buf[1024];
  const char *host, *port;

  CHECK(0 == pni_parse_addr("foo:bar", buf, sizeof(buf), &host, &port));
  CHECK_THAT("foo", Equals(host));
  CHECK_THAT("bar", Equals(port));

  CHECK(0 == pni_parse_addr("foo:", buf, sizeof(buf), &host, &port));
  CHECK_THAT("foo", Equals(host));
  CHECK_THAT("5672", Equals(port));

  CHECK(0 == pni_parse_addr(":bar", buf, sizeof(buf), &host, &port));
  CHECK(NULL == host);
  CHECK_THAT("bar", Equals(port));

  CHECK(0 == pni_parse_addr(":", buf, sizeof(buf), &host, &port));
  CHECK(NULL == host);
  CHECK_THAT("5672", Equals(port));

  CHECK(0 == pni_parse_addr(":amqps", buf, sizeof(buf), &host, &port));
  CHECK_THAT("5671", Equals(port));

  CHECK(0 == pni_parse_addr(":amqp", buf, sizeof(buf), &host, &port));
  CHECK_THAT("5672", Equals(port));

  CHECK(0 == pni_parse_addr("::1:2:3", buf, sizeof(buf), &host, &port));
  CHECK_THAT("::1:2", Equals(host));
  CHECK_THAT("3", Equals(port));

  CHECK(0 == pni_parse_addr(":::", buf, sizeof(buf), &host, &port));
  CHECK_THAT("::", Equals(host));
  CHECK_THAT("5672", Equals(port));

  CHECK(0 == pni_parse_addr("", buf, sizeof(buf), &host, &port));
  CHECK(NULL == host);
  CHECK_THAT("5672", Equals(port));
}

/* Test pn_proactor_disconnect */
TEST_CASE("proactor_disconnect") {
  common_handler ch, sh;
  proactor client(&ch), server(&sh);

  // Start two listeners on the server
  pn_listener_t *l = server.listen();
  REQUIRE_RUN(server, PN_LISTENER_OPEN);
  pn_listener_t *l2 = server.listen();
  REQUIRE_RUN(server, PN_LISTENER_OPEN);

  // Two connections from client
  pn_connection_t *c = client.connect(l);
  CHECK_CORUN(client, server, PN_CONNECTION_REMOTE_OPEN);
  pn_connection_t *c2 = client.connect(l);
  CHECK_CORUN(client, server, PN_CONNECTION_REMOTE_OPEN);

  /* Disconnect the client proactor */
  auto_free<pn_condition_t, pn_condition_free> cond(pn_condition());
  pn_condition_format(cond, "test-name", "test-description");
  pn_proactor_disconnect(client, cond);

  /* Verify expected client side first */
  CHECK_CORUN(client, server, PN_TRANSPORT_ERROR);
  CHECK_THAT(*client.handler->last_condition,
             cond_matches("test-name", "test-description"));
  CHECK_CORUN(client, server, PN_TRANSPORT_ERROR);
  CHECK_THAT(*client.handler->last_condition,
             cond_matches("test-name", "test-description"));
  REQUIRE_RUN(client, PN_PROACTOR_INACTIVE);

  /* Now check server sees the disconnects */
  CHECK_CORUN(server, client, PN_TRANSPORT_ERROR);
  CHECK_THAT(*server.handler->last_condition,
             cond_matches("amqp:connection:framing-error", "aborted"));
  CHECK_CORUN(server, client, PN_TRANSPORT_ERROR);
  CHECK_THAT(*server.handler->last_condition,
             cond_matches("amqp:connection:framing-error", "aborted"));

  /* Now disconnect the server end (the listeners) */
  pn_proactor_disconnect(server, NULL);
  REQUIRE_RUN(server, PN_LISTENER_CLOSE);
  REQUIRE_RUN(server, PN_LISTENER_CLOSE);
  REQUIRE_RUN(server, PN_PROACTOR_INACTIVE);

  /* Make sure the proactors are still functional */
  pn_listener_t *l3 = server.listen();
  REQUIRE_RUN(server, PN_LISTENER_OPEN);
  client.connect(l3);
  CHECK_CORUN(client, server, PN_CONNECTION_REMOTE_OPEN);
}

namespace {
const size_t FRAME = 512;                    /* Smallest legal frame */
const ssize_t CHUNK = (FRAME + FRAME / 2);   /* Chunk overflows frame */
const size_t BODY = (CHUNK * 3 + CHUNK / 2); /* Body doesn't fit into chunks */
} // namespace

struct message_stream_handler : public common_handler {
  pn_link_t *sender;
  pn_delivery_t *dlv;
  pn_rwbytes_t send_buf, recv_buf;
  ssize_t size, sent, received;
  bool complete;

  message_stream_handler()
      : sender(), dlv(), send_buf(), recv_buf(), size(), sent(), received(),
        complete() {}

  bool handle(pn_event_t *e) {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_BOUND:
      pn_transport_set_max_frame(pn_event_transport(e), FRAME);
      return false;

    case PN_SESSION_INIT:
      pn_session_set_incoming_capacity(pn_event_session(e),
                                       FRAME); /* Single frame incoming */
      pn_session_set_outgoing_window(pn_event_session(e),
                                     1); /* Single frame outgoing */
      return false;

    case PN_LINK_REMOTE_OPEN:
      common_handler::handle(e);
      if (pn_link_is_receiver(pn_event_link(e))) {
        pn_link_flow(pn_event_link(e), 1);
      } else {
        sender = pn_event_link(e);
      }
      return false;

    case PN_LINK_FLOW: /* Start a delivery */
      if (pn_link_is_sender(pn_event_link(e)) && !dlv) {
        dlv = pn_delivery(pn_event_link(e), pn_dtag("x", 1));
      }
      return false;

    case PN_CONNECTION_WAKE: { /* Send a chunk */
      ssize_t remains = size - sent;
      ssize_t n = (CHUNK < remains) ? CHUNK : remains;
      CHECK(n == pn_link_send(sender, send_buf.start + sent, n));
      sent += n;
      if (sent == size) {
        CHECK(pn_link_advance(sender));
      }
      return false;
    }

    case PN_DELIVERY: { /* Receive a delivery - smaller than a
                           chunk? */
      pn_delivery_t *dlv = pn_event_delivery(e);
      if (pn_delivery_readable(dlv)) {
        ssize_t n = pn_delivery_pending(dlv);
        rwbytes_ensure(&recv_buf, received + n);
        REQUIRE(n ==
                pn_link_recv(pn_event_link(e), recv_buf.start + received, n));
        received += n;
      }
      complete = !pn_delivery_partial(dlv);
      return true;
    }
    default:
      return common_handler::handle(e);
    }
  }
};

/* Test sending/receiving a message in chunks */
TEST_CASE("proactor_message_stream") {
  message_stream_handler h;
  proactor p(&h);

  pn_listener_t *l = p.listen();
  REQUIRE_RUN(p, PN_LISTENER_OPEN);

  /* Encode a large (not very) message to send in chunks */
  auto_free<pn_message_t, pn_message_free> m(pn_message());
  pn_data_put_binary(pn_message_body(m), pn_bytes(std::string(BODY, 'x')));
  h.size = pn_message_encode2(m, &h.send_buf);

  pn_connection_t *c = p.connect(l);
  pn_session_t *ssn = pn_session(c);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  REQUIRE_RUN(p, PN_LINK_FLOW);

  /* Send and receive the message in chunks */
  do {
    pn_connection_wake(c); /* Initiate send/receive of one chunk */
    do {                   /* May be multiple receives for one send */
      REQUIRE_RUN(p, PN_DELIVERY);
    } while (h.received < h.sent);
  } while (!h.complete);
  CHECK(h.received == h.size);
  CHECK(h.sent == h.size);
  CHECK(!memcmp(h.send_buf.start, h.recv_buf.start, h.size));

  free(h.send_buf.start);
  free(h.recv_buf.start);
}
