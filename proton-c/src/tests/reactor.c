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

#include <proton/reactor.h>
#include <proton/handlers.h>
#include <proton/event.h>
#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <proton/delivery.h>
#include <proton/url.h>
#include <stdlib.h>
#include <string.h>

#define assert(E) ((E) ? 0 : (abort(), 0))


static void test_reactor(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_free(reactor);
}

static void test_reactor_free(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_reactor_free(reactor);
}

static void test_reactor_run(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  // run should exit if there is nothing left to do
  pn_reactor_run(reactor);
  pn_free(reactor);
}

static void test_reactor_run_free(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  // run should exit if there is nothing left to do
  pn_reactor_run(reactor);
  pn_reactor_free(reactor);
}

typedef struct {
  pn_reactor_t *reactor;
  pn_list_t *events;
} pni_test_handler_t;

pni_test_handler_t *thmem(pn_handler_t *handler) {
  return (pni_test_handler_t *) pn_handler_mem(handler);
}

void test_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  pni_test_handler_t *th = thmem(handler);
  pn_reactor_t *reactor = pn_event_reactor(event);
  assert(reactor == th->reactor);
  pn_list_add(th->events, (void *) type);
}

pn_handler_t *test_handler(pn_reactor_t *reactor, pn_list_t *events) {
  pn_handler_t *handler = pn_handler_new(test_dispatch, sizeof(pni_test_handler_t), NULL);
  thmem(handler)->reactor = reactor;
  thmem(handler)->events = events;
  return handler;
}


void root_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  pni_test_handler_t *th = thmem(handler);
  pn_reactor_t *reactor = pn_event_reactor(event);
  assert(reactor == th->reactor);
  pn_list_add(th->events, pn_event_root(event));
}

pn_handler_t *test_root(pn_reactor_t *reactor, pn_list_t *events) {
  pn_handler_t *handler = pn_handler_new(root_dispatch, sizeof(pni_test_handler_t), NULL);
  thmem(handler)->reactor = reactor;
  thmem(handler)->events = events;
  return handler;
}

#define END PN_EVENT_NONE

void expect(pn_list_t *events, ...) {
  va_list ap;

  va_start(ap, events);
  size_t idx = 0;
  while (true) {
    pn_event_type_t expected = (pn_event_type_t) va_arg(ap, int);
    if (expected == END) {
      assert(idx == pn_list_size(events));
      break;
    }
    assert(idx < pn_list_size(events));
    pn_event_type_t actual = (pn_event_type_t)(size_t) pn_list_get(events, idx++);
    assert(expected == actual);
  }
  va_end(ap);
}

static void test_reactor_handler(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *handler = pn_reactor_get_handler(reactor);
  assert(handler);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_t *th = test_handler(reactor, events);
  pn_handler_add(handler, th);
  pn_decref(th);
  pn_free(reactor);
  expect(events, END);
  pn_free(events);
}

static void test_reactor_handler_free(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *handler = pn_reactor_get_handler(reactor);
  assert(handler);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_add(handler, test_handler(reactor, events));
  pn_reactor_free(reactor);
  expect(events, END);
  pn_free(events);
}

static void test_reactor_handler_run(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *handler = pn_reactor_get_handler(reactor);
  assert(handler);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_t *th = test_handler(reactor, events);
  pn_handler_add(handler, th);
  pn_reactor_run(reactor);
  expect(events, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED, PN_SELECTABLE_FINAL, PN_REACTOR_FINAL, END);
  pn_free(reactor);
  pn_free(th);
  pn_free(events);
}

static void test_reactor_handler_run_free(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *handler = pn_reactor_get_handler(reactor);
  assert(handler);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_add(handler, test_handler(reactor, events));
  pn_reactor_run(reactor);
  expect(events, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED, PN_SELECTABLE_FINAL, PN_REACTOR_FINAL, END);
  pn_reactor_free(reactor);
  pn_free(events);
}

static void test_reactor_event_root(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *handler = pn_reactor_get_handler(reactor);
  assert(handler);
  pn_list_t *roots = pn_list(PN_VOID, 0);
  pn_handler_t *th = test_root(reactor, roots);
  pn_handler_add(handler, th);
  pn_reactor_run(reactor);
  expect(roots, handler, handler, handler, handler, handler, END);
  pn_free(reactor);
  pn_free(th);
  pn_free(roots);
}

