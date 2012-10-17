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
#include <arpa/inet.h>
#include <proton/framing.h>

size_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available)
{
  if (available >= AMQP_HEADER_SIZE) {
    size_t size = htonl(*((uint32_t *) bytes));
    if (available >= size)
    {
      int doff = bytes[4]*4;
      frame->size = size - doff;
      frame->ex_size = doff - AMQP_HEADER_SIZE;
      frame->type = bytes[5];
      frame->channel = htons(*((uint16_t *) (bytes + 6)));

      frame->extended = bytes + AMQP_HEADER_SIZE;
      frame->payload = bytes + doff;
      return size;
    }
  }

  return 0;
}

size_t pn_write_frame(char *bytes, size_t available, pn_frame_t frame)
{
  size_t size = AMQP_HEADER_SIZE + frame.ex_size + frame.size;
  if (size <= available)
  {
    *((uint32_t *) bytes) = ntohl(size);
    int doff = (frame.ex_size + AMQP_HEADER_SIZE - 1)/4 + 1;
    bytes[4] = doff;
    bytes[5] = frame.type;
    *((uint16_t *) (bytes + 6)) = ntohs(frame.channel);

    memmove(bytes + AMQP_HEADER_SIZE, frame.extended, frame.ex_size);
    memmove(bytes + 4*doff, frame.payload, frame.size);
    return size;
  } else {
    return 0;
  }
}
