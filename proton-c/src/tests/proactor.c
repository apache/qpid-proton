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
#include "test_handler.h"
#include "test_config.h"
#include "../proactor/proactor-internal.h"

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/event.h>
#include <proton/listener.h>
#include <proton/session.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/ssl.h>
#include <proton/transport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAYLEN(A) (sizeof(A)/sizeof((A)[0]))

/* Proactor and handler that take part in a test */
typedef struct test_proactor_t {
  test_handler_t handler;
  pn_proactor_t *proactor;
} test_proactor_t;

static test_proactor_t test_proactor(test_t *t, test_handler_fn f) {
  test_proactor_t tp;
  test_handler_init(&tp.handler, t, f);
  tp.proactor = pn_proactor();
  TEST_ASSERT(tp.proactor);
  return tp;
}

static void test_proactor_destroy(test_proactor_t *tp) {
  pn_proactor_free(tp->proactor);
}

/* Set this to a pn_condition() to save condition data */
pn_condition_t *last_condition = NULL;

static void save_condition(pn_event_t *e) {
  if (last_condition) {
    pn_condition_t *cond = NULL;
    if (pn_event_listener(e)) {
      cond = pn_listener_condition(pn_event_listener(e));
    } else {
      cond = pn_event_condition(e);
    }
    if (cond) {
      pn_condition_copy(last_condition, cond);
    } else {
      pn_condition_clear(last_condition);
    }
  }
}

/* Process events on a proactor array until a handler returns an event, or
 * all proactors return NULL
 */
static pn_event_type_t test_proactors_get(test_proactor_t *tps, size_t n) {
  if (last_condition) pn_condition_clear(last_condition);
  while (true) {
    bool busy = false;
    for (test_proactor_t *tp = tps; tp < tps + n; ++tp) {
      pn_event_batch_t *eb =  pn_proactor_get(tp->proactor);
      if (eb) {
        busy = true;
        pn_event_type_t ret = PN_EVENT_NONE;
        for (pn_event_t* e = pn_event_batch_next(eb); e; e = pn_event_batch_next(eb)) {
          test_handler_log(&tp->handler, e);
          save_condition(e);
          ret = tp->handler.f(&tp->handler, e);
          if (ret) break;
        }
        pn_proactor_done(tp->proactor, eb);
        if (ret) return ret;
      }
    }
    if (!busy) {
      return PN_EVENT_NONE;
    }
  }
}

/* Run an array of proactors till a handler returns an event. */
static pn_event_type_t test_proactors_run(test_proactor_t *tps, size_t n) {
  pn_event_type_t e;
  while ((e = test_proactors_get(tps, n)) == PN_EVENT_NONE)
         ;
  return e;
}

/* Run an array of proactors till a handler returns the desired event. */
void test_proactors_run_until(test_proactor_t *tps, size_t n, pn_event_type_t want) {
  while (test_proactors_get(tps, n) != want)
         ;
}

/* Drain and discard outstanding events from an array of proactors */
static void test_proactors_drain(test_proactor_t *tps, size_t n) {
  while (test_proactors_get(tps, n))
         ;
}


#define TEST_PROACTORS_GET(A) test_proactors_get((A), ARRAYLEN(A))
#define TEST_PROACTORS_RUN(A) test_proactors_run((A), ARRAYLEN(A))
#define TEST_PROACTORS_RUN_UNTIL(A, WANT) test_proactors_run_until((A), ARRAYLEN(A), WANT)
#define TEST_PROACTORS_DRAIN(A) test_proactors_drain((A), ARRAYLEN(A))

#define TEST_PROACTORS_DESTROY(A) do {           \
    for (size_t i = 0; i < ARRAYLEN(A); ++i)       \
      test_proactor_destroy((A)+i);             \
  } while (0)


#define MAX_STR 256
struct addrinfo {
  char host[MAX_STR];
  char port[MAX_STR];
  char connect[MAX_STR];
  char host_port[MAX_STR];
};

struct addrinfo listener_info(pn_listener_t *l) {
  struct addrinfo ai = {{0}};
  const pn_netaddr_t *na = pn_netaddr_listening(l);
  TEST_ASSERT(0 == pn_netaddr_host_port(na, ai.host, sizeof(ai.host), ai.port, sizeof(ai.port)));
  for (na = pn_netaddr_next(na); na; na = pn_netaddr_next(na)) { /* Check that ports are consistent */
    char port[MAX_STR];
    TEST_ASSERT(0 == pn_netaddr_host_port(na, NULL, 0, port, sizeof(port)));
    TEST_ASSERTF(0 == strcmp(port, ai.port), "%s !=  %s", port, ai.port);
  }
  (void)pn_proactor_addr(ai.connect, sizeof(ai.connect), "", ai.port); /* Address for connecting */
  (void)pn_netaddr_str(na, ai.host_port, sizeof(ai.host_port)); /* host:port listening address */
  return ai;
}

