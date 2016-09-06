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

#include <proton/connection.h>
#include <proton/connection_engine.h>
#include <proton/transport.h>
#include <string.h>

int pn_connection_engine_init(pn_connection_engine_t* e) {
    memset(e, 0, sizeof(*e));
    e->connection = pn_connection();
    e->transport = pn_transport();
    e->collector = pn_collector();
    if (!e->connection || !e->transport || !e->collector) {
        pn_connection_engine_final(e);
        return PN_OUT_OF_MEMORY;
    }
    pn_connection_collect(e->connection, e->collector);
    return PN_OK;
}

void pn_connection_engine_start(pn_connection_engine_t* e) {
    /*
      Ignore bind errors. PN_STATE_ERR means we are already bound, any
      other error will be delivered as an event.
    */
    pn_transport_bind(e->transport, e->connection);
}

void pn_connection_engine_final(pn_connection_engine_t* e) {
    if (e->transport && e->connection) {
        pn_transport_unbind(e->transport);
        pn_decref(e->transport);
    }
    if (e->collector)
        pn_collector_free(e->collector); /* Break cycle with connection */
    if (e->connection)
        pn_decref(e->connection);
    memset(e, 0, sizeof(*e));
}

pn_rwbytes_t pn_connection_engine_read_buffer(pn_connection_engine_t* e) {
    ssize_t cap = pn_transport_capacity(e->transport);
    if (cap > 0)
        return pn_rwbytes(cap, pn_transport_tail(e->transport));
    else
        return pn_rwbytes(0, 0);
}

void pn_connection_engine_read_done(pn_connection_engine_t* e, size_t n) {
    if (n > 0)
        pn_transport_process(e->transport, n);
}

void pn_connection_engine_read_close(pn_connection_engine_t* e) {
    pn_transport_close_tail(e->transport);
}

pn_bytes_t pn_connection_engine_write_buffer(pn_connection_engine_t* e) {
    ssize_t pending = pn_transport_pending(e->transport);
    if (pending > 0)
        return pn_bytes(pending, pn_transport_head(e->transport));
    else
        return pn_bytes(0, 0);
}

void pn_connection_engine_write_done(pn_connection_engine_t* e, size_t n) {
    if (n > 0)
        pn_transport_pop(e->transport, n);
}

void pn_connection_engine_write_close(pn_connection_engine_t* e){
    pn_transport_close_head(e->transport);
}

void pn_connection_engine_disconnected(pn_connection_engine_t* e) {
    pn_connection_engine_read_close(e);
    pn_connection_engine_write_close(e);
}

static void log_event(pn_connection_engine_t *engine, pn_event_t* event) {
    if (event && engine->transport->trace & PN_TRACE_EVT) {
        pn_string_t *str = pn_string(NULL);
        pn_inspect(event, str);
        pn_transport_log(engine->transport, pn_string_get(str));
        pn_free(str);
    }
}

pn_event_t* pn_connection_engine_dispatch(pn_connection_engine_t* e) {
    if (e->event)
        pn_collector_pop(e->collector);
    e->event = pn_collector_peek(e->collector);
    log_event(e, e->event);
    return e->event;
}

bool pn_connection_engine_finished(pn_connection_engine_t* e) {
    return pn_transport_closed(e->transport) && (pn_collector_peek(e->collector) == NULL);
}

pn_connection_t* pn_connection_engine_connection(pn_connection_engine_t* e) {
    return e->connection;
}

pn_transport_t* pn_connection_engine_transport(pn_connection_engine_t* e) {
    return e->transport;
}

pn_condition_t* pn_connection_engine_condition(pn_connection_engine_t* e) {
    return pn_transport_condition(e->transport);
}
