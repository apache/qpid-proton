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

#include "test_tools.h"
#include "test_config.h"
#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/event.h>
#include <proton/listener.h>
#include <proton/proactor.h>
#include <proton/ssl.h>
#include <proton/transport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pn_millis_t timeout = 7*1000; /* timeout for hanging tests */

static const char *localhost = "127.0.0.1"; /* host for connect/listen */

typedef pn_event_type_t (*test_handler_fn)(test_t *, pn_event_t*);

/* Save the last condition description of a handled event here  */
char last_condition[1024] = {0};

/* Proactor and handler that take part in a test */
typedef struct proactor_test_t {
  test_handler_fn handler;
  test_t *t;
  pn_proactor_t *proactor;
} proactor_test_t;


/* Initialize an array of proactor_test_t */
static void proactor_test_init(proactor_test_t *pts, size_t n, test_t *t) {
  for (proactor_test_t *pt = pts; pt < pts + n; ++pt) {
    if (!pt->t) pt->t = t;
    if (!pt->proactor) pt->proactor = pn_proactor();
    pn_proactor_set_timeout(pt->proactor, timeout);
  }
}

#define PROACTOR_TEST_INIT(A, T) proactor_test_init(A, sizeof(A)/sizeof(*A), (T))

static void proactor_test_free(proactor_test_t *pts, size_t n) {
  for (proactor_test_t *pt = pts; pt < pts + n; ++pt) {
    pn_proactor_free(pt->proactor);
  }
}

#define PROACTOR_TEST_FREE(A) proactor_test_free(A, sizeof(A)/sizeof(*A))

static void save_condition(pn_event_t *e) {
  /* TODO aconway 2017-03-23: extend pn_event_condition to include listener */
  last_condition[0] = '\0';
  pn_condition_t *cond = NULL;
  if (pn_event_listener(e)) {
    cond = pn_listener_condition(pn_event_listener(e));
  } else {
    cond = pn_event_condition(e);
  }
  if (cond && pn_condition_is_set(cond)) {
    const char *desc = pn_condition_get_description(cond);
    strncpy(last_condition, desc, sizeof(last_condition));
  }
}

/* Process events on a proactor array until a handler returns an event, or
 * all proactors return NULL
 */
static pn_event_type_t proactor_test_get(proactor_test_t *pts, size_t n) {
  while (true) {
    bool busy = false;
    for (proactor_test_t *pt = pts; pt < pts + n; ++pt) {
      pn_event_batch_t *eb =  pn_proactor_get(pt->proactor);
      if (eb) {
        busy = true;
        pn_event_type_t ret = PN_EVENT_NONE;
        for (pn_event_t* e = pn_event_batch_next(eb); e; e = pn_event_batch_next(eb)) {
          save_condition(e);
          ret = pt->handler(pt->t, e);
          if (ret) break;
        }
        pn_proactor_done(pt->proactor, eb);
        if (ret) return ret;
      }
    }
    if (!busy) {
      return PN_EVENT_NONE;
    }
  }
}

/* Run an array of proactors till a handler returns an event. */
static pn_event_type_t proactor_test_run(proactor_test_t *pts, size_t n) {
  pn_event_type_t e;
  while ((e = proactor_test_get(pts, n)) == PN_EVENT_NONE)
         ;
  return e;
}


/* Drain and discard outstanding events from an array of proactors */
static void proactor_test_drain(proactor_test_t *pts, size_t n) {
  while (proactor_test_get(pts, n))
         ;
}


#define PROACTOR_TEST_GET(A) proactor_test_get((A), sizeof(A)/sizeof(*A))
#define PROACTOR_TEST_RUN(A) proactor_test_run((A), sizeof(A)/sizeof(*A))
#define PROACTOR_TEST_DRAIN(A) proactor_test_drain((A), sizeof(A)/sizeof(*A))

/* Wait for the next single event, return its type */
static pn_event_type_t wait_next(pn_proactor_t *proactor) {
  pn_event_batch_t *events = pn_proactor_wait(proactor);
  pn_event_type_t etype = pn_event_type(pn_event_batch_next(events));
  pn_proactor_done(proactor, events);
  return etype;
}

