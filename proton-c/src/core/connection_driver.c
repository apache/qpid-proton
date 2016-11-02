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
#include <proton/connection_engine.h>
#include <proton/transport.h>
#include <string.h>

int pn_connection_engine_init(pn_connection_engine_t* ce, pn_connection_t *c, pn_transport_t *t) {
  ce->connection = c ? c : pn_connection();
  ce->transport = t ? t : pn_transport();
  ce->collector = pn_collector();
  if (!ce->connection || !ce->transport || !ce->collector) {
    pn_connection_engine_destroy(ce);
    return PN_OUT_OF_MEMORY;
  }
  pn_connection_collect(ce->connection, ce->collector);
  return 0;
}

int pn_connection_engine_bind(pn_connection_engine_t *ce) {
  return pn_transport_bind(ce->transport, ce->connection);
}

void pn_connection_engine_destroy(pn_connection_engine_t *ce) {
  if (ce->transport) {
    pn_transport_unbind(ce->transport);
    pn_transport_free(ce->transport);
  }
  if (ce->collector) pn_collector_free(ce->collector);
  if (ce->connection) pn_connection_free(ce->connection);
  memset(ce, 0, sizeof(*ce));
}

pn_rwbytes_t pn_connection_engine_read_buffer(pn_connection_engine_t *ce) {
  ssize_t cap = pn_transport_capacity(ce->transport);
  return (cap > 0) ?  pn_rwbytes(cap, pn_transport_tail(ce->transport)) : pn_rwbytes(0, 0);
}

void pn_connection_engine_read_done(pn_connection_engine_t *ce, size_t n) {
  if (n > 0) pn_transport_process(ce->transport, n);
}

bool pn_connection_engine_read_closed(pn_connection_engine_t *ce) {
  return pn_transport_capacity(ce->transport) < 0;
}

void pn_connection_engine_read_close(pn_connection_engine_t *ce) {
  if (!pn_connection_engine_read_closed(ce)) {
    pn_transport_close_tail(ce->transport);
  }
}

pn_bytes_t pn_connection_engine_write_buffer(pn_connection_engine_t *ce) {
  ssize_t pending = pn_transport_pending(ce->transport);
  return (pending > 0) ?
    pn_bytes(pending, pn_transport_head(ce->transport)) : pn_bytes_null;
}

void pn_connection_engine_write_done(pn_connection_engine_t *ce, size_t n) {
  if (n > 0)
    pn_transport_pop(ce->transport, n);
}

bool pn_connection_engine_write_closed(pn_connection_engine_t *ce) {
  return pn_transport_pending(ce->transport) < 0;
}

void pn_connection_engine_write_close(pn_connection_engine_t *ce) {
  if (!pn_connection_engine_write_closed(ce)) {
    pn_transport_close_head(ce->transport);
  }
}

void pn_connection_engine_close(pn_connection_engine_t *ce) {
  pn_connection_engine_read_close(ce);
  pn_connection_engine_write_close(ce);
}

pn_event_t* pn_connection_engine_event(pn_connection_engine_t *ce) {
  pn_event_t *e = ce->collector ? pn_collector_peek(ce->collector) : NULL;
  if (e) {
    pn_transport_t *t = ce->transport;
    if (t && t->trace & PN_TRACE_EVT) {
      /* This can log the same event twice if pn_connection_engine_event is called
       * twice but for debugging it is much better to log before handling than after.
       */
      pn_string_clear(t->scratch);
      pn_inspect(e, t->scratch);
      pn_transport_log(t, pn_string_get(t->scratch));
    }
  }
  return e;
}

bool pn_connection_engine_has_event(pn_connection_engine_t *ce) {
  return ce->collector && pn_collector_peek(ce->collector);
}

void pn_connection_engine_pop_event(pn_connection_engine_t *ce) {
  if (ce->collector) {
    pn_event_t *e = pn_collector_peek(ce->collector);
    if (pn_event_type(e) == PN_TRANSPORT_CLOSED) { /* The last event ever */
      /* Events can accumulate behind the TRANSPORT_CLOSED before the
       * PN_TRANSPORT_CLOSED event is handled. They can never be processed
       * so release them.
       */
      pn_collector_release(ce->collector);
    } else {
      pn_collector_pop(ce->collector);
    }

  }
}

bool pn_connection_engine_finished(pn_connection_engine_t *ce) {
  return pn_transport_closed(ce->transport) && !pn_connection_engine_has_event(ce);
}

void pn_connection_engine_verrorf(pn_connection_engine_t *ce, const char *name, const char *fmt, va_list ap) {
  pn_transport_t *t = ce->transport;
  pn_condition_t *cond = pn_transport_condition(t);
  pn_string_vformat(t->scratch, fmt, ap);
  pn_condition_set_name(cond, name);
  pn_condition_set_description(cond, pn_string_get(t->scratch));
}

void pn_connection_engine_errorf(pn_connection_engine_t *ce, const char *name, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pn_connection_engine_verrorf(ce, name, fmt, ap);
  va_end(ap);
}

void pn_connection_engine_log(pn_connection_engine_t *ce, const char *msg) {
  pn_transport_log(ce->transport, msg);
}

void pn_connection_engine_vlogf(pn_connection_engine_t *ce, const char *fmt, va_list ap) {
  pn_transport_vlogf(ce->transport, fmt, ap);
}

void pn_connection_engine_vlog(pn_connection_engine_t *ce, const char *msg) {
  pn_transport_log(ce->transport, msg);
}
