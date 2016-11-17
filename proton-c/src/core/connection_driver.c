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
#include <proton/transport.h>
#include <string.h>

struct driver_batch {
  pn_event_batch_t batch;
};

static pn_event_t *batch_next(pn_event_batch_t *batch) {
  pn_connection_driver_t *d =
    (pn_connection_driver_t*)((char*)batch - offsetof(pn_connection_driver_t, batch));
  pn_collector_t *collector = pn_connection_collector(d->connection);
  pn_event_t *handled = pn_collector_prev(collector);
  if (handled && pn_event_type(handled) == PN_CONNECTION_INIT) {
      pn_transport_bind(d->transport, d->connection); /* Init event handled, auto-bind */
  }
  pn_event_t *next = pn_collector_next(collector);
  if (next && d->transport->trace & PN_TRACE_EVT) {
    pn_string_clear(d->transport->scratch);
    pn_inspect(next, d->transport->scratch);
    pn_transport_log(d->transport, pn_string_get(d->transport->scratch));
  }
  return next;
}

int pn_connection_driver_init(pn_connection_driver_t* d, pn_connection_t *c, pn_transport_t *t) {
  memset(d, 0, sizeof(*d));
  d->batch.next_event = &batch_next;
  d->connection = c ? c : pn_connection();
  d->transport = t ? t : pn_transport();
  pn_collector_t *collector = pn_collector();
  if (!d->connection || !d->transport || !collector) {
    if (collector) pn_collector_free(collector);
    pn_connection_driver_destroy(d);
    return PN_OUT_OF_MEMORY;
  }
  pn_connection_collect(d->connection, collector);
  return 0;
}

int pn_connection_driver_bind(pn_connection_driver_t *d) {
  return pn_transport_bind(d->transport, d->connection);
}

void pn_connection_driver_destroy(pn_connection_driver_t *d) {
  if (d->transport) {
    pn_transport_unbind(d->transport);
    pn_transport_free(d->transport);
  }
  if (d->connection) {
    pn_collector_t *collector = pn_connection_collector(d->connection);
    pn_connection_free(d->connection);
    pn_collector_free(collector);
  }
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
  return pn_transport_capacity(d->transport) < 0;
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
  if (n > 0)
    pn_transport_pop(d->transport, n);
}

bool pn_connection_driver_write_closed(pn_connection_driver_t *d) {
  return pn_transport_pending(d->transport) < 0;
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
  return pn_event_batch_next(&d->batch);
}

bool pn_connection_driver_has_event(pn_connection_driver_t *d) {
  return pn_collector_peek(pn_connection_collector(d->connection));
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
  pn_transport_log(d->transport, msg);
}

void pn_connection_driver_vlogf(pn_connection_driver_t *d, const char *fmt, va_list ap) {
  pn_transport_vlogf(d->transport, fmt, ap);
}

void pn_connection_driver_vlog(pn_connection_driver_t *d, const char *msg) {
  pn_transport_log(d->transport, msg);
}

pn_connection_driver_t* pn_event_batch_connection_driver(pn_event_batch_t *batch) {
  return (batch->next_event == batch_next) ?
    (pn_connection_driver_t*)((char*)batch - offsetof(pn_connection_driver_t, batch)) :
    NULL;
}