/* Test that interrupt and timeout events cause pn_proactor_wait() to return. */
static void test_interrupt_timeout(test_t *t) {
  pn_proactor_t *p = pn_proactor();
  TEST_CHECK(t, pn_proactor_get(p) == NULL); /* idle */
  pn_proactor_interrupt(p);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INTERRUPT, wait_next(p));
  TEST_CHECK(t, pn_proactor_get(p) == NULL); /* idle */

  /* Set an immediate timeout */
  pn_proactor_set_timeout(p, 0);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_TIMEOUT, wait_next(p));

  /* Set a (very short) timeout */
  pn_proactor_set_timeout(p, 10);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_TIMEOUT, wait_next(p));

  /* Set and cancel a timeout, make sure we don't get the timeout event */
  pn_proactor_set_timeout(p, 10);
  pn_proactor_cancel_timeout(p);
  TEST_CHECK(t, pn_proactor_get(p) == NULL); /* idle */

  pn_proactor_free(p);
}

/* Save the last connection accepted by the common_handler */
pn_connection_t *last_accepted = NULL;

/* Common handler for simple client/server interactions,  */
static pn_event_type_t common_handler(test_t *t, pn_event_t *e) {
  pn_connection_t *c = pn_event_connection(e);
  pn_listener_t *l = pn_event_listener(e);

  switch (pn_event_type(e)) {

    /* Cleanup events */
   case PN_LISTENER_CLOSE:
    pn_listener_free(pn_event_listener(e));
    return PN_EVENT_NONE;

   case PN_TRANSPORT_CLOSED:
    pn_connection_free(pn_event_connection(e));
    return PN_TRANSPORT_CLOSED;

    /* Stop on these events */
   case PN_PROACTOR_INACTIVE:
   case PN_PROACTOR_TIMEOUT:
    return pn_event_type(e);

   case PN_LISTENER_OPEN:
    last_accepted = NULL;
    return pn_event_type(e);

   case PN_LISTENER_ACCEPT:
    last_accepted = pn_connection();
    pn_listener_accept(l, last_accepted);
    pn_listener_close(l);       /* Only accept one connection */
    return PN_EVENT_NONE;

   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(c);      /* Return the open (no-op if already open) */
    return PN_EVENT_NONE;

   case PN_CONNECTION_REMOTE_CLOSE:
    pn_connection_close(c);     /* Return the close */
    return PN_EVENT_NONE;

    /* Ignored these events */
   case PN_CONNECTION_INIT:
   case PN_CONNECTION_BOUND:
   case PN_CONNECTION_LOCAL_OPEN:
   case PN_CONNECTION_LOCAL_CLOSE:
   case PN_TRANSPORT:
   case PN_TRANSPORT_ERROR:
   case PN_TRANSPORT_HEAD_CLOSED:
   case PN_TRANSPORT_TAIL_CLOSED:
    return PN_EVENT_NONE;

   default:
    TEST_ERRORF(t, "unexpected event %s", pn_event_type_name(pn_event_type(e)));
    return PN_EVENT_NONE;                   /* Fail the test but keep going */
  }
}

/* Like common_handler but does not auto-close the listener after one accept */
static pn_event_type_t listen_handler(test_t *t, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_LISTENER_ACCEPT:
    /* No automatic listener close/free for tests that accept multiple connections */
    last_accepted = pn_connection();
    pn_listener_accept(pn_event_listener(e), last_accepted);
    return PN_EVENT_NONE;

   case PN_LISTENER_CLOSE:
    /* No automatic free */
    return PN_LISTENER_CLOSE;

   default:
    return common_handler(t, e);
  }
}

/* close a connection when it is remote open */
static pn_event_type_t open_close_handler(test_t *t, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_close(pn_event_connection(e));
    return PN_EVENT_NONE;          /* common_handler will finish on TRANSPORT_CLOSED */
   default:
    return common_handler(t, e);
  }
}

/* Test simple client/server connection with 2 proactors */
static void test_client_server(test_t *t) {
  proactor_test_t pts[] ={ { open_close_handler }, { common_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);
  pn_proactor_listen(server, pn_listener(), port.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port.sock);
  /* Connect and wait for close at both ends */
  pn_proactor_connect(client, pn_connection(), port.host_port);
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  PROACTOR_TEST_FREE(pts);
}

/* Return on connection open, close and return on wake */
static pn_event_type_t open_wake_handler(test_t *t, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    return pn_event_type(e);
   case PN_CONNECTION_WAKE:
    pn_connection_close(pn_event_connection(e));
    return pn_event_type(e);
   default:
    return common_handler(t, e);
  }
}