/* Return a pn_listener_t*, raise errors if not successful */
pn_listener_t *test_listen(test_proactor_t *tp, const char *host) {
  char addr[1024];
  pn_listener_t *l = pn_listener();
  (void)pn_proactor_addr(addr, sizeof(addr), host, "0");
  pn_proactor_listen(tp->proactor, l, addr, 4);
  TEST_ETYPE_EQUAL(tp->handler.t, PN_LISTENER_OPEN, test_proactors_run(tp, 1));
  TEST_COND_EMPTY(tp->handler.t, last_condition);
  return l;
}


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
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, wait_next(p)); /* Inactive because timeout expired */

  /* Set a (very short) timeout */
  pn_proactor_set_timeout(p, 1);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_TIMEOUT, wait_next(p));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, wait_next(p));

  /* Set and cancel a timeout, make sure we don't get the timeout event */
  pn_proactor_set_timeout(p, 10000000);
  pn_proactor_cancel_timeout(p);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, wait_next(p));
  TEST_CHECK(t, pn_proactor_get(p) == NULL); /* idle */

  pn_proactor_free(p);
}

/* Save the last connection accepted by the common_handler */
pn_connection_t *last_accepted = NULL;

/* Common handler for simple client/server interactions,  */
static pn_event_type_t common_handler(test_handler_t *th, pn_event_t *e) {
  pn_connection_t *c = pn_event_connection(e);
  pn_listener_t *l = pn_event_listener(e);

  switch (pn_event_type(e)) {

    /* Stop on these events */
   case PN_TRANSPORT_CLOSED:
   case PN_PROACTOR_INACTIVE:
   case PN_PROACTOR_TIMEOUT:
   case PN_LISTENER_OPEN:
    return pn_event_type(e);

   case PN_LISTENER_ACCEPT:
    last_accepted = pn_connection();
    pn_listener_accept2(l, last_accepted, NULL);
    pn_listener_close(l);       /* Only accept one connection */
    return PN_EVENT_NONE;

   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(c);      /* Return the open (no-op if already open) */
    return PN_EVENT_NONE;

   case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(e));
    return PN_EVENT_NONE;

   case PN_LINK_REMOTE_OPEN:
    pn_link_open(pn_event_link(e));
    return PN_EVENT_NONE;

   case PN_CONNECTION_REMOTE_CLOSE:
    pn_connection_close(c);     /* Return the close */
    return PN_EVENT_NONE;

    /* Ignore these events */
   case PN_CONNECTION_BOUND:
   case PN_CONNECTION_INIT:
   case PN_CONNECTION_LOCAL_CLOSE:
   case PN_CONNECTION_LOCAL_OPEN:
   case PN_LINK_INIT:
   case PN_LINK_LOCAL_OPEN:
   case PN_LISTENER_CLOSE:
   case PN_SESSION_INIT:
   case PN_SESSION_LOCAL_OPEN:
   case PN_TRANSPORT:
   case PN_TRANSPORT_ERROR:
   case PN_TRANSPORT_HEAD_CLOSED:
   case PN_TRANSPORT_TAIL_CLOSED:
    return PN_EVENT_NONE;

   default:
    TEST_ERRORF(th->t, "unexpected event %s", pn_event_type_name(pn_event_type(e)));
    return PN_EVENT_NONE;                   /* Fail the test but keep going */
  }
}

/* Like common_handler but does not auto-close the listener after one accept,
   and returns on LISTENER_CLOSE
*/
static pn_event_type_t listen_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_LISTENER_ACCEPT:
    /* No automatic listener close/free for tests that accept multiple connections */
    last_accepted = pn_connection();
    pn_listener_accept2(pn_event_listener(e), last_accepted, NULL);
    /* No automatic close */
    return PN_EVENT_NONE;

   case PN_LISTENER_CLOSE:
    return PN_LISTENER_CLOSE;

   default:
    return common_handler(th, e);
  }
}

/* close a connection when it is remote open */
static pn_event_type_t open_close_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_close(pn_event_connection(e));
    return PN_EVENT_NONE;          /* common_handler will finish on TRANSPORT_CLOSED */
   default:
    return common_handler(th, e);
  }
}

/* Test simple client/server connection with 2 proactors */
static void test_client_server(test_t *t) {
  test_proactor_t tps[] ={ test_proactor(t, open_close_handler), test_proactor(t, common_handler) };
  pn_listener_t *l = test_listen(&tps[1], "");
  /* Connect and wait for close at both ends */
  pn_proactor_connect2(tps[0].proactor, NULL, NULL, listener_info(l).connect);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);  
  TEST_PROACTORS_DESTROY(tps);
}

/* Return on connection open, close and return on wake */
static pn_event_type_t open_wake_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    return pn_event_type(e);
   case PN_CONNECTION_WAKE:
    pn_connection_close(pn_event_connection(e));
    return pn_event_type(e);
   default:
    return common_handler(th, e);
  }
}

/* Test waking up a connection that is idle */
static void test_connection_wake(test_t *t) {
  test_proactor_t tps[] =  { test_proactor(t, open_wake_handler), test_proactor(t,  listen_handler) };
  pn_proactor_t *client = tps[0].proactor;
  pn_listener_t *l = test_listen(&tps[1], "");

  pn_connection_t *c = pn_connection();
  pn_incref(c);                 /* Keep a reference for wake() after free */
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_CHECK(t, pn_proactor_get(client) == NULL); /* Should be idle */
  pn_connection_wake(c);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_WAKE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps)); /* Both ends */
  /* The pn_connection_t is still valid so wake is legal but a no-op */
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_EVENT_NONE, TEST_PROACTORS_GET(tps)); /* No more wake */

  /* Verify we don't get a wake after close even if they happen together */
  pn_connection_t *c2 = pn_connection();
  pn_proactor_connect2(client, c2, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_connection_wake(c2);
  pn_proactor_disconnect(client, NULL);
  pn_connection_wake(c2);

  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, test_proactors_run(&tps[0], 1));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, test_proactors_run(&tps[0], 1));
  TEST_ETYPE_EQUAL(t, PN_EVENT_NONE, test_proactors_get(&tps[0], 1)); /* No late wake */

  TEST_PROACTORS_DESTROY(tps);
  /* The pn_connection_t is still valid so wake is legal but a no-op */
  pn_connection_wake(c);
  pn_decref(c);
}

