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
  disp->args = pn_dbuf(16);
  disp->payload = NULL;
  disp->size = 0;

  disp->output_args = pn_dbuf(16);
  // XXX
  disp->capacity = 4*1024;
  disp->output = malloc(disp->capacity);
  disp->available = 0;

  disp->halt = false;
  disp->batch = true;

  return disp;
}

void pn_dispatcher_destroy(pn_dispatcher_t *disp)
{
  if (disp) {
    pn_dbuf_free(disp->args);
    pn_dbuf_free(disp->output_args);
    free(disp->output);
    free(disp);
  }
}

void pn_dispatcher_action(pn_dispatcher_t *disp, uint8_t code, const char *name,
                          pn_action_t *action)
{
  disp->actions[code] = action;
  disp->names[code] = name;
}

typedef enum {IN, OUT} pn_dir_t;

static void pn_do_trace(pn_dispatcher_t *disp, uint16_t ch, pn_dir_t dir,
                        pn_dbuf_t *args, const char *payload, size_t size)
{
  if (disp->trace & PN_TRACE_FRM) {
    uint64_t code64;
    bool scanned;
    pn_dbuf_scan(args, "D?L.", &scanned, &code64);
    uint8_t code = scanned ? code64 : 0;
    size_t n = SCRATCH;
    pn_dbuf_format(args, disp->scratch, &n);
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
  while (!disp->halt) {
    pn_frame_t frame;
    size_t n = pn_read_frame(&frame, bytes + read, available);
    if (n) {
      size_t dsize = frame.size;
      int e = pn_dbuf_decode(disp->args, frame.payload, &dsize);
      if (e) {
        fprintf(stderr, "Error decoding frame: %s\n", pn_error(e));
        pn_fprint_data(stderr, frame.payload, frame.size);
        fprintf(stderr, "\n");
        return e;
      }

      disp->channel = frame.channel;
      // XXX: assuming numeric
      uint64_t lcode;
      bool scanned;
      e = pn_dbuf_scan(disp->args, "D?L.", &scanned, &lcode);
      if (e) return e;
      if (!scanned) {
        fprintf(stderr, "Error dispatching frame\n");
        return PN_ERR;
      }
      uint8_t code = lcode;
      disp->code = code;
      disp->size = frame.size - dsize;
      if (disp->size)
        disp->payload = frame.payload + dsize;

      pn_do_trace(disp, disp->channel, IN, disp->args, disp->payload, disp->size);

      pn_action_t *action = disp->actions[code];
      int err = action(disp);

      disp->channel = 0;
      disp->code = 0;
      pn_dbuf_clear(disp->args);
      disp->size = 0;
      disp->payload = NULL;

      if (err) return err;

      available -= n;
      read += n;

      if (!disp->batch) break;
    } else {
      break;
    }
  }

  return read;
}

int pn_scan_args(pn_dispatcher_t *disp, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_dbuf_vscan(disp->args, fmt, ap);
  va_end(ap);
  return err;
}

void pn_set_payload(pn_dispatcher_t *disp, const char *data, size_t size)
{
  disp->output_payload = data;
  disp->output_size = size;
}

int pn_post_frame(pn_dispatcher_t *disp, uint16_t ch, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pn_dbuf_clear(disp->output_args);
  int err = pn_dbuf_vfill(disp->output_args, fmt, ap);
  va_end(ap);
  if (err) {
    fprintf(stderr, "error posting frame: %s, %s\n", fmt, pn_error(err));
    return PN_ERR;
  }

  pn_do_trace(disp, ch, OUT, disp->output_args, disp->output_payload, disp->output_size);

  size_t size = 1024;

  while (true) {
    char buf[size];
    size_t wr = size;
    err = pn_dbuf_encode(disp->output_args, buf, &wr);
    if (err)
    {
      if (err == PN_OVERFLOW) {
        size *= 2;
        continue;
      } else {
        fprintf(stderr, "error posting frame: %s", pn_error(err));
        return PN_ERR;
      }
    } else if (size - wr < disp->output_size) {
      size += wr + disp->output_size - size;
      continue;
    } else {
      if (disp->output_size) {
        memmove(buf + wr, disp->output_payload, disp->output_size);
        wr += disp->output_size;
        disp->output_payload = NULL;
        disp->output_size = 0;
      }

      pn_frame_t frame = {disp->frame_type};
      frame.channel = ch;
      frame.payload = buf;
      frame.size = wr;
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
      break;
    }
  }

  return 0;
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