/* Test waking up a connection that is idle */
static void test_connection_wake(test_t *t) {
  proactor_test_t pts[] =  { { open_wake_handler }, { common_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);          /* Hold a port */
  pn_proactor_listen(server, pn_listener(), port.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port.sock);

  pn_connection_t *c = pn_connection();
  pn_proactor_connect(client, c, port.host_port);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, PROACTOR_TEST_RUN(pts));
  TEST_CHECK(t, pn_proactor_get(client) == NULL); /* Should be idle */
  pn_connection_wake(c);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_WAKE, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  /* The pn_connection_t is still valid so wake is legal but a no-op */
  pn_connection_wake(c);

  PROACTOR_TEST_FREE(pts);
  /* The pn_connection_t is still valid so wake is legal but a no-op */
  pn_connection_wake(c);
  pn_connection_free(c);
}

/* Test that INACTIVE event is generated when last connections/listeners closes. */
static void test_inactive(test_t *t) {
  proactor_test_t pts[] =  { { open_wake_handler }, { listen_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);          /* Hold a port */

  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, port.host_port,  4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  pn_connection_t *c = pn_connection();
  pn_proactor_connect(client, c, port.host_port);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, PROACTOR_TEST_RUN(pts));
  pn_connection_wake(c);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_WAKE, PROACTOR_TEST_RUN(pts));
  /* expect TRANSPORT_CLOSED from client and server, INACTIVE from client */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, PROACTOR_TEST_RUN(pts));
  /* server won't be INACTIVE until listener is closed */
  TEST_CHECK(t, pn_proactor_get(server) == NULL);
  pn_listener_close(l);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, PROACTOR_TEST_RUN(pts));
  pn_listener_free(l);

  sock_close(port.sock);
  PROACTOR_TEST_FREE(pts);
}

/* Tests for error handling */
static void test_errors(test_t *t) {
  proactor_test_t pts[] =  { { open_wake_handler }, { listen_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);          /* Hold a port */

  /* Invalid connect/listen parameters */
  pn_connection_t *c = pn_connection();
  pn_proactor_connect(client, c, "127.0.0.1:xxx");
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  TEST_STR_IN(t, "xxx", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, PROACTOR_TEST_RUN(pts));

  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, "127.0.0.1:xxx", 1);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, PROACTOR_TEST_RUN(pts));
  TEST_STR_IN(t, "xxx", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, PROACTOR_TEST_RUN(pts));
  pn_listener_free(l);

  /* Connect with no listener */
  c = pn_connection();
  pn_proactor_connect(client, c, port.host_port);
  if (TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts))) {
    TEST_STR_IN(t, "connection refused", last_condition);
    TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, PROACTOR_TEST_RUN(pts));
    sock_close(port.sock);
    PROACTOR_TEST_FREE(pts);
  }
}

/* Test that we can control listen/select on ipv6/v4 and listen on both by default */
static void test_ipv4_ipv6(test_t *t) {
  proactor_test_t pts[] ={ { open_close_handler }, { listen_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;

  /* Listen on all interfaces for IPv6 only. If this fails, skip IPv6 tests */
  test_port_t port6 = test_port("[::]");
  pn_listener_t *l6 = pn_listener();
  pn_proactor_listen(server, l6, port6.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port6.sock);
  pn_event_type_t e = PROACTOR_TEST_GET(pts);
  bool has_ipv6 = (e != PN_LISTENER_CLOSE);
  if (!has_ipv6) {
    TEST_LOGF(t, "skip IPv6 tests: %s", last_condition);
  }
  PROACTOR_TEST_DRAIN(pts);

  /* Listen on all interfaces for IPv4 only. */
  test_port_t port4 = test_port("0.0.0.0");
  pn_listener_t *l4 = pn_listener();
  pn_proactor_listen(server, l4, port4.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port4.sock);
  TEST_CHECKF(t, PROACTOR_TEST_GET(pts) != PN_LISTENER_CLOSE, "listener error: %s",  last_condition);
  PROACTOR_TEST_DRAIN(pts);

  /* Empty address listens on both IPv4 and IPv6 on all interfaces */
  test_port_t port = test_port("");
  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, port.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port.sock);
  e = PROACTOR_TEST_GET(pts);
  TEST_CHECKF(t, PROACTOR_TEST_GET(pts) != PN_LISTENER_CLOSE, "listener error: %s",  last_condition);
  PROACTOR_TEST_DRAIN(pts);

#define EXPECT_CONNECT(TP, HOST) do {                                   \
    pn_proactor_connect(client, pn_connection(), test_port_use_host(&(TP), (HOST))); \
    TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));    \
    TEST_STR_EQUAL(t, "", last_condition);                              \
    PROACTOR_TEST_DRAIN(pts);                                           \
  } while(0)