/* Close the transport to abort a connection, i.e. close the socket without an AMQP close */
static pn_event_type_t listen_abort_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    /* Close the transport - abruptly closes the socket */
    pn_transport_close_tail(pn_connection_transport(pn_event_connection(e)));
    pn_transport_close_head(pn_connection_transport(pn_event_connection(e)));
    return PN_EVENT_NONE;

   default:
    /* Don't auto-close the listener to keep the event sequences simple */
    return listen_handler(th, e);
  }
}

/* Verify that pn_transport_close_head/tail aborts a connection without an AMQP protocol close */
static void test_abort(test_t *t) {
  test_proactor_t tps[] = { test_proactor(t, open_close_handler), test_proactor(t, listen_abort_handler) };
  pn_proactor_t *client = tps[0].proactor;
  pn_listener_t *l = test_listen(&tps[1], "");
  pn_proactor_connect2(client, NULL, NULL, listener_info(l).connect);

  /* server transport closes */
  if (TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps))) {
    TEST_COND_NAME(t, "amqp:connection:framing-error",last_condition);
    TEST_COND_DESC(t, "abort", last_condition);
  }
  /* client transport closes */
  if (TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps))) {
    TEST_COND_NAME(t, "amqp:connection:framing-error", last_condition);
    TEST_COND_DESC(t, "abort", last_condition);
  }

  pn_listener_close(l);

  while (TEST_PROACTORS_RUN(tps) != PN_PROACTOR_INACTIVE) {}
  while (TEST_PROACTORS_RUN(tps) != PN_PROACTOR_INACTIVE) {}

  /* Verify expected event sequences, no unexpected events */
  TEST_HANDLER_EXPECT(
    &tps[0].handler,
    PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_BOUND,
    PN_TRANSPORT_TAIL_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
    PN_PROACTOR_INACTIVE,
    0);

  TEST_HANDLER_EXPECT(
    &tps[1].handler,
    PN_LISTENER_OPEN, PN_LISTENER_ACCEPT,
    PN_CONNECTION_INIT, PN_CONNECTION_BOUND, PN_CONNECTION_REMOTE_OPEN,
    PN_TRANSPORT_TAIL_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
    PN_LISTENER_CLOSE,
    PN_PROACTOR_INACTIVE,
    0);

  TEST_PROACTORS_DESTROY(tps);
}

/* Refuse a connection: abort before the AMQP open sequence begins. */
static pn_event_type_t listen_refuse_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {

   case PN_CONNECTION_BOUND:
    /* Close the transport - abruptly closes the socket */
    pn_transport_close_tail(pn_connection_transport(pn_event_connection(e)));
    pn_transport_close_head(pn_connection_transport(pn_event_connection(e)));
    return PN_EVENT_NONE;

   default:
    /* Don't auto-close the listener to keep the event sequences simple */
    return listen_handler(th, e);
  }
}

/* Verify that pn_transport_close_head/tail aborts a connection without an AMQP protocol close */
static void test_refuse(test_t *t) {
  test_proactor_t tps[] = { test_proactor(t, open_close_handler), test_proactor(t, listen_refuse_handler) };
  pn_proactor_t *client = tps[0].proactor;
  pn_listener_t *l = test_listen(&tps[1], "");
  pn_proactor_connect2(client, NULL, NULL, listener_info(l).connect);

  /* client transport closes */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps)); /* client */
  TEST_COND_NAME(t, "amqp:connection:framing-error", last_condition);

  pn_listener_close(l);
  while (TEST_PROACTORS_RUN(tps) != PN_PROACTOR_INACTIVE) {}
  while (TEST_PROACTORS_RUN(tps) != PN_PROACTOR_INACTIVE) {}

  /* Verify expected event sequences, no unexpected events */
  TEST_HANDLER_EXPECT(
    &tps[0].handler,
    PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_BOUND,
    PN_TRANSPORT_TAIL_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
    PN_PROACTOR_INACTIVE,
    0);

  TEST_HANDLER_EXPECT(
    &tps[1].handler,
    PN_LISTENER_OPEN, PN_LISTENER_ACCEPT,
    PN_CONNECTION_INIT, PN_CONNECTION_BOUND,
    PN_TRANSPORT_TAIL_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
    PN_LISTENER_CLOSE,
    PN_PROACTOR_INACTIVE,
    0);

  TEST_PROACTORS_DESTROY(tps);
}

