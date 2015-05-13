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

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <proton/handlers.h>
#include <assert.h>

typedef struct {
  pn_map_t *handlers;
} pni_handshaker_t;

pni_handshaker_t *pni_handshaker(pn_handler_t *handler) {
  return (pni_handshaker_t *) pn_handler_mem(handler);
}

static void pn_handshaker_finalize(pn_handler_t *handler) {
  pni_handshaker_t *handshaker = pni_handshaker(handler);
  pn_free(handshaker->handlers);
}

static void pn_handshaker_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  switch (type) {
  case PN_CONNECTION_REMOTE_OPEN:
    {
      pn_connection_t *conn = pn_event_connection(event);
      if (pn_connection_state(conn) & PN_LOCAL_UNINIT) {
        pn_connection_open(conn);
      }
    }
    break;
  case PN_SESSION_REMOTE_OPEN:
    {
      pn_session_t *ssn = pn_event_session(event);
      if (pn_session_state(ssn) & PN_LOCAL_UNINIT) {
        pn_session_open(ssn);
      }
    }
    break;
  case PN_LINK_REMOTE_OPEN:
    {
      pn_link_t *link = pn_event_link(event);
      if (pn_link_state(link) & PN_LOCAL_UNINIT) {
        pn_terminus_copy(pn_link_source(link), pn_link_remote_source(link));
        pn_terminus_copy(pn_link_target(link), pn_link_remote_target(link));
        pn_link_open(link);
      }
    }
    break;
  case PN_CONNECTION_REMOTE_CLOSE:
    {
      pn_connection_t *conn = pn_event_connection(event);
      if (!(pn_connection_state(conn) & PN_LOCAL_CLOSED)) {
        pn_connection_close(conn);
      }
    }
    break;
  case PN_SESSION_REMOTE_CLOSE:
    {
      pn_session_t *ssn = pn_event_session(event);
      if (!(pn_session_state(ssn) & PN_LOCAL_CLOSED)) {
        pn_session_close(ssn);
      }
    }
    break;
  case PN_LINK_REMOTE_CLOSE:
    {
      pn_link_t *link = pn_event_link(event);
      if (!(pn_link_state(link) & PN_LOCAL_CLOSED)) {
        pn_link_close(link);
      }
    }
    break;
  default:
    break;
  }
}

pn_handshaker_t *pn_handshaker(void) {
  pn_handler_t *handler = pn_handler_new(pn_handshaker_dispatch, sizeof(pni_handshaker_t), pn_handshaker_finalize);
  pni_handshaker_t *handshaker = pni_handshaker(handler);
  handshaker->handlers = NULL;
  return handler;
}
