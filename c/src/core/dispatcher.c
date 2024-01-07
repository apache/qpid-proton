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

#include "consumers.h"
#include "dispatch_actions.h"
#include "engine-internal.h"
#include "framing.h"
#include "logger_private.h"
#include "protocol.h"


int pni_bad_frame(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_bytes_t payload) {
  PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame: type: %u: Unknown performative", frame_type);
  return PN_ERR;
}

int pni_bad_frame_type(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_bytes_t payload) {
  PN_LOG(&transport->logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame: Unknown frame type: %u", frame_type);
  return PN_ERR;
}

// We could use a table based approach here if we needed to dynamically
// add new performatives
static inline int pni_dispatch_action(pn_transport_t* transport, uint64_t lcode, uint8_t frame_type, uint16_t channel, pn_bytes_t frame_payload)
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
  return action(transport, frame_type, channel, frame_payload);
}

static int pni_dispatch_frame(pn_frame_t frame, pn_logger_t *logger, pn_transport_t * transport)
{
  pn_bytes_t frame_payload = frame.frame_payload0;

  if (frame_payload.size == 0) { // ignore null frames
    return 0;
  }

  uint64_t lcode;
  pni_consumer_t consumer = make_consumer_from_bytes(frame_payload);
  pni_consumer_t subconsumer;
  if (!consume_described_ulong_descriptor(&consumer, &subconsumer, &lcode)
      || !pni_islist(&subconsumer)
  ) {
    PN_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_ERROR, "Error dispatching frame");
    return PN_ERR;
  }

  uint8_t frame_type = frame.type;
  uint16_t channel = frame.channel;

  int err = pni_dispatch_action(transport, lcode, frame_type, channel, frame_payload);

  return err;
}

ssize_t pn_dispatcher_input(pn_transport_t *transport, const char *bytes, size_t available, bool batch, bool *halt)
{
  size_t read = 0;

  while (available && !*halt) {
    pn_frame_t frame;

    ssize_t n = pn_read_frame(&frame, bytes + read, available, transport->local_max_frame, &transport->logger);
    if (n > 0) {
      read += n;
      available -= n;
      transport->input_frames_ct += 1;
      int e = pni_dispatch_frame(frame, &transport->logger, transport);
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