/* Test that INACTIVE event is generated when last connections/listeners closes. */
static void test_inactive(test_t *t) {
  test_proactor_t tps[] =  { test_proactor(t, open_wake_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor, *server = tps[1].proactor;

  /* Listen, connect, disconnect */
  pn_listener_t *l = test_listen(&tps[1], "");
  pn_connection_t *c = pn_connection();
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_connection_wake(c);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_WAKE, TEST_PROACTORS_RUN(tps));
  /* Expect TRANSPORT_CLOSED from client and server, INACTIVE from client */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Immediate timer generates INACTIVE on client (no connections) */
  pn_proactor_set_timeout(client, 0);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_TIMEOUT, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Connect, set-timer, disconnect */
  pn_proactor_set_timeout(client, 1000000);
  c = pn_connection();
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_connection_wake(c);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_WAKE, TEST_PROACTORS_RUN(tps));
  /* Expect TRANSPORT_CLOSED from client and server */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  /* No INACTIVE till timer is cancelled */
  TEST_CHECK(t, pn_proactor_get(server) == NULL);
  pn_proactor_cancel_timeout(client);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Server won't be INACTIVE until listener is closed */
  TEST_CHECK(t, pn_proactor_get(server) == NULL);
  pn_listener_close(l);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  TEST_PROACTORS_DESTROY(tps);
}

/* Tests for error handling */
static void test_errors(test_t *t) {
  test_proactor_t tps[] =  { test_proactor(t, open_wake_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor, *server = tps[1].proactor;

  /* Invalid connect/listen service name */
  pn_connection_t *c = pn_connection();
  pn_proactor_connect2(client, c, NULL, "127.0.0.1:xxx");
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_COND_DESC(t, "xxx", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  pn_proactor_listen(server, pn_listener(), "127.0.0.1:xxx", 1);
  TEST_PROACTORS_RUN(tps);
  TEST_HANDLER_EXPECT(&tps[1].handler, PN_LISTENER_CLOSE, 0); /* CLOSE only, no OPEN */
  TEST_COND_DESC(t, "xxx", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Invalid connect/listen host name */
  c = pn_connection();
  pn_proactor_connect2(client, c, NULL, "nosuch.example.com:");
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_COND_DESC(t, "nosuch", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  test_handler_clear(&tps[1].handler, 0);
  pn_proactor_listen(server, pn_listener(), "nosuch.example.com:", 1);
  TEST_PROACTORS_RUN(tps);
  TEST_HANDLER_EXPECT(&tps[1].handler, PN_LISTENER_CLOSE, 0); /* CLOSE only, no OPEN */
  TEST_COND_DESC(t, "nosuch", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Listen on a port already in use */
  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, ":0", 1);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_OPEN, TEST_PROACTORS_RUN(tps));
  test_handler_clear(&tps[1].handler, 0);
  struct addrinfo laddr = listener_info(l);
  pn_proactor_listen(server, pn_listener(), laddr.connect, 1); /* Busy */
  TEST_PROACTORS_RUN(tps);
  TEST_HANDLER_EXPECT(&tps[1].handler, PN_LISTENER_CLOSE, 0); /* CLOSE only, no OPEN */
  TEST_COND_NAME(t, "proton:io", last_condition);
  pn_listener_close(l);
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Connect with no listener */
  c = pn_connection();
  pn_proactor_connect2(client, c, NULL, laddr.connect);
  if (TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps))) {
    TEST_COND_DESC(t, "refused", last_condition);
    TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));
  }

  TEST_PROACTORS_DESTROY(tps);
}

/* Closing the connection during PN_TRANSPORT_ERROR should be a no-op
 * Regression test for: https://issues.apache.org/jira/browse/PROTON-1586
 */
static pn_event_type_t transport_close_connection_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_TRANSPORT_ERROR:
    pn_connection_close(pn_event_connection(e));
    break;
   default:
    return open_wake_handler(th, e);
  }
  return PN_EVENT_NONE;
}

/* Closing the connection during PN_TRANSPORT_ERROR due to connection failure should be a no-op
 * Regression test for: https://issues.apache.org/jira/browse/PROTON-1586
 */
static void test_proton_1586(test_t *t) {
  test_proactor_t tps[] =  { test_proactor(t, transport_close_connection_handler) };
  pn_proactor_connect2(tps[0].proactor, NULL, NULL, ":yyy");
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_COND_DESC(t, ":yyy", last_condition);
  test_handler_clear(&tps[0].handler, 0); /* Clear events */
  /* There should be no events generated after PN_TRANSPORT_CLOSED */
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));
  TEST_HANDLER_EXPECT(&tps[0].handler,PN_PROACTOR_INACTIVE, 0);

  TEST_PROACTORS_DESTROY(tps);
}

