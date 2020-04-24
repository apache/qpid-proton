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

#include "engine-internal.h"
#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/connection_driver.h>
#include <proton/event.h>
#include <proton/transport.h>
#include <string.h>

static pn_event_t *batch_next(pn_connection_driver_t *d) {
  if (!d->collector) return NULL;
  pn_event_t *handled = pn_collector_prev(d->collector);
  if (handled) {
    switch (pn_event_type(handled)) {
     case PN_CONNECTION_INIT:   /* Auto-bind after the INIT event is handled */
      pn_transport_bind(d->transport, d->connection);
      break;
     case PN_TRANSPORT_CLOSED:  /* No more events after TRANSPORT_CLOSED  */
      pn_collector_release(d->collector);
      break;
     default:
      break;
    }
  }
  /* Log the next event that will be processed */
  pn_event_t *next = pn_collector_next(d->collector);
  if (next && PN_SHOULD_LOG(&d->transport->logger, PN_SUBSYSTEM_EVENT, PN_LEVEL_DEBUG)) {
    pn_string_clear(d->transport->scratch);
    pn_inspect(next, d->transport->scratch);
    pni_logger_log(&d->transport->logger, PN_SUBSYSTEM_EVENT, PN_LEVEL_DEBUG, pn_string_get(d->transport->scratch));
  }
  return next;
}

int pn_connection_driver_init(pn_connection_driver_t* d, pn_connection_t *c, pn_transport_t *t) {
  memset(d, 0, sizeof(*d));
  d->connection = c ? c : pn_connection();
  d->transport = t ? t : pn_transport();
  d->collector = pn_collector();
  if (!d->connection || !d->transport || !d->collector) {
    pn_connection_driver_destroy(d);
    return PN_OUT_OF_MEMORY;
  }
  pn_connection_collect(d->connection, d->collector);
  return 0;
}

int pn_connection_driver_bind(pn_connection_driver_t *d) {
  return pn_transport_bind(d->transport, d->connection);
}

pn_connection_t *pn_connection_driver_release_connection(pn_connection_driver_t *d) {
  if (d->transport) {           /* Make sure transport is closed and unbound */
    pn_connection_driver_close(d);
    pn_transport_unbind(d->transport);
  }
  pn_connection_t *c = d->connection;
  if (c) {
    d->connection = NULL;
    pn_connection_reset(c);
    pn_connection_collect(c, NULL); /* Disconnect from the collector */
  }
  return c;
}

void pn_connection_driver_destroy(pn_connection_driver_t *d) {
  pn_connection_t *c = pn_connection_driver_release_connection(d);
  if (c) pn_connection_free(c);
  if (d->transport) pn_transport_free(d->transport);
  if (d->collector) pn_collector_free(d->collector);
  memset(d, 0, sizeof(*d));
}

pn_rwbytes_t pn_connection_driver_read_buffer(pn_connection_driver_t *d) {
  ssize_t cap = pn_transport_capacity(d->transport);
  return (cap > 0) ?  pn_rwbytes(cap, pn_transport_tail(d->transport)) : pn_rwbytes(0, 0);
}

void pn_connection_driver_read_done(pn_connection_driver_t *d, size_t n) {
  if (n > 0) pn_transport_process(d->transport, n);
}

bool pn_connection_driver_read_closed(pn_connection_driver_t *d) {
  return pn_transport_tail_closed(d->transport);
}

void pn_connection_driver_read_close(pn_connection_driver_t *d) {
  if (!pn_connection_driver_read_closed(d)) {
    pn_transport_close_tail(d->transport);
  }
}

pn_bytes_t pn_connection_driver_write_buffer(pn_connection_driver_t *d) {
  ssize_t pending = pn_transport_pending(d->transport);
  return (pending > 0) ?
    pn_bytes(pending, pn_transport_head(d->transport)) : pn_bytes_null;
}

void pn_connection_driver_write_done(pn_connection_driver_t *d, size_t n) {
  pn_transport_pop(d->transport, n);
}

bool pn_connection_driver_write_closed(pn_connection_driver_t *d) {
  return pn_transport_head_closed(d->transport);
}

void pn_connection_driver_write_close(pn_connection_driver_t *d) {
  if (!pn_connection_driver_write_closed(d)) {
    pn_transport_close_head(d->transport);
  }
}

void pn_connection_driver_close(pn_connection_driver_t *d) {
  pn_connection_driver_read_close(d);
  pn_connection_driver_write_close(d);
}

pn_event_t* pn_connection_driver_next_event(pn_connection_driver_t *d) {
  return batch_next(d);
}

bool pn_connection_driver_has_event(pn_connection_driver_t *d) {
  return d->connection && pn_collector_peek(pn_connection_collector(d->connection));
}

bool pn_connection_driver_finished(pn_connection_driver_t *d) {
  return pn_transport_closed(d->transport) && !pn_connection_driver_has_event(d);
}

void pn_connection_driver_verrorf(pn_connection_driver_t *d, const char *name, const char *fmt, va_list ap) {
  pn_transport_t *t = d->transport;
  pn_condition_t *cond = pn_transport_condition(t);
  pn_string_vformat(t->scratch, fmt, ap);
  pn_condition_set_name(cond, name);
  pn_condition_set_description(cond, pn_string_get(t->scratch));
}

void pn_connection_driver_errorf(pn_connection_driver_t *d, const char *name, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pn_connection_driver_verrorf(d, name, fmt, ap);
  va_end(ap);
}

void pn_connection_driver_log(pn_connection_driver_t *d, const char *msg) {
  pni_logger_log(&d->transport->logger, PN_SUBSYSTEM_IO, PN_LEVEL_TRACE, msg);
}

void pn_connection_driver_logf(pn_connection_driver_t *d, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pni_logger_vlogf(&d->transport->logger, PN_SUBSYSTEM_IO, PN_LEVEL_TRACE, fmt, ap);
  va_end(ap);
}

void pn_connection_driver_vlogf(pn_connection_driver_t *d, const char *fmt, va_list ap) {
  pni_logger_vlogf(&d->transport->logger, PN_SUBSYSTEM_IO, PN_LEVEL_TRACE, fmt, ap);
}

pn_connection_driver_t** pn_connection_driver_ptr(pn_connection_t *c) { return &c->driver; }

/* Backwards ABI compatability hack - this has been removed because it can't be used sanely */
PN_EXTERN pn_connection_driver_t *pn_event_batch_connection_driver(pn_event_batch_t *b) { return NULL; }
