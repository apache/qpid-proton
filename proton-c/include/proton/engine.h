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
#include <proton/value.h>

typedef struct pn_error_t pn_error_t;
typedef struct pn_endpoint_t pn_endpoint_t;
typedef struct pn_transport_t pn_transport_t;
typedef struct pn_connection_t pn_connection_t;
typedef struct pn_session_t pn_session_t;
typedef struct pn_link_t pn_link_t;
typedef struct pn_sender_t pn_sender_t;
typedef struct pn_receiver_t pn_receiver_t;
typedef struct pn_delivery_t pn_delivery_t;

typedef enum pn_endpoint_state_t {UNINIT=1, ACTIVE=2, CLOSED=4} pn_endpoint_state_t;
typedef enum pn_endpoint_type_t {CONNECTION=1, TRANSPORT=2, SESSION=3, SENDER=4, RECEIVER=5} pn_endpoint_type_t;
typedef enum pn_disposition_t {PN_RECEIVED=1, PN_ACCEPTED=2, PN_REJECTED=3, PN_RELEASED=4, PN_MODIFIED=5} pn_disposition_t;

typedef enum pn_trace_t {PN_TRACE_OFF=0, PN_TRACE_RAW=1, PN_TRACE_FRM=2} pn_trace_t;

/* Currently the way inheritence is done it is safe to "upcast" from
   pn_{transport,connection,session,link,sender,or receiver}_t to
   pn_endpoint_t and to "downcast" based on the endpoint type. I'm
   not sure if this should be part of the ABI or not. */

// endpoint
pn_endpoint_type_t pn_endpoint_type(pn_endpoint_t *endpoint);
pn_endpoint_state_t pn_local_state(pn_endpoint_t *endpoint);
pn_endpoint_state_t pn_remote_state(pn_endpoint_t *endpoint);
pn_error_t *pn_local_error(pn_endpoint_t *endpoint);
pn_error_t *pn_remote_error(pn_endpoint_t *endpoint);
void pn_destroy(pn_endpoint_t *endpoint);
void pn_open(pn_endpoint_t *endpoint);
void pn_close(pn_endpoint_t *endpoint);

// connection
pn_connection_t *pn_connection();
void pn_connection_set_container(pn_connection_t *connection, const wchar_t *container);
void pn_connection_set_hostname(pn_connection_t *connection, const wchar_t *hostname);
pn_delivery_t *pn_work_head(pn_connection_t *connection);
pn_delivery_t *pn_work_next(pn_delivery_t *delivery);

pn_session_t *pn_session(pn_connection_t *connection);
pn_transport_t *pn_transport(pn_connection_t *connection);

pn_endpoint_t *pn_endpoint_head(pn_connection_t *connection,
                                pn_endpoint_state_t local,
                                pn_endpoint_state_t remote);
pn_endpoint_t *pn_endpoint_next(pn_endpoint_t *endpoint,
                                pn_endpoint_state_t local,
                                pn_endpoint_state_t remote);

// transport
#define PN_EOS (-1)
#define PN_ERR (-2)
ssize_t pn_input(pn_transport_t *transport, char *bytes, size_t available);
ssize_t pn_output(pn_transport_t *transport, char *bytes, size_t size);
time_t pn_tick(pn_transport_t *transport, time_t now);
void pn_trace(pn_transport_t *transport, pn_trace_t trace);

// session
pn_sender_t *pn_sender(pn_session_t *session, const wchar_t *name);
pn_receiver_t *pn_receiver(pn_session_t *session, const wchar_t *name);

// link
pn_session_t *pn_get_session(pn_link_t *link);
void pn_set_source(pn_link_t *link, const wchar_t *source);
void pn_set_target(pn_link_t *link, const wchar_t *target);
wchar_t *pn_remote_source(pn_link_t *link);
wchar_t *pn_remote_target(pn_link_t *link);
pn_delivery_t *pn_delivery(pn_link_t *link, pn_binary_t *tag);
pn_delivery_t *pn_current(pn_link_t *link);
bool pn_advance(pn_link_t *link);

pn_delivery_t *pn_unsettled_head(pn_link_t *link);
pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery);

// sender
//void pn_offer(pn_sender_t *sender, int credits);
ssize_t pn_send(pn_sender_t *sender, const char *bytes, size_t n);
//void pn_abort(pn_sender_t *sender);

// receiver
#define PN_EOM (-1)
void pn_flow(pn_receiver_t *receiver, int credits);
ssize_t pn_recv(pn_receiver_t *receiver, char *bytes, size_t n);

// delivery
pn_binary_t *pn_delivery_tag(pn_delivery_t *delivery);
pn_link_t *pn_link(pn_delivery_t *delivery);
// how do we do delivery state?
int pn_local_disp(pn_delivery_t *delivery);
int pn_remote_disp(pn_delivery_t *delivery);
size_t pn_pending(pn_delivery_t *delivery);
bool pn_writable(pn_delivery_t *delivery);
bool pn_readable(pn_delivery_t *delivery);
bool pn_dirty(pn_delivery_t *delivery);
void pn_clean(pn_delivery_t *delivery);
void pn_disposition(pn_delivery_t *delivery, pn_disposition_t disposition);
//int pn_format(pn_delivery_t *delivery);
void pn_settle(pn_delivery_t *delivery);
void pn_delivery_dump(pn_delivery_t *delivery);

#endif /* engine.h */