/* Test that we can control listen/select on ipv6/v4 and listen on both by default */
static void test_ipv4_ipv6(test_t *t) {
  test_proactor_t tps[] ={ test_proactor(t, open_close_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor, *server = tps[1].proactor;

  /* Listen on all interfaces for IPv4 only. */
  pn_listener_t *l4 = test_listen(&tps[1], "0.0.0.0");
  TEST_PROACTORS_DRAIN(tps);

  /* Empty address listens on both IPv4 and IPv6 on all interfaces */
  pn_listener_t *l = test_listen(&tps[1], "");
  TEST_PROACTORS_DRAIN(tps);

#define EXPECT_CONNECT(LISTENER, HOST) do {                             \
    char addr[1024];                                                    \
    pn_proactor_addr(addr, sizeof(addr), HOST, listener_info(LISTENER).port); \
    pn_proactor_connect2(client, NULL, NULL, addr);                     \
    TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));  \
    TEST_COND_EMPTY(t, last_condition);                                 \
    TEST_PROACTORS_DRAIN(tps);                                          \
  } while(0)

  EXPECT_CONNECT(l4, "127.0.0.1"); /* v4->v4 */
  EXPECT_CONNECT(l4, "");          /* local->v4*/

  EXPECT_CONNECT(l, "127.0.0.1"); /* v4->all */
  EXPECT_CONNECT(l, "");          /* local->all */

  /* Listen on ipv6 loopback, if it fails skip ipv6 tests.

     NOTE: Don't use the unspecified address "::" here - ipv6-disabled platforms
     may allow listening on "::" without complaining. However they won't have a
     local ipv6 loopback configured, so "::1" will force an error.
  */
  TEST_PROACTORS_DRAIN(tps);
  pn_listener_t *l6 = pn_listener();
  pn_proactor_listen(server, l6, "::1:0", 4);
  pn_event_type_t e = TEST_PROACTORS_RUN(tps);
  if (e == PN_LISTENER_OPEN && !pn_condition_is_set(last_condition)) {
    TEST_PROACTORS_DRAIN(tps);

    EXPECT_CONNECT(l6, "::1"); /* v6->v6 */
    EXPECT_CONNECT(l6, "");    /* local->v6 */
    EXPECT_CONNECT(l, "::1");  /* v6->all */

    pn_listener_close(l6);
  } else  {
    const char *d = pn_condition_get_description(last_condition);
    TEST_LOGF(t, "skip IPv6 tests: %s %s", pn_event_type_name(e), d ? d : "no condition");
  }

  pn_listener_close(l);
  pn_listener_close(l4);
  TEST_PROACTORS_DESTROY(tps);
}

/* Make sure we clean up released connections and open sockets correctly */
static void test_release_free(test_t *t) {
  test_proactor_t tps[] = { test_proactor(t, open_wake_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor;
  pn_listener_t *l = test_listen(&tps[1], "");

  /* leave one connection to the proactor  */
  pn_proactor_connect2(client, NULL, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));

  /* release c1 and free immediately */
  pn_connection_t *c1 = pn_connection();
  pn_proactor_connect2(client, c1, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_proactor_release_connection(c1); /* We free but socket should still be cleaned up */
  pn_connection_free(c1);
  TEST_CHECK(t, pn_proactor_get(client) == NULL); /* Should be idle */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps)); /* Server closed */

  /* release c2 and but don't free till after proactor free */
  pn_connection_t *c2 = pn_connection();
  pn_proactor_connect2(client, c2, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_proactor_release_connection(c2);
  TEST_CHECK(t, pn_proactor_get(client) == NULL); /* Should be idle */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps)); /* Server closed */

  TEST_PROACTORS_DESTROY(tps);
  pn_connection_free(c2);

  /* Check freeing a listener or connection that was never given to a proactor */
  pn_listener_free(pn_listener());
  pn_connection_free(pn_connection());
}

#define SSL_FILE(NAME) CMAKE_CURRENT_SOURCE_DIR "/ssl_certs/" NAME
#define SSL_PW "tserverpw"
/* Windows vs. OpenSSL certificates */
#if defined(_WIN32)
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.p12")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_ssl_domain_set_credentials(DOMAIN, SSL_FILE(NAME "-full.p12"), "", SSL_PW)
#else
#  define CERTIFICATE(NAME) SSL_FILE(NAME "-certificate.pem")
#  define SET_CREDENTIALS(DOMAIN, NAME)                                 \
  pn_ssl_domain_set_credentials(DOMAIN, CERTIFICATE(NAME), SSL_FILE(NAME "-private-key.pem"), SSL_PW)
#endif

static pn_event_type_t ssl_handler(test_handler_t *h, pn_event_t *e) {
  switch (pn_event_type(e)) {

   case PN_CONNECTION_BOUND:
    TEST_CHECK(h->t, 0 == pn_ssl_init(pn_ssl(pn_event_transport(e)), h->ssl_domain, NULL));
    return PN_EVENT_NONE;

   case PN_CONNECTION_REMOTE_OPEN: {
     pn_ssl_t *ssl = pn_ssl(pn_event_transport(e));
     TEST_CHECK(h->t, ssl);
     char protocol[256];
     TEST_CHECK(h->t, pn_ssl_get_protocol_name(ssl, protocol, sizeof(protocol)));
     TEST_STR_IN(h->t, "TLS", protocol);
     return PN_CONNECTION_REMOTE_OPEN;
   }
   default:
    return PN_EVENT_NONE;
  }
}

static pn_event_type_t ssl_server_handler(test_handler_t *h, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_BOUND:
    return ssl_handler(h, e);
   case PN_CONNECTION_REMOTE_OPEN: {
     pn_event_type_t et = ssl_handler(h, e);
     pn_connection_open(pn_event_connection(e));
     return et;
   }
   default:
    return listen_handler(h, e);
  }
}

static pn_event_type_t ssl_client_handler(test_handler_t *h, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_BOUND:
    return ssl_handler(h, e);
   case PN_CONNECTION_REMOTE_OPEN: {
     pn_event_type_t et = ssl_handler(h, e);
     pn_connection_close(pn_event_connection(e));
     return et;
   }
    break;
   default:
    return common_handler(h, e);
  }
}

