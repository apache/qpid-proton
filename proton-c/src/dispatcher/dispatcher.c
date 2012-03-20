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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <proton/framing.h>
#include <proton/engine.h>
#include "dispatcher.h"
#include "../util.h"

pn_dispatcher_t *pn_dispatcher(uint8_t frame_type, void *context)
{
  pn_dispatcher_t *disp = calloc(sizeof(pn_dispatcher_t), 1);

  disp->frame_type = frame_type;
  disp->context = context;
  disp->trace = PN_TRACE_OFF;

  disp->channel = 0;
  disp->code = 0;
  disp->args = NULL;
  disp->payload = NULL;
  disp->size = 0;

  disp->output_args = pn_list(16);
  // XXX
  disp->capacity = 4*1024;
  disp->output = malloc(disp->capacity);
  disp->available = 0;

  return disp;
}

void pn_dispatcher_destroy(pn_dispatcher_t *disp)
{
  pn_free_list(disp->output_args);
  free(disp->output);
  free(disp);
}

void pn_dispatcher_action(pn_dispatcher_t *disp, uint8_t code, const char *name,
                          pn_action_t *action)
{
  disp->actions[code] = action;
  disp->names[code] = name;
}

typedef enum {IN, OUT} pn_dir_t;

static void pn_do_trace(pn_dispatcher_t *disp, uint16_t ch, pn_dir_t dir,
                        uint8_t code, pn_list_t *args, const char *payload,
                        size_t size)
{
  if (disp->trace & PN_TRACE_FRM) {
    pn_format(disp->scratch, SCRATCH, pn_from_list(args));
    fprintf(stderr, "[%u] %s %s %s", ch, dir == OUT ? "->" : "<-",
            disp->names[code], disp->scratch);
    if (size) {
      size_t capacity = 4*size + 1;
      char buf[capacity];
      pn_quote_data(buf, capacity, payload, size);
      fprintf(stderr, " (%zu) \"%s\"\n", size, buf);
    } else {
      fprintf(stderr, "\n");
    }
  }
}

ssize_t pn_dispatcher_input(pn_dispatcher_t *disp, char *bytes, size_t available)
{
  size_t read = 0;
  while (true) {
    pn_frame_t frame;
    size_t n = pn_read_frame(&frame, bytes + read, available);
    if (n) {
      pn_value_t performative;
      ssize_t e = pn_decode(&performative, frame.payload, frame.size);
      if (e < 0) {
        fprintf(stderr, "Error decoding frame: %zi\n", e);
        pn_format(disp->scratch, SCRATCH, pn_value("z", frame.size, frame.payload));
        fprintf(stderr, "%s\n", disp->scratch);
        return e;
      }

      disp->channel = frame.channel;
      // XXX: assuming numeric
      uint8_t code = pn_to_uint8(pn_tag_descriptor(pn_to_tag(performative)));
      disp->code = code;
      disp->args = pn_to_list(pn_tag_value(pn_to_tag(performative)));
      disp->size = frame.size - e;
      if (disp->size)
        disp->payload = frame.payload + e;

      pn_do_trace(disp, disp->channel, IN, code, disp->args, disp->payload, disp->size);

      pn_action_t *action = disp->actions[code];
      action(disp);

      disp->channel = 0;
      disp->code = 0;
      disp->args = NULL;
      disp->size = 0;
      disp->payload = NULL;
      pn_visit(performative, pn_free_value);

      available -= n;
      read += n;
    } else {
      break;
    }
  }

  return read;
}

void pn_init_frame(pn_dispatcher_t *disp)
{
  pn_list_clear(disp->output_args);
  disp->output_payload = NULL;
  disp->output_size = 0;
}

void pn_field(pn_dispatcher_t *disp, int index, pn_value_t arg)
{
  int n = pn_list_size(disp->output_args);
  if (index >= n)
    pn_list_fill(disp->output_args, EMPTY_VALUE, index - n + 1);
  pn_list_set(disp->output_args, index, arg);
}

void pn_append_payload(pn_dispatcher_t *disp, const char *data, size_t size)
{
  disp->output_payload = data;
  disp->output_size = size;
}

void pn_post_frame(pn_dispatcher_t *disp, uint16_t ch, uint32_t performative)
{
  pn_tag_t tag = { .descriptor = pn_ulong(performative),
                   .value = pn_from_list(disp->output_args) };
  pn_do_trace(disp, ch, OUT, performative, disp->output_args, disp->output_payload,
              disp->output_size);
  pn_frame_t frame = {disp->frame_type};
  char bytes[pn_encode_sizeof(pn_from_tag(&tag)) + disp->output_size];
  size_t size = pn_encode(pn_from_tag(&tag), bytes);
  for (int i = 0; i < pn_list_size(disp->output_args); i++)
    pn_visit(pn_list_get(disp->output_args, i), pn_free_value);
  if (disp->output_size) {
    memmove(bytes + size, disp->output_payload, disp->output_size);
    size += disp->output_size;
    disp->output_payload = NULL;
    disp->output_size = 0;
  }
  frame.channel = ch;
  frame.payload = bytes;
  frame.size = size;
  size_t n;
  while (!(n = pn_write_frame(disp->output + disp->available,
                              disp->capacity - disp->available, frame))) {
    disp->capacity *= 2;
    disp->output = realloc(disp->output, disp->capacity);
  }
  if (disp->trace & PN_TRACE_RAW) {
    fprintf(stderr, "RAW: \"");
    pn_fprint_data(stderr, disp->output + disp->available, n);
    fprintf(stderr, "\"\n");
  }
  disp->available += n;
}

ssize_t pn_dispatcher_output(pn_dispatcher_t *disp, char *bytes, size_t size)
{
  int n = disp->available < size ? disp->available : size;
  memmove(bytes, disp->output, n);
  memmove(disp->output, disp->output + n, disp->available - n);
  disp->available -= n;
  // XXX: need to check for errors
  return n;
}
