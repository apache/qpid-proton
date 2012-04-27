#ifndef _PROTON_ENGINE_H
#define _PROTON_ENGINE_H 1

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

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <proton/errors.h>

typedef struct pn_error_t pn_error_t;
typedef struct pn_transport_t pn_transport_t;
typedef struct pn_connection_t pn_connection_t;
typedef struct pn_session_t pn_session_t;
typedef struct pn_link_t pn_link_t;
typedef struct pn_delivery_t pn_delivery_t;

typedef struct pn_delivery_tag_t {
  size_t size;
  const char *bytes;
} pn_delivery_tag_t;

#define pn_dtag(BYTES, SIZE) ((pn_delivery_tag_t) {(SIZE), (BYTES)})

typedef int pn_state_t;

#define PN_LOCAL_UNINIT (1)
#define PN_LOCAL_ACTIVE (2)
#define PN_LOCAL_CLOSED (4)
#define PN_REMOTE_UNINIT (8)
#define PN_REMOTE_ACTIVE (16)
#define PN_REMOTE_CLOSED (32)

#define PN_LOCAL_MASK (PN_LOCAL_UNINIT | PN_LOCAL_ACTIVE | PN_LOCAL_CLOSED)
#define PN_REMOTE_MASK (PN_REMOTE_UNINIT | PN_REMOTE_ACTIVE | PN_REMOTE_CLOSED)

typedef enum pn_disposition_t {
  PN_RECEIVED=1,
  PN_ACCEPTED=2,
  PN_REJECTED=3,
  PN_RELEASED=4,
  PN_MODIFIED=5
} pn_disposition_t;

typedef int pn_trace_t;

#define PN_TRACE_OFF (0)
#define PN_TRACE_RAW (1)
#define PN_TRACE_FRM (2)

// connection
pn_connection_t *pn_connection();

pn_state_t pn_connection_state(pn_connection_t *connection);
pn_error_t *pn_connection_error(pn_connection_t *connection);
void pn_connection_set_container(pn_connection_t *connection, const wchar_t *container);
void pn_connection_set_hostname(pn_connection_t *connection, const wchar_t *hostname);

pn_delivery_t *pn_work_head(pn_connection_t *connection);
pn_delivery_t *pn_work_next(pn_delivery_t *delivery);

pn_session_t *pn_session(pn_connection_t *connection);
pn_transport_t *pn_transport(pn_connection_t *connection);

pn_session_t *pn_session_head(pn_connection_t *connection, pn_state_t state);
pn_session_t *pn_session_next(pn_session_t *session, pn_state_t state);

pn_link_t *pn_link_head(pn_connection_t *connection, pn_state_t state);
pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state);

void pn_connection_open(pn_connection_t *connection);
void pn_connection_close(pn_connection_t *connection);
void pn_connection_destroy(pn_connection_t *connection);

// transport
pn_state_t pn_transport_state(pn_transport_t *transport);
pn_error_t *pn_transport_error(pn_transport_t *transport);
ssize_t pn_input(pn_transport_t *transport, char *bytes, size_t available);
ssize_t pn_output(pn_transport_t *transport, char *bytes, size_t size);
time_t pn_tick(pn_transport_t *transport, time_t now);
void pn_trace(pn_transport_t *transport, pn_trace_t trace);
void pn_transport_open(pn_transport_t *transport);
void pn_transport_close(pn_transport_t *transport);
void pn_transport_destroy(pn_transport_t *transport);

// session
pn_state_t pn_session_state(pn_session_t *session);
pn_error_t *pn_session_error(pn_session_t *session);
pn_link_t *pn_sender(pn_session_t *session, const wchar_t *name);
pn_link_t *pn_receiver(pn_session_t *session, const wchar_t *name);
void pn_session_open(pn_session_t *session);
void pn_session_close(pn_session_t *session);
void pn_session_destroy(pn_session_t *session);

// link
bool pn_is_sender(pn_link_t *link);
bool pn_is_receiver(pn_link_t *link);
pn_state_t pn_link_state(pn_link_t *link);
pn_error_t *pn_link_error(pn_link_t *link);
pn_session_t *pn_get_session(pn_link_t *link);
void pn_set_source(pn_link_t *link, const wchar_t *source);
void pn_set_target(pn_link_t *link, const wchar_t *target);
wchar_t *pn_remote_source(pn_link_t *link);
wchar_t *pn_remote_target(pn_link_t *link);
pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
pn_delivery_t *pn_current(pn_link_t *link);
bool pn_advance(pn_link_t *link);
int pn_credit(pn_link_t *link);

pn_delivery_t *pn_unsettled_head(pn_link_t *link);
pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery);

void pn_link_open(pn_link_t *sender);
void pn_link_close(pn_link_t *sender);
void pn_link_destroy(pn_link_t *sender);

// sender
//void pn_offer(pn_sender_t *sender, int credits);
ssize_t pn_send(pn_link_t *sender, const char *bytes, size_t n);
//void pn_abort(pn_sender_t *sender);

// receiver
void pn_flow(pn_link_t *receiver, int credits);
ssize_t pn_recv(pn_link_t *receiver, char *bytes, size_t n);

// delivery
pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
pn_link_t *pn_link(pn_delivery_t *delivery);
// how do we do delivery state?
int pn_local_disp(pn_delivery_t *delivery);
int pn_remote_disp(pn_delivery_t *delivery);
size_t pn_pending(pn_delivery_t *delivery);
bool pn_writable(pn_delivery_t *delivery);
bool pn_readable(pn_delivery_t *delivery);
bool pn_updated(pn_delivery_t *delivery);
void pn_clear(pn_delivery_t *delivery);
void pn_disposition(pn_delivery_t *delivery, pn_disposition_t disposition);
//int pn_format(pn_delivery_t *delivery);
void pn_settle(pn_delivery_t *delivery);
void pn_delivery_dump(pn_delivery_t *delivery);

#endif /* engine.h */
