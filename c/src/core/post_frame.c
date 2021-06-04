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

#include "proton/codec.h"
#include "proton/logger.h"
#include "proton/object.h"
#include "proton/type_compat.h"

#include "buffer.h"
#include "engine-internal.h"
#include "framing.h"
#include "dispatch_actions.h"

static inline struct out {int err; pn_bytes_t bytes;} pn_vfill_performative(pn_buffer_t *frame_buf, pn_data_t *output_args, const char *fmt, va_list ap)
{
  pn_data_clear(output_args);
  int err = pn_data_vfill(output_args, fmt, ap);
  if (err) {
    return (struct out){err, pn_bytes_null};
  }

encode_performatives:
  pn_buffer_clear( frame_buf );
  pn_rwbytes_t buf = pn_buffer_memory( frame_buf );
  buf.size = pn_buffer_available( frame_buf );

  ssize_t wr = pn_data_encode( output_args, buf.start, buf.size );
  if (wr < 0) {
    if (wr == PN_OVERFLOW) {
      pn_buffer_ensure( frame_buf, pn_buffer_available( frame_buf ) * 2 );
      goto encode_performatives;
    }
    return (struct out){wr, pn_bytes_null};
  }
  return (struct out){0, {.size = wr, .start = buf.start}};
}

static inline void pn_log_fill_error(pn_logger_t *logger, pn_data_t *data, int error, const char *fmt) {
  if (pn_error_code(pn_data_error(data))) {
    pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR,
                   "error posting frame: %s, %s: %s", fmt, pn_code(error),
                   pn_error_text(pn_data_error(data)));
  } else {
    pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR,
                   "error posting frame: %s", pn_code(error));
  }
}

pn_bytes_t pn_fill_performative(pn_transport_t *transport, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  struct out out = pn_vfill_performative(transport->frame, transport->output_args, fmt, ap);
  va_end(ap);
  if (out.err){
    pn_log_fill_error(&transport->logger, transport->output_args, out.err, fmt);
  }
  return out.bytes;
}

void pn_do_tx_trace(pn_logger_t *logger, uint16_t ch, pn_data_t *args)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ) {
    if (pn_data_size(args)==0) {
      pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, "%u -> (EMPTY FRAME)", ch);
    } else {
      pni_logger_log_msg_inspect(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, args, "%u -> ", ch);
    }
  }
}

void pn_do_rx_trace(pn_logger_t *logger, uint16_t ch, pn_data_t *args)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ) {
    if (pn_data_size(args)==0) {
      pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, "%u <- (EMPTY FRAME)", ch);
    } else {
      pni_logger_log_msg_inspect(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, args, "%u <- ", ch);
    }
  }
}

void pn_do_trace_payload(pn_logger_t *logger, pn_bytes_t payload)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ) {
    char buf[1100];
    if (payload.size) {
      size_t l = snprintf(buf, 21, " (%zu) ", payload.size);
      ssize_t e = pn_quote_data(buf+l, 1024, payload.start, payload.size);
      if (e == PN_OVERFLOW) {
        size_t s = strlen(buf);
        strcpy(buf+s, "... (truncated)");
      }
    }
    pni_logger_log(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, buf);
  }
}

void inline pn_do_raw_trace(pn_logger_t *logger, pn_buffer_t *output, size_t size)
{
  PN_LOG_RAW(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW, output, size);
}

static inline void pn_post_frame(pn_buffer_t *output, uint8_t type, uint16_t ch, pn_bytes_t frame_payload)
{
  pn_frame_t frame = {
    .type = type,
    .channel = ch,
    .payload = frame_payload.start,
    .size = frame_payload.size
  };
  pn_buffer_ensure(output, AMQP_HEADER_SIZE+frame.ex_size+frame.size);
  pn_write_frame(output, frame);
}

int pn_post_amqp_frame(pn_transport_t *transport, uint16_t ch, pn_bytes_t performative)
{
  if (!performative.start)
    return PN_ERR;

  pn_do_tx_trace(&transport->logger, ch, transport->output_args);

  pn_post_frame(transport->output_buffer, AMQP_FRAME_TYPE, ch, performative);
  pn_do_raw_trace(&transport->logger, transport->output_buffer, AMQP_HEADER_SIZE+performative.size);
  transport->output_frames_ct += 1;
  return 0;
}

int pn_post_amqp_payload_frame(pn_transport_t *transport, uint16_t ch, pn_bytes_t performative, pn_bytes_t payload)
{
  if (!performative.start)
    return PN_ERR;

  pn_do_tx_trace(&transport->logger, ch, transport->output_args);
  pn_do_trace_payload(&transport->logger, payload);

  // For the moment this is a nasty hack - we know that performative is in transport->frame
  pn_buffer_t *frame = transport->frame;
  if (pn_buffer_available( frame ) < (payload.size + performative.size)) {
    // not enough room for payload - resize...
    // - importantly the performative is still at the front
    pn_buffer_ensure( frame, payload.size + performative.size );
  }
  pn_rwbytes_t wbuf = pn_buffer_memory(frame);
  wbuf.size = pn_buffer_available(frame);
  memcpy( wbuf.start + performative.size, payload.start, payload.size);

  pn_post_frame(transport->output_buffer, AMQP_FRAME_TYPE, ch,
                (pn_bytes_t){.size = performative.size + payload.size, .start = wbuf.start});
  pn_do_raw_trace(&transport->logger, transport->output_buffer,
                  AMQP_HEADER_SIZE+performative.size+payload.size);
  transport->output_frames_ct += 1;
  return 0;
}

int pn_post_sasl_frame(pn_transport_t *transport, pn_bytes_t performative)
{
  if (!performative.start)
    return PN_ERR;

  pn_do_tx_trace(&transport->logger, 0, transport->output_args);

  // All SASL frames go on channel 0
  pn_post_frame(transport->output_buffer, SASL_FRAME_TYPE, 0, performative);
  pn_do_raw_trace(&transport->logger, transport->output_buffer, AMQP_HEADER_SIZE+performative.size);
  transport->output_frames_ct += 1;
  return 0;
}