static void test_reactor_connection(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_list_t *cevents = pn_list(PN_VOID, 0);
  pn_handler_t *tch = test_handler(reactor, cevents);
  pn_connection_t *connection = pn_reactor_connection(reactor, tch);
  assert(connection);
  pn_reactor_set_connection_host(reactor, connection, "127.0.0.1", "5672");
  pn_url_t *url = pn_url_parse(pn_reactor_get_connection_address(reactor, connection));
  assert(strcmp(pn_url_get_host(url), "127.0.0.1") == 0);
  assert(strcmp(pn_url_get_port(url), "5672") == 0);
  pn_decref(url);
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  pn_list_t *revents = pn_list(PN_VOID, 0);
  pn_handler_add(root, test_handler(reactor, revents));
  pn_reactor_run(reactor);
  expect(revents, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED, PN_SELECTABLE_FINAL, PN_REACTOR_FINAL,
         END);
  expect(cevents, PN_CONNECTION_INIT, END);
  pn_reactor_free(reactor);
  pn_handler_free(tch);
  pn_free(cevents);
  pn_free(revents);
}

static void test_reactor_acceptor(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_acceptor_t *acceptor = pn_reactor_acceptor(reactor, "0.0.0.0", "5678", NULL);
  assert(acceptor);
  pn_reactor_free(reactor);
}

pn_acceptor_t **tram(pn_handler_t *h) {
  return (pn_acceptor_t **) pn_handler_mem(h);
}

static void tra_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  switch (type) {
  case PN_REACTOR_INIT:
    {
      pn_acceptor_t *acceptor = *tram(handler);
      pn_acceptor_close(acceptor);
    }
    break;
  default:
    break;
  }
}

static pn_handler_t *tra_handler(pn_acceptor_t *acceptor) {
  pn_handler_t *handler = pn_handler_new(tra_dispatch, sizeof(pn_acceptor_t *), NULL);
  *tram(handler) = acceptor;
  return handler;
}

static void test_reactor_acceptor_run(void) {
  pn_reactor_t *reactor = pn_reactor();
  assert(reactor);
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  assert(root);
  pn_acceptor_t *acceptor = pn_reactor_acceptor(reactor, "0.0.0.0", "5678", NULL);
  assert(acceptor);
  pn_handler_add(root, tra_handler(acceptor));
  pn_reactor_run(reactor);
  pn_reactor_free(reactor);
}

typedef struct {
  pn_reactor_t *reactor;
  pn_acceptor_t *acceptor;
  pn_list_t *events;
} server_t;

static server_t *smem(pn_handler_t *handler) {
  return (server_t *) pn_handler_mem(handler);
}

static void server_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  server_t *srv = smem(handler);
  pn_list_add(srv->events, (void *) pn_event_type(event));
  switch (type) {
  case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(event));
    break;
  case PN_CONNECTION_REMOTE_CLOSE:
    pn_acceptor_close(srv->acceptor);
    pn_connection_close(pn_event_connection(event));
    pn_connection_release(pn_event_connection(event));
    break;
  default:
    break;
  }
}

typedef struct {
  pn_list_t *events;
} client_t;

static client_t *cmem(pn_handler_t *handler) {
  return (client_t *) pn_handler_mem(handler);
}

static void client_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  client_t *cli = cmem(handler);
  pn_list_add(cli->events, (void *) type);
  pn_connection_t *conn = pn_event_connection(event);
  switch (pn_event_type(event)) {
  case PN_CONNECTION_INIT:
    pn_connection_set_hostname(conn, "some.org");
    pn_connection_open(conn);
    break;
  case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_close(conn);
    break;
  case PN_CONNECTION_REMOTE_CLOSE:
    pn_connection_release(conn);
    break;
  default:
    break;
  }
}

