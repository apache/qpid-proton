#ifndef _PROTON_ENGINE_INTERNAL_H
#define _PROTON_ENGINE_INTERNAL_H 1

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

#include <proton/engine.h>
#include <proton/value.h>
#include "../dispatcher/dispatcher.h"
#include "../util.h"

#define DESCRIPTION (1024)

struct pn_error_t {
  const char *condition;
  char description[DESCRIPTION];
  pn_map_t *info;
};

typedef enum pn_endpoint_type_t {CONNECTION, SESSION, SENDER, RECEIVER, TRANSPORT} pn_endpoint_type_t;

typedef struct pn_endpoint_t pn_endpoint_t;

struct pn_endpoint_t {
  pn_endpoint_type_t type;
  pn_state_t state;
  pn_error_t error;
  pn_endpoint_t *endpoint_next;
  pn_endpoint_t *endpoint_prev;
  pn_endpoint_t *transport_next;
  pn_endpoint_t *transport_prev;
  bool modified;
};

typedef int32_t pn_sequence_t;

typedef struct {
  pn_delivery_t *delivery;
  pn_sequence_t id;
  bool sent;
} pn_delivery_state_t;

typedef struct {
  pn_sequence_t next;
  size_t capacity;
  size_t head;
  size_t size;
  pn_delivery_state_t *deliveries;
} pn_delivery_buffer_t;

typedef struct {
  pn_link_t *link;
  // XXX: stop using negative numbers
  uint32_t local_handle;
  uint32_t remote_handle;
  pn_sequence_t delivery_count;
  // XXX: this is only used for receiver
  pn_sequence_t link_credit;
} pn_link_state_t;

typedef struct {
  pn_session_t *session;
  // XXX: stop using negative numbers
  uint16_t local_channel;
  uint16_t remote_channel;
  pn_delivery_buffer_t incoming;
  pn_delivery_buffer_t outgoing;
  pn_link_state_t *links;
  size_t link_capacity;
  pn_link_state_t **handles;
  size_t handle_capacity;
} pn_session_state_t;

#define SCRATCH (1024)

struct pn_transport_t {
  pn_endpoint_t endpoint;
  pn_connection_t *connection;
  pn_dispatcher_t *disp;
  bool open_sent;
  bool close_sent;
  pn_session_state_t *sessions;
  size_t session_capacity;
  pn_session_state_t **channels;
  size_t channel_capacity;
  char scratch[SCRATCH];
};

struct pn_connection_t {
  pn_endpoint_t endpoint;
  pn_endpoint_t *endpoint_head;
  pn_endpoint_t *endpoint_tail;
  pn_endpoint_t *transport_head;
  pn_endpoint_t *transport_tail;
  pn_session_t **sessions;
  size_t session_capacity;
  size_t session_count;
  pn_transport_t *transport;
  pn_delivery_t *work_head;
  pn_delivery_t *work_tail;
  pn_delivery_t *tpwork_head;
  pn_delivery_t *tpwork_tail;
  wchar_t *container;
  wchar_t *hostname;
};

struct pn_session_t {
  pn_endpoint_t endpoint;
  pn_connection_t *connection;
  pn_link_t **links;
  size_t link_capacity;
  size_t link_count;
  size_t id;
};

struct pn_link_t {
  pn_endpoint_t endpoint;
  wchar_t *name;
  pn_session_t *session;
  wchar_t *local_source;
  wchar_t *local_target;
  wchar_t *remote_source;
  wchar_t *remote_target;
  pn_delivery_t *head;
  pn_delivery_t *tail;
  pn_delivery_t *current;
  pn_delivery_t *settled_head;
  pn_delivery_t *settled_tail;
  // XXX
  pn_sequence_t credit;
  pn_sequence_t credits;
  size_t id;
};

struct pn_delivery_t {
  pn_link_t *link;
  pn_binary_t *tag;
  int local_state;
  int remote_state;
  bool local_settled;
  bool remote_settled;
  bool dirty;
  pn_delivery_t *link_next;
  pn_delivery_t *link_prev;
  pn_delivery_t *work_next;
  pn_delivery_t *work_prev;
  bool work;
  pn_delivery_t *tpwork_next;
  pn_delivery_t *tpwork_prev;
  bool tpwork;
  char *bytes;
  size_t size;
  size_t capacity;
  bool done;
  void *context;
};

#define PN_SET_LOCAL(OLD, NEW)                                          \
  (OLD) = ((OLD) & (PN_REMOTE_UNINIT | PN_REMOTE_ACTIVE | PN_REMOTE_CLOSED)) | (NEW)

#define PN_SET_REMOTE(OLD, NEW)                                         \
  (OLD) = ((OLD) & (PN_LOCAL_UNINIT | PN_LOCAL_ACTIVE | PN_LOCAL_CLOSED)) | (NEW)

void pn_link_dump(pn_link_t *link);

#define PN_ENSURE(ARRAY, CAPACITY, COUNT)                      \
  while ((CAPACITY) < (COUNT)) {                                \
    (CAPACITY) = (CAPACITY) ? 2 * (CAPACITY) : 16;              \
    (ARRAY) = realloc((ARRAY), (CAPACITY) * sizeof (*(ARRAY))); \
  }                                                             \

#define PN_ENSUREZ(ARRAY, CAPACITY, COUNT)                \
  {                                                        \
    size_t _old_capacity = (CAPACITY);                     \
    PN_ENSURE((ARRAY), (CAPACITY), (COUNT));              \
    memset((ARRAY) + _old_capacity, 0,                     \
           sizeof(*(ARRAY))*((CAPACITY) - _old_capacity)); \
  }

void pn_dump(pn_connection_t *conn);

#endif /* engine-internal.h */
