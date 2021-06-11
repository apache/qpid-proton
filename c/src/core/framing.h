#ifndef PROTON_FRAMING_H
#define PROTON_FRAMING_H 1

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

#include "buffer.h"
#include "logger_private.h"

#include "proton/codec.h"
#include "proton/types.h"

#include <stddef.h>

#define AMQP_HEADER_SIZE (8)
#define AMQP_MIN_MAX_FRAME_SIZE ((uint32_t)512) // minimum allowable max-frame
#define AMQP_MAX_WINDOW_SIZE (2147483647)

#define AMQP_FRAME_TYPE (0)
#define SASL_FRAME_TYPE (1)

typedef struct {
  uint8_t type;
  uint16_t channel;
  pn_bytes_t extended;
  pn_bytes_t frame_payload0;
  pn_bytes_t frame_payload1;
} pn_frame_t;

ssize_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available, uint32_t max, pn_logger_t *logger);
size_t pn_write_frame(pn_buffer_t* buffer, pn_frame_t frame, pn_logger_t *logger);

int pn_framing_send_amqp(pn_transport_t *transport, uint16_t ch, pn_bytes_t performative);
int pn_framing_send_amqp_with_payload(pn_transport_t *transport, uint16_t ch, pn_bytes_t performative, pn_bytes_t payload);
int pn_framing_send_sasl(pn_transport_t *transport, pn_bytes_t performative);

ssize_t pn_framing_recv_amqp(pn_data_t *args, pn_logger_t  *logger, const pn_bytes_t frame_payload);

#endif /* framing.h */
