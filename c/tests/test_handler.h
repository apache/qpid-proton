#ifndef TESTS_TEST_DRIVER_H
#define TESTS_TEST_DRIVER_H

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
#include <proton/ssl.h>

/* C event handlers for tests */

#define MAX_EVENT_LOG 2048         /* Max number of event types stored per proactor_test */

struct test_handler_t;

/* Returns 0 if the test should continue processing events, non-0 if processing should pause here */
typedef pn_event_type_t (*test_handler_fn)(struct test_handler_t*, pn_event_t*);

/* A handler that logs event types and delegates to another handler */
typedef struct test_handler_t {
  test_t *t;
  test_handler_fn f;
  pn_event_type_t log[MAX_EVENT_LOG]; /* Log of event types */
  size_t log_size;                    /* Number of events in the log */
  void *context;                      /* Generic test context */
  /* Specific context slots for proton objects commonly used by handlers  */
  pn_connection_t *connection;
  pn_session_t *session;
  pn_link_t *link;
  pn_link_t *sender;
  pn_link_t *receiver;
  pn_delivery_t *delivery;
  pn_message_t *message;
  pn_ssl_domain_t *ssl_domain;
} test_handler_t;

void test_handler_init(test_handler_t *th, test_t *t, test_handler_fn f) {
  memset(th, 0, sizeof(*th));
  th->t = t;
  th->f = f;
}

/* Save event type in the handler log */
void test_handler_log(test_handler_t *th, pn_event_t *e) {
  TEST_ASSERT(th->log_size < MAX_EVENT_LOG);
  th->log[th->log_size++] = pn_event_type(e);
}

/* Keep at most n events in the handler's log, remove old events if necessary */
void test_handler_clear(test_handler_t *th, size_t n) {
  if (n == 0) {
    th->log_size = 0;
  } else if (n < th->log_size) {
    memmove(th->log, th->log + th->log_size - n, n * sizeof(pn_event_type_t));
    th->log_size = n;
  }
}

void test_etypes_expect_(test_t *t, pn_event_type_t *etypes, size_t size, const char* file, int line, ...) {
  va_list ap;
  va_start(ap, line);             /* ap is null terminated */
  pn_event_type_t want = (pn_event_type_t)va_arg(ap, int);
  size_t i = 0;
  while (want && i < size && want == etypes[i]) {
    ++i;
    want = (pn_event_type_t)va_arg(ap, int);
  }
  if (i < size || want) {
    test_errorf_(t, NULL, file, line, "event mismatch");
    fprintf(stderr, "after:");
    for (size_t j = 0; j < i; ++j) { /* These events matched */
      fprintf(stderr, " %s", pn_event_type_name(etypes[j]));
    }
    fprintf(stderr, "\n want:");
    for (; want; want = (pn_event_type_t)va_arg(ap, int)) {
      fprintf(stderr, " %s", pn_event_type_name(want));
    }
    fprintf(stderr, "\n  got:");
    for (; i < size; ++i) {
      fprintf(stderr, " %s", pn_event_type_name(etypes[i]));
    }
    fprintf(stderr, "\n");
  }
  va_end(ap);
}

#define TEST_HANDLER_EXPECT(TH, ...) do {                               \
    test_etypes_expect_((TH)->t, (TH)->log, (TH)->log_size, __FILE__, __LINE__, __VA_ARGS__); \
    test_handler_clear((TH), 0);                                         \
  } while(0)

#define TEST_HANDLER_EXPECT_LAST(TH, ETYPE) do {                        \
    test_handler_clear((TH), 1);                                        \
    test_etypes_expect_((TH)->t, (TH)->log, (TH)->log_size, __FILE__, __LINE__, ETYPE, 0); \
    test_handler_clear((TH), 0);                                         \
  } while(0)

/* A pn_connection_driver_t with a test_handler */
typedef struct test_connection_driver_t {
  test_handler_t handler;
  pn_connection_driver_t driver;
} test_connection_driver_t;

void test_connection_driver_init(test_connection_driver_t *d, test_t *t, test_handler_fn f, void* context)
{
  test_handler_init(&d->handler, t, f);
  d->handler.context = context;
  pn_connection_driver_init(&d->driver, NULL, NULL);
}

void test_connection_driver_destroy(test_connection_driver_t *d) {
  pn_connection_driver_destroy(&d->driver);
}

pn_event_type_t test_connection_driver_handle(test_connection_driver_t *d) {
  for (pn_event_t *e = pn_connection_driver_next_event(&d->driver);
       e;
       e = pn_connection_driver_next_event(&d->driver))
  {
    test_handler_log(&d->handler, e);
    pn_event_type_t et = d->handler.f ? d->handler.f(&d->handler, e) : PN_EVENT_NONE;
    if (et) return et;
  }
  return PN_EVENT_NONE;
}

/* Transfer data from one driver to another in memory */
static int test_connection_drivers_xfer(test_connection_driver_t *dst, test_connection_driver_t *src)
{
  pn_bytes_t wb = pn_connection_driver_write_buffer(&src->driver);
  pn_rwbytes_t rb =  pn_connection_driver_read_buffer(&dst->driver);
  size_t size = rb.size < wb.size ? rb.size : wb.size;
  if (size) {
    memcpy(rb.start, wb.start, size);
    pn_connection_driver_write_done(&src->driver, size);
    pn_connection_driver_read_done(&dst->driver, size);
  }
  return size;
}

/* Run a pair of test drivers till there is nothing to do or one of their handlers returns non-0
   In that case return that driver
*/
test_connection_driver_t* test_connection_drivers_run(test_connection_driver_t *a, test_connection_driver_t *b)
{
  int data = 0;
  do {
    if (test_connection_driver_handle(a)) return a;
    if (test_connection_driver_handle(b)) return b;
    data = test_connection_drivers_xfer(a, b) + test_connection_drivers_xfer(b, a);
  } while (data || pn_connection_driver_has_event(&a->driver) || pn_connection_driver_has_event(&b->driver));
  return NULL;
}

/* Initialize a client-server driver pair */
void test_connection_drivers_init(test_t *t, test_connection_driver_t *a, test_handler_fn fa, test_connection_driver_t *b, test_handler_fn fb) {
  test_connection_driver_init(a, t, fa, NULL);
  test_connection_driver_init(b, t, fb, NULL);
  pn_transport_set_server(b->driver.transport);
}

void test_connection_drivers_destroy(test_connection_driver_t *a, test_connection_driver_t *b) {
  test_connection_driver_destroy(a);
  test_connection_driver_destroy(b);
}
#endif // TESTS_TEST_DRIVER_H