#define EXPECT_FAIL(TP, HOST) do {                                      \
    pn_proactor_connect(client, pn_connection(), test_port_use_host(&(TP), (HOST))); \
    TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));    \
    TEST_STR_IN(t, "refused", last_condition);                          \
    PROACTOR_TEST_DRAIN(pts);                                           \
  } while(0)

  EXPECT_CONNECT(port4, "127.0.0.1"); /* v4->v4 */
  EXPECT_CONNECT(port4, "");          /* local->v4*/

  EXPECT_CONNECT(port, "127.0.0.1"); /* v4->all */
  EXPECT_CONNECT(port, "");          /* local->all */

  if (has_ipv6) {
    EXPECT_CONNECT(port6, "[::]"); /* v6->v6 */
    EXPECT_CONNECT(port6, "");     /* local->v6 */
    EXPECT_CONNECT(port, "[::1]"); /* v6->all */

    EXPECT_FAIL(port6, "127.0.0.1"); /* fail v4->v6 */
    EXPECT_FAIL(port4, "[::1]");     /* fail v6->v4 */
  }
  PROACTOR_TEST_DRAIN(pts);

  pn_listener_close(l);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, PROACTOR_TEST_RUN(pts));
  pn_listener_free(l);

  pn_listener_close(l6);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, PROACTOR_TEST_RUN(pts));
  pn_listener_free(l6);

  pn_listener_close(l4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, PROACTOR_TEST_RUN(pts));
  pn_listener_free(l4);

  PROACTOR_TEST_FREE(pts);
}

