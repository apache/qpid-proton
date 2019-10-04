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

#include "dispatcher.h"

#include "engine-internal.h"
#include "framing.h"
#include "logger_private.h"
#include "protocol.h"

#include "dispatch_actions.h"

int pni_bad_frame(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload) {
  PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame: type: %d: Unknown performative", frame_type);
  return PN_ERR;
}

int pni_bad_frame_type(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload) {
  PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame: Unknown frame type: %d", frame_type);
  return PN_ERR;
}

// We could use a table based approach here if we needed to dynamically
// add new performatives
static inline int pni_dispatch_action(pn_transport_t* transport, uint64_t lcode, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_action_t *action;
  switch (frame_type) {
  case AMQP_FRAME_TYPE:
    /* Regular AMQP frames */
    switch (lcode) {
    case OPEN:            action = pn_do_open; break;
    case BEGIN:           action = pn_do_begin; break;
    case ATTACH:          action = pn_do_attach; break;
    case FLOW:            action = pn_do_flow; break;
    case TRANSFER:        action = pn_do_transfer; break;
    case DISPOSITION:     action = pn_do_disposition; break;
    case DETACH:          action = pn_do_detach; break;
    case END:             action = pn_do_end; break;
    case CLOSE:           action = pn_do_close; break;
    default:              action = pni_bad_frame; break;
    };
    break;
  case SASL_FRAME_TYPE:
    /* SASL frames */
    switch (lcode) {
    case SASL_MECHANISMS: action = pn_do_mechanisms; break;
    case SASL_INIT:       action = pn_do_init; break;
    case SASL_CHALLENGE:  action = pn_do_challenge; break;
    case SASL_RESPONSE:   action = pn_do_response; break;
    case SASL_OUTCOME:    action = pn_do_outcome; break;
    default:              action = pni_bad_frame; break;
    };
    break;
  default:              action = pni_bad_frame_type; break;
  };
  return action(transport, frame_type, channel, args, payload);
}

static int pni_dispatch_frame(pn_transport_t * transport, pn_data_t *args, pn_frame_t frame)
{
  if (frame.size == 0) { // ignore null frames
    PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, "%u <- (EMPTY FRAME)", frame.channel);
    return 0;
  }

  ssize_t dsize = pn_data_decode(args, frame.payload, frame.size);
  if (dsize < 0) {
    pn_string_format(transport->scratch,
                     "Error decoding frame: %s %s\n", pn_code(dsize),
                     pn_error_text(pn_data_error(args)));
    pn_quote(transport->scratch, frame.payload, frame.size);
    PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, pn_string_get(transport->scratch));
    return dsize;
  }

  uint8_t frame_type = frame.type;
  uint16_t channel = frame.channel;
  // XXX: assuming numeric -
  // if we get a symbol we should map it to the numeric value and dispatch on that
  uint64_t lcode;
  bool scanned;
  int e = pn_data_scan(args, "D?L.", &scanned, &lcode);
  if (e) {
    PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Scan error");
    return e;
  }
  if (!scanned) {
    PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame");
    return PN_ERR;
  }
  size_t payload_size = frame.size - dsize;
  const char *payload_mem = payload_size ? frame.payload + dsize : NULL;
  pn_bytes_t payload = {payload_size, payload_mem};

  pn_do_trace(transport, channel, IN, args, payload_mem, payload_size);

  int err = pni_dispatch_action(transport, lcode, frame_type, channel, args, &payload);

  pn_data_clear(args);

  return err;
}

ssize_t pn_dispatcher_input(pn_transport_t *transport, const char *bytes, size_t available, bool batch, bool *halt)
{
  size_t read = 0;

  while (available && !*halt) {
    pn_frame_t frame;

    ssize_t n = pn_read_frame(&frame, bytes + read, available, transport->local_max_frame);
    if (n > 0) {
      read += n;
      available -= n;
      transport->input_frames_ct += 1;
      int e = pni_dispatch_frame(transport, transport->args, frame);
      if (e) return e;
    } else if (n < 0) {
      pn_do_error(transport, "amqp:connection:framing-error", "malformed frame");
      return n;
    } else {
      break;
    }

    if (!batch) break;
  }

  return read;
}

ssize_t pn_dispatcher_output(pn_transport_t *transport, char *bytes, size_t size)
{
    int n = pn_buffer_get(transport->output_buffer, 0, size, bytes);
    pn_buffer_trim(transport->output_buffer, n, 0);
    // XXX: need to check for errors
    return n;
}
