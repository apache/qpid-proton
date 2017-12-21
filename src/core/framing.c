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

#include <stdio.h>
#include <string.h>

#include "framing.h"

// TODO: These are near duplicates of code in codec.c - they should be
// deduplicated.
static inline void pn_i_write16(char *bytes, uint16_t value)
{
    bytes[0] = 0xFF & (value >> 8);
    bytes[1] = 0xFF & (value     );
}


static inline void pn_i_write32(char *bytes, uint32_t value)
{
    bytes[0] = 0xFF & (value >> 24);
    bytes[1] = 0xFF & (value >> 16);
    bytes[2] = 0xFF & (value >>  8);
    bytes[3] = 0xFF & (value      );
}

static inline uint16_t pn_i_read16(const char *bytes)
{
    uint16_t a = (uint8_t) bytes[0];
    uint16_t b = (uint8_t) bytes[1];
    uint16_t r = a << 8
    | b;
    return r;
}

static inline uint32_t pn_i_read32(const char *bytes)
{
    uint32_t a = (uint8_t) bytes[0];
    uint32_t b = (uint8_t) bytes[1];
    uint32_t c = (uint8_t) bytes[2];
    uint32_t d = (uint8_t) bytes[3];
    uint32_t r = a << 24
    | b << 16
    | c <<  8
    | d;
    return r;
}


ssize_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available, uint32_t max)
{
  if (available < AMQP_HEADER_SIZE) return 0;
  uint32_t size = pn_i_read32(&bytes[0]);
  if (max && size > max) return PN_ERR;
  if (available < size) return 0;
  unsigned int doff = 4 * (uint8_t)bytes[4];
  if (doff < AMQP_HEADER_SIZE || doff > size) return PN_ERR;

  frame->size = size - doff;
  frame->ex_size = doff - AMQP_HEADER_SIZE;
  frame->type = bytes[5];
  frame->channel = pn_i_read16(&bytes[6]);
  frame->extended = bytes + AMQP_HEADER_SIZE;
  frame->payload = bytes + doff;

  return size;
}

size_t pn_write_frame(pn_buffer_t* buffer, pn_frame_t frame)
{
  size_t size = AMQP_HEADER_SIZE + frame.ex_size + frame.size;
  if (size <= pn_buffer_available(buffer))
  {
    // Prepare header
    char bytes[8];
    pn_i_write32(&bytes[0], size);
    int doff = (frame.ex_size + AMQP_HEADER_SIZE - 1)/4 + 1;
    bytes[4] = doff;
    bytes[5] = frame.type;
    pn_i_write16(&bytes[6], frame.channel);

    // Write header then rest of frame
    pn_buffer_append(buffer, bytes, 8);
    if (frame.extended)
        pn_buffer_append(buffer, frame.extended, frame.ex_size);
    pn_buffer_append(buffer, frame.payload, frame.size);
    return size;
  } else {
    return 0;
  }
}
