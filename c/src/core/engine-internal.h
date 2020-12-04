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

#include <proton/object.h>
#include <proton/engine.h>
#include <proton/types.h>

#include "buffer.h"
#include "dispatcher.h"
#include "logger_private.h"
#include "util.h"

#if __cplusplus
extern "C" {
#endif

typedef enum pn_endpoint_type_t {CONNECTION, SESSION, SENDER, RECEIVER} pn_endpoint_type_t;

typedef struct pn_endpoint_t pn_endpoint_t;

struct pn_condition_t {
  pn_string_t *name;
  pn_string_t *description;
  pn_data_t *info;
};

struct pn_endpoint_t {
  pn_condition_t condition;
  pn_condition_t remote_condition;
  pn_endpoint_t *endpoint_next;
  pn_endpoint_t *endpoint_prev;
  pn_endpoint_t *transport_next;
  pn_endpoint_t *transport_prev;
  int refcount; // when this hits zero we generate a final event
  uint8_t state;
  uint8_t type;
  bool modified;
  bool freed;
  bool referenced;
};

typedef struct {
  pn_sequence_t id;
  bool sending;
  bool sent;
  bool init;
} pn_delivery_state_t;

typedef struct {
  pn_sequence_t next;
  pn_hash_t *deliveries;
} pn_delivery_map_t;

typedef struct {
  // XXX: stop using negative numbers
  uint32_t local_handle;
  uint32_t remote_handle;
  pn_sequence_t delivery_count;
  pn_sequence_t link_credit;
} pn_link_state_t;

typedef struct {
  pn_delivery_map_t incoming;
  pn_delivery_map_t outgoing;
  pn_hash_t *local_handles;
  pn_hash_t *remote_handles;
  uint64_t disp_code;
  pn_sequence_t incoming_transfer_count;
  pn_sequence_t incoming_window;
  pn_sequence_t remote_incoming_window;
  pn_sequence_t outgoing_transfer_count;
  pn_sequence_t outgoing_window;
  pn_sequence_t disp_first;
  pn_sequence_t disp_last;
  // XXX: stop using negative numbers
  uint16_t local_channel;
  uint16_t remote_channel;
  bool incoming_init;
  bool disp;
  bool disp_settled;
  bool disp_type;
} pn_session_state_t;

typedef struct pn_io_layer_t {
  ssize_t (*process_input)(struct pn_transport_t *transport, unsigned int layer, const char *, size_t);
  ssize_t (*process_output)(struct pn_transport_t *transport, unsigned int layer, char *, size_t);
  void (*handle_error)(struct pn_transport_t* transport, unsigned int layer);
  int64_t (*process_tick)(struct pn_transport_t *transport, unsigned int layer, int64_t);
  size_t (*buffered_output)(struct pn_transport_t *transport);  // how much output is held
} pn_io_layer_t;

extern const pn_io_layer_t pni_passthru_layer;
extern const pn_io_layer_t ssl_layer;
extern const pn_io_layer_t sasl_header_layer;
extern const pn_io_layer_t sasl_write_header_layer;

// Bit flag defines for the protocol layers
typedef uint8_t pn_io_layer_flags_t;
#define LAYER_NONE     0
#define LAYER_AMQP1    1
#define LAYER_AMQPSASL 2
#define LAYER_AMQPSSL  4
#define LAYER_SSL      8

typedef struct pni_sasl_t pni_sasl_t;
typedef struct pni_ssl_t pni_ssl_t;

struct pn_transport_t {
  pn_logger_t logger;
  pn_tracer_t tracer;
  pni_sasl_t *sasl;
  pni_ssl_t *ssl;
  pn_connection_t *connection;  // reference counted
  char *remote_container;
  char *remote_hostname;
  pn_data_t *remote_offered_capabilities;
  pn_data_t *remote_desired_capabilities;
  pn_data_t *remote_properties;
  pn_data_t *disp_data;
  //#define PN_DEFAULT_MAX_FRAME_SIZE (16*1024)
/* This is wrong and bad  we should really use a sensible starting size not unlimited */
#define PN_DEFAULT_MAX_FRAME_SIZE (0)  /* for now, allow unlimited size */
  uint32_t   local_max_frame;
  uint32_t   remote_max_frame;
  pn_condition_t remote_condition;
  pn_condition_t condition;
  pn_error_t *error;

#define PN_IO_LAYER_CT 3
  const pn_io_layer_t *io_layers[PN_IO_LAYER_CT];

  /* dead remote detection */
  pn_millis_t local_idle_timeout;
  pn_millis_t remote_idle_timeout;
  pn_timestamp_t dead_remote_deadline;
  uint64_t last_bytes_input;

  /* keepalive */
  pn_timestamp_t keepalive_deadline;
  uint64_t last_bytes_output;

  pn_hash_t *local_channels;
  pn_hash_t *remote_channels;


  /* scratch area */
  pn_string_t *scratch;
  pn_data_t *args;
  pn_data_t *output_args;
  pn_buffer_t *frame;  // frame under construction

  // Temporary - ??
  pn_buffer_t *output_buffer;

  /* statistics */
  uint64_t bytes_input;
  uint64_t bytes_output;
  uint64_t output_frames_ct;
  uint64_t input_frames_ct;

  /* output buffered for send */
  #define PN_TRANSPORT_INITIAL_BUFFER_SIZE (8*1024)
  size_t output_size;
  size_t output_pending;
  char *output_buf;

  /* input from peer */
  size_t input_size;
  size_t input_pending;
  char *input_buf;

  pn_record_t *context;

  /*
   * The maximum channel number can be constrained in several ways:
   *   1. an unchangeable limit imposed by this library code
   *   2. a limit imposed by the remote peer when the connection is opened,
   *      which this app must honor
   *   3. a limit imposed by this app, which may be raised and lowered
   *      until the OPEN frame is sent.
   * These constraints are all summed up in channel_max, below.
   */
  #define PN_IMPL_CHANNEL_MAX  32767
  uint16_t local_channel_max;
  uint16_t remote_channel_max;
  uint16_t channel_max;

  pn_io_layer_flags_t allowed_layers;
  pn_io_layer_flags_t present_layers;

  bool freed;
  bool open_sent;
  bool open_rcvd;
  bool close_sent;
  bool close_rcvd;
  bool tail_closed;      // input stream closed by driver
  bool head_closed;
  bool done_processing; // if true, don't call pn_process again
  bool posted_idle_timeout;
  bool server;
  bool halt;
  bool auth_required;
  bool authenticated;
  bool encryption_required;

  bool referenced;
};

struct pn_connection_t {
  pn_endpoint_t endpoint;
  pn_endpoint_t *endpoint_head;
  pn_endpoint_t *endpoint_tail;
  pn_endpoint_t *transport_head;  // reference counted
  pn_endpoint_t *transport_tail;
  pn_list_t *sessions;
  pn_list_t *freed;
  pn_transport_t *transport;
  pn_delivery_t *work_head;
  pn_delivery_t *work_tail;
  pn_delivery_t *tpwork_head;  // reference counted
  pn_delivery_t *tpwork_tail;
  pn_string_t *container;
  pn_string_t *hostname;
  pn_string_t *auth_user;
  pn_string_t *authzid;
  pn_string_t *auth_password;
  pn_data_t *offered_capabilities;
  pn_data_t *desired_capabilities;
  pn_data_t *properties;
  pn_collector_t *collector;
  pn_record_t *context;
  pn_list_t *delivery_pool;
  struct pn_connection_driver_t *driver;
};

struct pn_session_t {
  pn_endpoint_t endpoint;
  pn_session_state_t state;
  pn_connection_t *connection;  // reference counted
  pn_list_t *links;
  pn_list_t *freed;
  pn_record_t *context;
  size_t incoming_capacity;
  pn_sequence_t incoming_bytes;
  pn_sequence_t outgoing_bytes;
  pn_sequence_t incoming_deliveries;
  pn_sequence_t outgoing_deliveries;
  pn_sequence_t outgoing_window;
};

struct pn_terminus_t {
  pn_string_t *address;
  pn_data_t *properties;
  pn_data_t *capabilities;
  pn_data_t *outcomes;
  pn_data_t *filter;
  pn_seconds_t timeout;
  uint8_t durability;
  uint8_t expiry_policy;
  uint8_t type;
  uint8_t distribution_mode;
  bool has_expiry_policy;
  bool dynamic;
};

struct pn_link_t {
  pn_endpoint_t endpoint;
  pn_terminus_t source;
  pn_terminus_t target;
  pn_terminus_t remote_source;
  pn_terminus_t remote_target;
  pn_link_state_t state;
  pn_string_t *name;
  pn_session_t *session;  // reference counted
  pn_delivery_t *unsettled_head;
  pn_delivery_t *unsettled_tail;
  pn_delivery_t *current;
  pn_record_t *context;
  pn_data_t *properties;
  pn_data_t *remote_properties;
  size_t unsettled_count;
  uint64_t max_message_size;
  uint64_t remote_max_message_size;
  pn_sequence_t available;
  pn_sequence_t credit;
  pn_sequence_t queued;
  pn_sequence_t more_id;
  int drained; // number of drained credits
  uint8_t snd_settle_mode;
  uint8_t rcv_settle_mode;
  uint8_t remote_snd_settle_mode;
  uint8_t remote_rcv_settle_mode;
  bool drain_flag_mode; // receiver only
  bool drain;
  bool detached;
  bool more_pending;
};

struct pn_disposition_t {
  pn_condition_t condition;
  uint64_t type;
  pn_data_t *data;
  pn_data_t *annotations;
  uint64_t section_offset;
  uint32_t section_number;
  bool failed;
  bool undeliverable;
  bool settled;
};

struct pn_delivery_t {
  pn_disposition_t local;
  pn_disposition_t remote;
  pn_link_t *link;  // reference counted
  pn_buffer_t *tag;
  pn_delivery_t *unsettled_next;
  pn_delivery_t *unsettled_prev;
  pn_delivery_t *work_next;
  pn_delivery_t *work_prev;
  pn_delivery_t *tpwork_next;
  pn_delivery_t *tpwork_prev;
  pn_delivery_state_t state;
  pn_buffer_t *bytes;
  pn_record_t *context;
  bool updated;
  bool settled; // tracks whether we're in the unsettled list or not
  bool work;
  bool tpwork;
  bool done;
  bool referenced;
  bool aborted;
};

#define PN_SET_LOCAL(OLD, NEW)                                          \
  (OLD) = ((OLD) & PN_REMOTE_MASK) | (NEW)

#define PN_SET_REMOTE(OLD, NEW)                                         \
  (OLD) = ((OLD) & PN_LOCAL_MASK) | (NEW)

void pn_link_dump(pn_link_t *link);

void pn_dump(pn_connection_t *conn);
void pn_transport_sasl_init(pn_transport_t *transport);

void pn_condition_init(pn_condition_t *condition);
void pn_condition_tini(pn_condition_t *condition);
void pn_modified(pn_connection_t *connection, pn_endpoint_t *endpoint, bool emit);
void pn_real_settle(pn_delivery_t *delivery);  // will free delivery if link is freed
void pn_clear_tpwork(pn_delivery_t *delivery);
void pn_work_update(pn_connection_t *connection, pn_delivery_t *delivery);
void pn_clear_modified(pn_connection_t *connection, pn_endpoint_t *endpoint);
void pn_connection_bound(pn_connection_t *conn);
void pn_connection_unbound(pn_connection_t *conn);
int pn_do_error(pn_transport_t *transport, const char *condition, const char *fmt, ...);
void pn_set_error_layer(pn_transport_t *transport);
void pn_session_unbound(pn_session_t* ssn);
void pn_link_unbound(pn_link_t* link);
void pn_ep_incref(pn_endpoint_t *endpoint);
void pn_ep_decref(pn_endpoint_t *endpoint);

int pn_post_frame(pn_transport_t *transport, uint8_t type, uint16_t ch, const char *fmt, ...);

typedef enum {IN, OUT} pn_dir_t;

void pn_do_trace(pn_transport_t *transport, uint16_t ch, pn_dir_t dir,
                 pn_data_t *args, const char *payload, size_t size);

#if __cplusplus
}
#endif

#endif /* engine-internal.h */
