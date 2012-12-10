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

#include <proton/buffer.h>
#include <proton/engine.h>
#include <proton/types.h>
#include "../dispatcher/dispatcher.h"
#include "../util.h"

typedef enum pn_endpoint_type_t {CONNECTION, SESSION, SENDER, RECEIVER, TRANSPORT} pn_endpoint_type_t;

typedef struct pn_endpoint_t pn_endpoint_t;

#define COND_NAME_MAX (256)
#define COND_DESC_MAX (1024)

struct pn_condition_t {
  char name[COND_NAME_MAX];
  char description[COND_DESC_MAX];
  pn_data_t *info;
};

struct pn_endpoint_t {
  pn_endpoint_type_t type;
  pn_state_t state;
  pn_error_t *error;
  pn_condition_t condition;
  pn_endpoint_t *endpoint_next;
  pn_endpoint_t *endpoint_prev;
  pn_endpoint_t *transport_next;
  pn_endpoint_t *transport_prev;
  bool modified;
};

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
  pn_sequence_t link_credit;
} pn_link_state_t;

typedef struct {
  pn_session_t *session;
  // XXX: stop using negative numbers
  uint16_t local_channel;
  uint16_t remote_channel;
  bool incoming_init;
  pn_delivery_buffer_t incoming;
  pn_delivery_buffer_t outgoing;
  pn_sequence_t incoming_transfer_count;
  pn_sequence_t incoming_window;
  pn_sequence_t outgoing_transfer_count;
  pn_sequence_t outgoing_window;
  pn_link_state_t *links;
  size_t link_capacity;
  pn_link_state_t **handles;
  size_t handle_capacity;

  uint64_t disp_code;
  bool disp_settled;
  bool disp_type;
  pn_sequence_t disp_first;
  pn_sequence_t disp_last;
  bool disp;
} pn_session_state_t;

#define SCRATCH (1024)

#include <proton/sasl.h>
#include <proton/ssl.h>

struct pn_transport_t {
  ssize_t (*process_input)(pn_transport_t *, const char *, size_t);
  ssize_t (*process_output)(pn_transport_t *, char *, size_t);
  pn_timestamp_t (*process_tick)(pn_transport_t *, pn_timestamp_t);
  size_t header_count;
  pn_sasl_t *sasl;
  pn_ssl_t *ssl;
  pn_connection_t *connection;
  pn_dispatcher_t *disp;
  bool open_sent;
  bool open_rcvd;
  bool close_sent;
  bool close_rcvd;
  char *remote_container;
  char *remote_hostname;
  pn_data_t *remote_offered_capabilities;
  pn_data_t *remote_desired_capabilities;
  uint32_t   local_max_frame;
  uint32_t   remote_max_frame;
  pn_condition_t remote_condition;

  /* dead remote detection */
  pn_millis_t local_idle_timeout;
  pn_timestamp_t dead_remote_deadline;
  uint64_t last_bytes_input;

  /* keepalive */
  pn_millis_t remote_idle_timeout;
  pn_timestamp_t keepalive_deadline;
  uint64_t last_bytes_output;

  pn_error_t *error;
  pn_session_state_t *sessions;
  size_t session_capacity;
  pn_session_state_t **channels;
  size_t channel_capacity;
  char scratch[SCRATCH];

  /* statistics */
  uint64_t bytes_input;
  uint64_t bytes_output;
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
  char *container;
  char *hostname;
  pn_data_t *offered_capabilities;
  pn_data_t *desired_capabilities;
  void *context;
};

struct pn_session_t {
  pn_endpoint_t endpoint;
  pn_connection_t *connection;
  pn_link_t **links;
  size_t link_capacity;
  size_t link_count;
  size_t id;
  void *context;
};

struct pn_terminus_t {
  pn_terminus_type_t type;
  char *address;
  pn_durability_t durability;
  pn_expiry_policy_t expiry_policy;
  pn_seconds_t timeout;
  bool dynamic;
  pn_data_t *properties;
  pn_data_t *capabilities;
  pn_data_t *outcomes;
  pn_data_t *filter;
};

struct pn_link_t {
  pn_endpoint_t endpoint;
  char *name;
  pn_session_t *session;
  pn_terminus_t source;
  pn_terminus_t target;
  pn_terminus_t remote_source;
  pn_terminus_t remote_target;
  pn_delivery_t *unsettled_head;
  pn_delivery_t *unsettled_tail;
  pn_delivery_t *current;
  pn_delivery_t *settled_head;
  pn_delivery_t *settled_tail;
  size_t unsettled_count;
  pn_sequence_t available;
  pn_sequence_t credit;
  pn_sequence_t queued;
  bool drain;
  bool drained; // sender only
  size_t id;
  void *context;
};

struct pn_delivery_t {
  pn_link_t *link;
  pn_buffer_t *tag;
  int local_state;
  int remote_state;
  bool local_settled;
  bool remote_settled;
  bool updated;
  bool settled; // tracks whether we're in the unsettled list or not
  pn_delivery_t *unsettled_next;
  pn_delivery_t *unsettled_prev;
  pn_delivery_t *settled_next;
  pn_delivery_t *settled_prev;
  pn_delivery_t *work_next;
  pn_delivery_t *work_prev;
  bool work;
  pn_delivery_t *tpwork_next;
  pn_delivery_t *tpwork_prev;
  bool tpwork;
  pn_buffer_t *bytes;
  bool done;
  void *transport_context;
  void *context;
};

#define PN_SET_LOCAL(OLD, NEW)                                          \
  (OLD) = ((OLD) & PN_REMOTE_MASK) | (NEW)

#define PN_SET_REMOTE(OLD, NEW)                                         \
  (OLD) = ((OLD) & PN_LOCAL_MASK) | (NEW)

void pn_link_dump(pn_link_t *link);

void pn_dump(pn_connection_t *conn);
void pn_transport_sasl_init(pn_transport_t *transport);

#endif /* engine-internal.h */