/* Test various SSL connections between proactors*/
static void test_ssl(test_t *t) {
  if (!pn_ssl_present()) {
    TEST_LOGF(t, "Skip SSL test, no support");
    return;
  }

  test_proactor_t tps[] ={ test_proactor(t, ssl_client_handler), test_proactor(t, ssl_server_handler) };
  test_proactor_t *client = &tps[0], *server = &tps[1];
  pn_ssl_domain_t *cd = client->handler.ssl_domain = pn_ssl_domain(PN_SSL_MODE_CLIENT);
  pn_ssl_domain_t *sd =  server->handler.ssl_domain = pn_ssl_domain(PN_SSL_MODE_SERVER);
  TEST_CHECK(t, 0 == SET_CREDENTIALS(sd, "tserver"));
  pn_listener_t *l = test_listen(server, "");

  /* Basic SSL connection */
  pn_proactor_connect2(client->proactor, NULL, NULL, listener_info(l).connect);
  /* Open ok at both ends */
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_COND_EMPTY(t, last_condition);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_COND_EMPTY(t, last_condition);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);

  /* Verify peer with good hostname */
  TEST_INT_EQUAL(t, 0, pn_ssl_domain_set_trusted_ca_db(cd, CERTIFICATE("tserver")));
  TEST_INT_EQUAL(t, 0, pn_ssl_domain_set_peer_authentication(cd, PN_SSL_VERIFY_PEER_NAME, NULL));
  pn_connection_t *c = pn_connection();
  pn_connection_set_hostname(c, "test_server");
  pn_proactor_connect2(client->proactor, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_COND_EMPTY(t, last_condition);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_COND_EMPTY(t, last_condition);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_TRANSPORT_CLOSED);

  /* Verify peer with bad hostname */
  c = pn_connection();
  pn_connection_set_hostname(c, "wrongname");
  pn_proactor_connect2(client->proactor, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_COND_NAME(t, "amqp:connection:framing-error",  last_condition);
  TEST_COND_DESC(t, "SSL",  last_condition);
  TEST_PROACTORS_DRAIN(tps);

  pn_ssl_domain_free(cd);
  pn_ssl_domain_free(sd);
  TEST_PROACTORS_DESTROY(tps);
}

static void test_proactor_addr(test_t *t) {
  /* Test the address formatter */
  char addr[PN_MAX_ADDR];
  pn_proactor_addr(addr, sizeof(addr), "foo", "bar");
  TEST_STR_EQUAL(t, "foo:bar", addr);
  pn_proactor_addr(addr, sizeof(addr), "foo", "");
  TEST_STR_EQUAL(t, "foo:", addr);
  pn_proactor_addr(addr, sizeof(addr), "foo", NULL);
  TEST_STR_EQUAL(t, "foo:", addr);
  pn_proactor_addr(addr, sizeof(addr), "", "bar");
  TEST_STR_EQUAL(t, ":bar", addr);
  pn_proactor_addr(addr, sizeof(addr), NULL, "bar");
  TEST_STR_EQUAL(t, ":bar", addr);
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", "5");
  TEST_STR_EQUAL(t, "1:2:3:4:5", addr);
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", "");
  TEST_STR_EQUAL(t, "1:2:3:4:", addr);
  pn_proactor_addr(addr, sizeof(addr), "1:2:3:4", NULL);
  TEST_STR_EQUAL(t, "1:2:3:4:", addr);
}

