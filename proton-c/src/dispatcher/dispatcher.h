#ifndef _PROTON_DISPATCHER_H
#define _PROTON_DISPATCHER_H 1

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

#include <sys/types.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <proton/buffer.h>
#include <proton/codec.h>

typedef struct pn_dispatcher_t pn_dispatcher_t;

typedef int (pn_action_t)(pn_dispatcher_t *disp);

#define SCRATCH (1024)
#define CODEC_LIMIT (1024)

struct pn_dispatcher_t {
  pn_action_t *actions[256];
  uint8_t frame_type;
  pn_trace_t trace;
  pn_buffer_t *input;
  size_t fragment;
  uint16_t channel;
  uint8_t code;
  pn_data_t *args;
  const char *payload;
  size_t size;
  pn_data_t *output_args;
  const char *output_payload;
  size_t output_size;
  size_t remote_max_frame;
  pn_buffer_t *frame;  // frame under construction
  size_t capacity;
  size_t available; /* number of raw bytes pending output */
  char *output;
  pn_transport_t *transport;
  bool halt;
  bool batch;
  uint64_t output_frames_ct;
  uint64_t input_frames_ct;
  pn_string_t *scratch;
};

pn_dispatcher_t *pn_dispatcher(uint8_t frame_type, pn_transport_t *transport);
void pn_dispatcher_free(pn_dispatcher_t *disp);
void pn_dispatcher_action(pn_dispatcher_t *disp, uint8_t code,
                          pn_action_t *action);
int pn_scan_args(pn_dispatcher_t *disp, const char *fmt, ...);
void pn_set_payload(pn_dispatcher_t *disp, const char *data, size_t size);
int pn_post_frame(pn_dispatcher_t *disp, uint16_t ch, const char *fmt, ...);
ssize_t pn_dispatcher_input(pn_dispatcher_t *disp, const char *bytes, size_t available);
ssize_t pn_dispatcher_output(pn_dispatcher_t *disp, char *bytes, size_t size);
int pn_post_transfer_frame(pn_dispatcher_t *disp,
                           uint16_t local_channel,
                           uint32_t handle,
                           pn_sequence_t delivery_id,
                           const pn_bytes_t *delivery_tag,
                           uint32_t message_format,
                           bool settled,
                           bool more,
                           pn_sequence_t frame_limit);
#endif /* dispatcher.h */
