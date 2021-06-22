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
