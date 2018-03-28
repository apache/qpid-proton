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

#include <proton/import_export.h>
#include <proton/type_compat.h>
#include <proton/error.h>

#define AMQP_HEADER_SIZE (8)
#define AMQP_MIN_MAX_FRAME_SIZE ((uint32_t)512) // minimum allowable max-frame
#define AMQP_MAX_WINDOW_SIZE (2147483647)

typedef struct {
  uint8_t type;
  uint16_t channel;
  size_t ex_size;
  const char *extended;
  size_t size;
  const char *payload;
} pn_frame_t;

ssize_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available, uint32_t max);
size_t pn_write_frame(pn_buffer_t* buffer, pn_frame_t frame);

#endif /* framing.h */