static void test_reactor_connect(void) {
  pn_reactor_t *reactor = pn_reactor();
  pn_handler_t *sh = pn_handler_new(server_dispatch, sizeof(server_t), NULL);
  server_t *srv = smem(sh);
  pn_acceptor_t *acceptor = pn_reactor_acceptor(reactor, "0.0.0.0", "5678", sh);
  srv->reactor = reactor;
  srv->acceptor = acceptor;
  srv->events = pn_list(PN_VOID, 0);
  pn_handler_t *ch = pn_handler_new(client_dispatch, sizeof(client_t), NULL);
  client_t *cli = cmem(ch);
  cli->events = pn_list(PN_VOID, 0);
  pn_connection_t *conn = pn_reactor_connection_to_host(reactor,
                                                        "127.0.0.1",
                                                        "5678",
                                                        ch);
  assert(conn);
  pn_url_t *url = pn_url_parse(pn_reactor_get_connection_address(reactor, conn));
  assert(strcmp(pn_url_get_host(url), "127.0.0.1") == 0);
  assert(strcmp(pn_url_get_port(url), "5678") == 0);
  pn_decref(url);
  pn_reactor_run(reactor);
  expect(srv->events, PN_CONNECTION_INIT, PN_CONNECTION_BOUND,
         PN_CONNECTION_REMOTE_OPEN,
         PN_CONNECTION_LOCAL_OPEN, PN_TRANSPORT,
         PN_CONNECTION_REMOTE_CLOSE, PN_TRANSPORT_TAIL_CLOSED,
         PN_CONNECTION_LOCAL_CLOSE, PN_TRANSPORT,
         PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_CLOSED,
         PN_CONNECTION_UNBOUND, PN_CONNECTION_FINAL, END);
  pn_free(srv->events);
  pn_decref(sh);
  expect(cli->events, PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN,
         PN_CONNECTION_BOUND,
         PN_CONNECTION_REMOTE_OPEN, PN_CONNECTION_LOCAL_CLOSE,
         PN_TRANSPORT, PN_TRANSPORT_HEAD_CLOSED,
         PN_CONNECTION_REMOTE_CLOSE, PN_TRANSPORT_TAIL_CLOSED,
         PN_TRANSPORT_CLOSED, PN_CONNECTION_UNBOUND,
         PN_CONNECTION_FINAL, END);
  pn_free(cli->events);
  pn_decref(ch);
  pn_reactor_free(reactor);
}

typedef struct {
  int received;
} sink_t;

static sink_t *sink(pn_handler_t *handler) {
  return (sink_t *) pn_handler_mem(handler);
}

void sink_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  sink_t *snk = sink(handler);
  pn_delivery_t *dlv = pn_event_delivery(event);
  switch (type) {
  case PN_DELIVERY:
    if (!pn_delivery_partial(dlv)) {
      pn_delivery_settle(dlv);
      snk->received++;
    }
    break;
  default:
    break;
  }
}

typedef struct {
  int remaining;
} source_t;

static source_t *source(pn_handler_t *handler) {
  return (source_t *) pn_handler_mem(handler);
}


void source_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  source_t *src = source(handler);
  pn_connection_t *conn = pn_event_connection(event);
  switch (type) {
  case PN_CONNECTION_INIT:
    {
      pn_session_t *ssn = pn_session(conn);
      pn_link_t *snd = pn_sender(ssn, "sender");
      pn_connection_open(conn);
      pn_session_open(ssn);
      pn_link_open(snd);
    }
    break;
  case PN_LINK_FLOW:
    {
      pn_link_t *link = pn_event_link(event);
      while (pn_link_credit(link) > 0 && src->remaining > 0) {
        pn_delivery_t *dlv = pn_delivery(link, pn_dtag("", 0));
        assert(dlv);
        pn_delivery_settle(dlv);
        src->remaining--;
      }

      if (!src->remaining) {
        pn_connection_close(conn);
      }
    }
    break;
  case PN_CONNECTION_REMOTE_CLOSE:
    pn_connection_release(conn);
    break;
  default:
    break;
  }
}