static void test_parse_addr(test_t *t) {
  char buf[1024];
  const char *host, *port;

  TEST_CHECK(t, 0 == pni_parse_addr("foo:bar", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "foo", host);
  TEST_STR_EQUAL(t, "bar", port);

  TEST_CHECK(t, 0 == pni_parse_addr("foo:", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "foo", host);
  TEST_STR_EQUAL(t, "5672", port);

  TEST_CHECK(t, 0 == pni_parse_addr(":bar", buf, sizeof(buf), &host, &port));
  TEST_CHECKF(t, NULL == host, "expected null, got: %s", host);
  TEST_STR_EQUAL(t, "bar", port);

  TEST_CHECK(t, 0 == pni_parse_addr(":", buf, sizeof(buf), &host, &port));
  TEST_CHECKF(t, NULL == host, "expected null, got: %s", host);
  TEST_STR_EQUAL(t, "5672", port);

  TEST_CHECK(t, 0 == pni_parse_addr(":amqps", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "5671", port);

  TEST_CHECK(t, 0 == pni_parse_addr(":amqp", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "5672", port);

  TEST_CHECK(t, 0 == pni_parse_addr("::1:2:3", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "::1:2", host);
  TEST_STR_EQUAL(t, "3", port);

  TEST_CHECK(t, 0 == pni_parse_addr(":::", buf, sizeof(buf), &host, &port));
  TEST_STR_EQUAL(t, "::", host);
  TEST_STR_EQUAL(t, "5672", port);

  TEST_CHECK(t, 0 == pni_parse_addr("", buf, sizeof(buf), &host, &port));
  TEST_CHECKF(t, NULL == host, "expected null, got: %s", host);
  TEST_STR_EQUAL(t, "5672", port);
}

/* Test pn_proactor_addr functions */

static void test_netaddr(test_t *t) {
  test_proactor_t tps[] ={ test_proactor(t, open_wake_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor;
  /* Use IPv4 to get consistent results all platforms */
  pn_listener_t *l = test_listen(&tps[1], "127.0.0.1");
  pn_connection_t *c = pn_connection();
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  if (!TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps))) {
    TEST_COND_EMPTY(t, last_condition); /* Show the last condition */
    return;                     /* don't continue if connection is closed */
  }

  /* client remote, client local, server remote and server local address strings */
  char cr[1024], cl[1024], sr[1024], sl[1024];

  pn_transport_t *ct = pn_connection_transport(c);
  const pn_netaddr_t *na = pn_netaddr_remote(ct);
  pn_netaddr_str(na, cr, sizeof(cr));
  TEST_STR_IN(t, listener_info(l).port, cr); /* remote address has listening port */

  pn_connection_t *s = last_accepted; /* server side of the connection */

  pn_transport_t *st = pn_connection_transport(s);
  if (!TEST_CHECK(t, st)) return;
  pn_netaddr_str(pn_netaddr_local(st), sl, sizeof(sl));
  TEST_STR_EQUAL(t, cr, sl);  /* client remote == server local */

  pn_netaddr_str(pn_netaddr_local(ct), cl, sizeof(cl));
  pn_netaddr_str(pn_netaddr_remote(st), sr, sizeof(sr));
  TEST_STR_EQUAL(t, cl, sr);    /* client local == server remote */

  char host[MAX_STR] = "";
  char serv[MAX_STR] = "";
  int err = pn_netaddr_host_port(na, host, sizeof(host), serv, sizeof(serv));
  TEST_CHECK(t, 0 == err);
  TEST_STR_EQUAL(t, "127.0.0.1", host);
  TEST_STR_EQUAL(t, listener_info(l).port, serv);

  /* Make sure you can use NULL, 0 to get length of address string without a crash */
  size_t len = pn_netaddr_str(pn_netaddr_local(ct), NULL, 0);
  TEST_CHECKF(t, strlen(cl) == len, "%d != %d", strlen(cl), len);

  TEST_PROACTORS_DRAIN(tps);
  TEST_PROACTORS_DESTROY(tps);
}

/* Test pn_proactor_disconnect */
static void test_disconnect(test_t *t) {
  test_proactor_t tps[] ={ test_proactor(t, open_wake_handler), test_proactor(t, listen_handler) };
  pn_proactor_t *client = tps[0].proactor, *server = tps[1].proactor;

  /* Start two listeners */
  pn_listener_t *l = test_listen(&tps[1], "");
  pn_listener_t *l2 = test_listen(&tps[1], "");

  /* Only wait for one connection to remote-open before disconnect */
  pn_connection_t *c = pn_connection();
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_connection_t *c2 = pn_connection();
  pn_proactor_connect2(client, c2, NULL, listener_info(l2).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  TEST_PROACTORS_DRAIN(tps);

  /* Disconnect the client proactor */
  pn_condition_t *cond = pn_condition();
  pn_condition_set_name(cond, "test-name");
  pn_condition_set_description(cond, "test-description");
  pn_proactor_disconnect(client, cond);
  /* Verify expected client side first */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, test_proactors_run(&tps[0], 1));
  TEST_COND_NAME(t, "test-name", last_condition);
  TEST_COND_DESC(t, "test-description", last_condition);
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, test_proactors_run(&tps[0], 1));
  TEST_COND_NAME(t, "test-name", last_condition);
  TEST_COND_DESC(t, "test-description", last_condition);
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, test_proactors_run(&tps[0], 1));

  /* Now check server sees the disconnects */
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_TRANSPORT_CLOSED, TEST_PROACTORS_RUN(tps));

  /* Now disconnect the server end (the listeners) */
  pn_proactor_disconnect(server, cond);
  pn_condition_free(cond);

  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_LISTENER_CLOSE, TEST_PROACTORS_RUN(tps));
  TEST_ETYPE_EQUAL(t, PN_PROACTOR_INACTIVE, TEST_PROACTORS_RUN(tps));

  /* Make sure the proactors are still functional */
  pn_listener_t *l3 = test_listen(&tps[1], "");
  pn_proactor_connect2(client, NULL, NULL, listener_info(l3).connect);
  TEST_ETYPE_EQUAL(t, PN_CONNECTION_REMOTE_OPEN, TEST_PROACTORS_RUN(tps));
  pn_proactor_disconnect(client, NULL);

  TEST_PROACTORS_DRAIN(tps);
  TEST_PROACTORS_DESTROY(tps);
}

struct message_stream_context {
  pn_link_t *sender;
  pn_delivery_t *dlv;
  pn_rwbytes_t send_buf, recv_buf;
  ssize_t size, sent, received;
  bool complete;
};

#define FRAME 512                   /* Smallest legal frame */
#define CHUNK (FRAME + FRAME/2)     /* Chunk overflows frame */
#define BODY (CHUNK*3 + CHUNK/2)    /* Body doesn't fit into chunks */

static pn_event_type_t message_stream_handler(test_handler_t *th, pn_event_t *e) {
  struct message_stream_context *ctx = (struct message_stream_context*)th->context;
  switch (pn_event_type(e)) {
   case PN_CONNECTION_BOUND:
    pn_transport_set_max_frame(pn_event_transport(e), FRAME);
    return PN_EVENT_NONE;

   case PN_SESSION_INIT:
    pn_session_set_incoming_capacity(pn_event_session(e), FRAME); /* Single frame incoming */
    pn_session_set_outgoing_window(pn_event_session(e), 1);       /* Single frame outgoing */
    return PN_EVENT_NONE;

   case PN_LINK_REMOTE_OPEN:
    common_handler(th, e);
    if (pn_link_is_receiver(pn_event_link(e))) {
      pn_link_flow(pn_event_link(e), 1);
    } else {
      ctx->sender = pn_event_link(e);
    }
    return PN_EVENT_NONE;

   case PN_LINK_FLOW:           /* Start a delivery */
    if (pn_link_is_sender(pn_event_link(e)) && !ctx->dlv) {
      ctx->dlv = pn_delivery(pn_event_link(e), pn_dtag("x", 1));
    }
    return PN_LINK_FLOW;

   case PN_CONNECTION_WAKE: {     /* Send a chunk */
     ssize_t remains = ctx->size - ctx->sent;
     ssize_t n = (CHUNK < remains) ? CHUNK : remains;
     TEST_CHECK(th->t, n == pn_link_send(ctx->sender, ctx->send_buf.start + ctx->sent, n));
     ctx->sent += n;
     if (ctx->sent == ctx->size) {
       TEST_CHECK(th->t, pn_link_advance(ctx->sender));
     }
     return PN_CONNECTION_WAKE;
   }

   case PN_DELIVERY: {          /* Receive a delivery - smaller than a chunk? */
     pn_delivery_t *dlv = pn_event_delivery(e);
     if (pn_delivery_readable(dlv)) {
       ssize_t n = pn_delivery_pending(dlv);
       rwbytes_ensure(&ctx->recv_buf, ctx->received + n);
       TEST_ASSERT(n == pn_link_recv(pn_event_link(e), ctx->recv_buf.start + ctx->received, n));
       ctx->received += n;
     }
     ctx->complete = !pn_delivery_partial(dlv);
     return PN_DELIVERY;
   }

   default:
    return common_handler(th, e);
  }
}

/* Test sending/receiving a message in chunks */
static void test_message_stream(test_t *t) {
  test_proactor_t tps[] ={
    test_proactor(t, message_stream_handler),
    test_proactor(t, message_stream_handler)
  };
  pn_proactor_t *client = tps[0].proactor;
  pn_listener_t *l = test_listen(&tps[1], "");
  struct message_stream_context ctx = { 0 };
  tps[0].handler.context = &ctx;
  tps[1].handler.context = &ctx;

  /* Encode a large (not very) message to send in chunks */
  char *body = (char*)malloc(BODY);
  memset(body, 'x', BODY);
  pn_message_t *m = pn_message();
  pn_data_put_binary(pn_message_body(m), pn_bytes(BODY, body));
  free(body);
  ctx.size = message_encode(m, &ctx.send_buf);
  pn_message_free(m);

  pn_connection_t *c = pn_connection();
  pn_proactor_connect2(client, c, NULL, listener_info(l).connect);
  pn_session_t *ssn = pn_session(c);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  TEST_PROACTORS_RUN_UNTIL(tps, PN_LINK_FLOW);

  /* Send and receive the message in chunks */
  do {
    pn_connection_wake(c);      /* Initiate send/receive of one chunk */
    do {                        /* May be multiple receives for one send */
      TEST_PROACTORS_RUN_UNTIL(tps, PN_DELIVERY);
    } while (ctx.received < ctx.sent);
  } while (!ctx.complete);
  TEST_CHECK(t, ctx.received == ctx.size);
  TEST_CHECK(t, ctx.sent == ctx.size);
  TEST_CHECK(t, !memcmp(ctx.send_buf.start, ctx.recv_buf.start, ctx.size));

  free(ctx.send_buf.start);
  free(ctx.recv_buf.start);
  TEST_PROACTORS_DESTROY(tps);
}

int main(int argc, char **argv) {
  int failed = 0;
  last_condition = pn_condition();
  RUN_ARGV_TEST(failed, t, test_inactive(&t));
  RUN_ARGV_TEST(failed, t, test_interrupt_timeout(&t));
  RUN_ARGV_TEST(failed, t, test_errors(&t));
  RUN_ARGV_TEST(failed, t, test_proton_1586(&t));
  RUN_ARGV_TEST(failed, t, test_client_server(&t));
  RUN_ARGV_TEST(failed, t, test_connection_wake(&t));
  RUN_ARGV_TEST(failed, t, test_ipv4_ipv6(&t));
  RUN_ARGV_TEST(failed, t, test_release_free(&t));
#if !defined(_WIN32)
  RUN_ARGV_TEST(failed, t, test_ssl(&t));
#endif
  RUN_ARGV_TEST(failed, t, test_proactor_addr(&t));
  RUN_ARGV_TEST(failed, t, test_parse_addr(&t));
  RUN_ARGV_TEST(failed, t, test_netaddr(&t));
  RUN_ARGV_TEST(failed, t, test_disconnect(&t));
  RUN_ARGV_TEST(failed, t, test_abort(&t));
  RUN_ARGV_TEST(failed, t, test_refuse(&t));
  RUN_ARGV_TEST(failed, t, test_message_stream(&t));
  pn_condition_free(last_condition);
  return failed;
}