/* Make sure pn_proactor_free cleans up open sockets */
static void test_free_cleanup(test_t *t) {
  proactor_test_t pts[] = { { open_wake_handler }, { listen_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t ports[3] = { test_port(localhost), test_port(localhost), test_port(localhost) };
  pn_listener_t *l[3];
  pn_connection_t *c[3];
  for (int i = 0; i < 3; ++i) {
    l[i] = pn_listener();
    pn_proactor_listen(server, l[i], ports[i].host_port, 2);
    TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
    sock_close(ports[i].sock);
    c[i] = pn_connection();
    pn_proactor_connect(client, c[i], ports[i].host_port);
  }
  PROACTOR_TEST_FREE(pts);
  /* Safe to free after proactor is gone */
  for (int i = 0; i < 3; ++i) {
    pn_listener_free(l[i]);
    pn_connection_free(c[i]);
  }
  /* Freeing an unused listener/connector should be safe */
  pn_listener_free(pn_listener());
  pn_connection_free(pn_connection());
}

/* TODO aconway 2017-03-27: need windows version with .p12 certs */
#define CERTFILE(NAME) CMAKE_CURRENT_SOURCE_DIR "/ssl_certs/" NAME ".pem"

static pn_event_type_t ssl_handler(test_t *t, pn_event_t *e) {
  pn_connection_t *c = pn_event_connection(e);
  switch (pn_event_type(e)) {

   case PN_CONNECTION_BOUND: {
     bool incoming = (pn_connection_state(c) & PN_LOCAL_UNINIT);
     pn_ssl_domain_t *ssld = pn_ssl_domain(incoming ? PN_SSL_MODE_SERVER : PN_SSL_MODE_CLIENT);
     TEST_CHECK(t, 0 == pn_ssl_domain_set_credentials(
                  ssld, CERTFILE("tserver-certificate"), CERTFILE("tserver-private-key"), "tserverpw"));
     TEST_CHECK(t, 0 == pn_ssl_init(pn_ssl(pn_event_transport(e)), ssld, NULL));
     pn_ssl_domain_free(ssld);
     return PN_EVENT_NONE;
   }

   case PN_CONNECTION_REMOTE_OPEN: {
     if (pn_connection_state(c) | PN_LOCAL_ACTIVE) {
       /* Outgoing connection is complete, close it */
       pn_connection_close(c);
     } else {
       /* Incoming connection, check for SSL */
       pn_ssl_t *ssl = pn_ssl(pn_event_transport(e));
       TEST_CHECK(t, ssl);
       TEST_CHECK(t, pn_ssl_get_protocol_name(ssl, NULL, 0));
       pn_connection_open(c);      /* Return the open (no-op if already open) */
     }
    return PN_CONNECTION_REMOTE_OPEN;
   }

   default:
    return common_handler(t, e);
  }
}

/* Establish an SSL connection between proactors*/
static void test_ssl(test_t *t) {
  if (!pn_ssl_present()) {
    TEST_LOGF(t, "Skip SSL test, no support");
    return;
  }

  proactor_test_t pts[] ={ { ssl_handler }, { ssl_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);
  pn_proactor_listen(server, pn_listener(), port.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  sock_close(port.sock);
  pn_connection_t *c = pn_connection();
  pn_proactor_connect(client, c, port.host_port);
  /* Open ok at both ends */
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, PROACTOR_TEST_RUN(pts));
  TEST_STR_EQUAL(t,"", last_condition);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, PROACTOR_TEST_RUN(pts));
  TEST_STR_EQUAL(t, "", last_condition);
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, PROACTOR_TEST_RUN(pts));

  PROACTOR_TEST_FREE(pts);
}

/* Test pn_proactor_addr funtions */
static void test_addr(test_t *t) {
  /* Make sure NULL addr gives empty string */
  char str[1024] = "not-empty";
  pn_proactor_addr_str(str, sizeof(str), NULL);
  TEST_STR_EQUAL(t, "", str);

  proactor_test_t pts[] ={ { open_wake_handler }, { listen_handler } };
  PROACTOR_TEST_INIT(pts, t);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port(localhost);
  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, port.host_port, 4);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, PROACTOR_TEST_RUN(pts));
  pn_connection_t *c = pn_connection();
  pn_proactor_connect(client, c, port.host_port);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, PROACTOR_TEST_RUN(pts));

  /* client remote, client local, server remote and server local address strings */
  char cr[1024], cl[1024], sr[1024], sl[1024];

  pn_transport_t *ct = pn_connection_transport(c);
  pn_proactor_addr_str(cr, sizeof(cr), pn_proactor_addr_remote(ct));
  TEST_STR_IN(t, test_port_use_host(&port, ""), cr); /* remote address has listening port */

  pn_connection_t *s = last_accepted; /* server side of the connection */
  pn_transport_t *st = pn_connection_transport(s);
  if (!TEST_CHECK(t, st)) return;
  pn_proactor_addr_str(sl, sizeof(sl), pn_proactor_addr_local(st));
  TEST_STR_EQUAL(t, cr, sl);  /* client remote == server local */

  pn_proactor_addr_str(cl, sizeof(cl), pn_proactor_addr_local(ct));
  pn_proactor_addr_str(sr, sizeof(sr), pn_proactor_addr_remote(st));
  TEST_STR_EQUAL(t, cl, sr);    /* client local == server remote */

  /* Make sure you can use NULL, 0 to get length of address string without a crash */
  size_t len = pn_proactor_addr_str(NULL, 0, pn_proactor_addr_local(ct));
  TEST_CHECK(t, strlen(cl) == len);

  sock_close(port.sock);
  PROACTOR_TEST_DRAIN(pts);
  PROACTOR_TEST_FREE(pts);
  pn_listener_free(l);
  pn_connection_free(c);
  pn_connection_free(s);
}

int main(int argc, char **argv) {
  int failed = 0;
  RUN_ARGV_TEST(failed, t, test_inactive(&t));
  RUN_ARGV_TEST(failed, t, test_interrupt_timeout(&t));
  RUN_ARGV_TEST(failed, t, test_errors(&t));
  RUN_ARGV_TEST(failed, t, test_client_server(&t));
  RUN_ARGV_TEST(failed, t, test_connection_wake(&t));
  RUN_ARGV_TEST(failed, t, test_ipv4_ipv6(&t));
  RUN_ARGV_TEST(failed, t, test_free_cleanup(&t));
  RUN_ARGV_TEST(failed, t, test_ssl(&t));
  RUN_ARGV_TEST(failed, t, test_addr(&t));
  return failed;
}