static void test_reactor_transfer(int count, int window) {
  pn_reactor_t *reactor = pn_reactor();

  pn_handler_t *sh = pn_handler_new(server_dispatch, sizeof(server_t), NULL);
  server_t *srv = smem(sh);
  pn_acceptor_t *acceptor = pn_reactor_acceptor(reactor, "0.0.0.0", "5678", sh);
  srv->reactor = reactor;
  srv->acceptor = acceptor;
  srv->events = pn_list(PN_VOID, 0);
  pn_handler_add(sh, pn_handshaker());
  // XXX: a window of 1 doesn't work unless the flowcontroller is
  // added after the thing that settles the delivery
  pn_handler_add(sh, pn_flowcontroller(window));
  pn_handler_t *snk = pn_handler_new(sink_dispatch, sizeof(sink_t), NULL);
  sink(snk)->received = 0;
  pn_handler_add(sh, snk);

  pn_handler_t *ch = pn_handler_new(source_dispatch, sizeof(source_t), NULL);
  source_t *src = source(ch);
  src->remaining = count;
  pn_connection_t *conn = NULL;
  // Using the connection's hostname to set the connection address is
  // deprecated. Once support is dropped the conditional code can be removed:
  #if 0
  conn = pn_reactor_connection(reactor, ch);
  assert(conn);
  pn_reactor_connection_set_address(reactor, conn, "127.0.0.1", "5678");
  #else
  // This is deprecated:
  conn = pn_reactor_connection(reactor, ch);
  pn_connection_set_hostname(conn, "127.0.0.1:5678");
  #endif

  pn_reactor_run(reactor);

  assert(sink(snk)->received == count);

  pn_free(srv->events);
  pn_reactor_free(reactor);
  pn_handler_free(sh);
  pn_handler_free(ch);
}

static void test_reactor_schedule(void) {
  pn_reactor_t *reactor = pn_reactor();
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_add(root, test_handler(reactor, events));
  pn_reactor_schedule(reactor, 0, NULL);
  pn_reactor_run(reactor);
  pn_reactor_free(reactor);
  expect(events, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED, PN_REACTOR_QUIESCED,
         PN_TIMER_TASK, PN_SELECTABLE_UPDATED, PN_SELECTABLE_FINAL, PN_REACTOR_FINAL, END);
  pn_free(events);
}

static void test_reactor_schedule_handler(void) {
  pn_reactor_t *reactor = pn_reactor();
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_list_t *tevents = pn_list(PN_VOID, 0);
  pn_handler_add(root, test_handler(reactor, events));
  pn_handler_t *th = test_handler(reactor, tevents);
  pn_reactor_schedule(reactor, 0, th);
  pn_reactor_run(reactor);
  pn_reactor_free(reactor);
  pn_handler_free(th);
  expect(events, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED, PN_REACTOR_QUIESCED, PN_SELECTABLE_UPDATED,
         PN_SELECTABLE_FINAL, PN_REACTOR_FINAL, END);
  expect(tevents, PN_TIMER_TASK, END);
  pn_free(events);
  pn_free(tevents);
}

static void test_reactor_schedule_cancel(void) {
  pn_reactor_t *reactor = pn_reactor();
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  pn_list_t *events = pn_list(PN_VOID, 0);
  pn_handler_add(root, test_handler(reactor, events));
  pn_task_t *task = pn_reactor_schedule(reactor, 0, NULL);
  pn_task_cancel(task);
  pn_reactor_run(reactor);
  pn_reactor_free(reactor);
  expect(events, PN_REACTOR_INIT, PN_SELECTABLE_INIT, PN_SELECTABLE_UPDATED,
         PN_SELECTABLE_FINAL, PN_REACTOR_FINAL, END);
  pn_free(events);
}

int main(int argc, char **argv)
{
  test_reactor_event_root();
  test_reactor();
  test_reactor_free();
  test_reactor_run();
  test_reactor_run_free();
  test_reactor_handler();
  test_reactor_handler_free();
  test_reactor_handler_run();
  test_reactor_handler_run_free();
  test_reactor_connection();
  test_reactor_acceptor();
  test_reactor_acceptor_run();
  test_reactor_connect();
  for (int i = 0; i < 64; i++) {
    test_reactor_transfer(i, 2);
  }
  test_reactor_transfer(1024, 64);
  test_reactor_transfer(4*1024, 1024);
  test_reactor_schedule();
  test_reactor_schedule_handler();
  test_reactor_schedule_cancel();
  return 0;
}
