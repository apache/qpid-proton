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


#include "framing.h"

#include "engine-internal.h"
#include "util.h"

ssize_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available, uint32_t max)
{
  if (available < AMQP_HEADER_SIZE) return 0;
  uint32_t size = pni_read32(&bytes[0]);
  if (max && size > max) return PN_ERR;
  if (available < size) return 0;
  unsigned int doff = 4 * (uint8_t)bytes[4];
  if (doff < AMQP_HEADER_SIZE || doff > size) return PN_ERR;

  frame->size = size - doff;
  frame->ex_size = doff - AMQP_HEADER_SIZE;
  frame->type = bytes[5];
  frame->channel = pni_read16(&bytes[6]);
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
    pni_write32(&bytes[0], size);
    int doff = (frame.ex_size + AMQP_HEADER_SIZE - 1)/4 + 1;
    bytes[4] = doff;
    bytes[5] = frame.type;
    pni_write16(&bytes[6], frame.channel);

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
