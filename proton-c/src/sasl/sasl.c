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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <proton/error.h>
#include <proton/sasl.h>

#include "buffer.h"
#include "protocol.h"
#include "dispatch_actions.h"
#include "framing/framing.h"
#include "engine/engine-internal.h"
#include "dispatcher/dispatcher.h"
#include "util.h"
#include "transport/autodetect.h"


struct pni_sasl_t {
  char *mechanisms;
  char *remote_mechanisms;
  pn_buffer_t *send_data;
  pn_buffer_t *recv_data;
  pn_sasl_outcome_t outcome;
  bool client;
  bool allow_skip;
  bool sent_init;
  bool rcvd_init;
  bool sent_done;
  bool rcvd_done;
  bool halt;
  bool input_bypass;
  bool output_bypass;
};

static inline pn_transport_t *get_transport_internal(pn_sasl_t *sasl)
{
    // The external pn_sasl_t is really a pointer to the internal pni_transport_t
    return ((pn_transport_t *)sasl);
}

static inline pni_sasl_t *get_sasl_internal(pn_sasl_t *sasl)
{
    // The external pn_sasl_t is really a pointer to the internal pni_transport_t
    return sasl ? ((pn_transport_t *)sasl)->sasl : NULL;
}

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available);
static ssize_t pn_input_read_sasl(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_output_write_sasl_header(pn_transport_t* transport, unsigned int layer, char* bytes, size_t size);
static ssize_t pn_output_write_sasl(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);

const pn_io_layer_t sasl_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl_header,
    NULL,
    NULL
};

const pn_io_layer_t sasl_write_header_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl_header,
    NULL,
    NULL
};

const pn_io_layer_t sasl_read_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl,
    NULL,
    NULL
};

static void pni_emit(pn_sasl_t *sasl) {
  pn_transport_t *transport = get_transport_internal(sasl);
  if (transport->connection && transport->connection->collector) {
    pn_collector_t *collector = transport->connection->collector;
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT);
  }
}

pn_sasl_t *pn_sasl(pn_transport_t *transport)
{
  if (!transport->sasl) {
    pni_sasl_t *sasl = (pni_sasl_t *) malloc(sizeof(pni_sasl_t));

    sasl->client = !transport->server;
    sasl->mechanisms = NULL;
    sasl->remote_mechanisms = NULL;
    sasl->send_data = pn_buffer(16);
    sasl->recv_data = pn_buffer(16);
    sasl->outcome = PN_SASL_NONE;
    sasl->allow_skip = false;
    sasl->sent_init = false;
    sasl->rcvd_init = false;
    sasl->sent_done = false;
    sasl->rcvd_done = false;
    sasl->input_bypass = false;
    sasl->output_bypass = false;
    sasl->halt = false;

    transport->sasl = sasl;
  }

  // The actual external pn_sasl_t pointer is a pointer to its enclosing pn_transport_t
  return (pn_sasl_t *)transport;
}

pn_sasl_state_t pn_sasl_state(pn_sasl_t *sasl0)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl) {
    if (sasl->outcome == PN_SASL_NONE) {
      return sasl->rcvd_init ? PN_SASL_STEP : PN_SASL_IDLE;
    } else {
      return sasl->outcome == PN_SASL_OK ? PN_SASL_PASS : PN_SASL_FAIL;
    }
    //    if (sasl->rcvd_init && sasl->outcome == PN_SASL_NONE) return PN_SASL_STEP;
    //if (sasl->outcome == PN_SASL_OK) return PN_SASL_PASS;
    //else return PN_SASL_FAIL;
  } else {
    return PN_SASL_FAIL;
  }
}

void pn_sasl_mechanisms(pn_sasl_t *sasl0, const char *mechanisms)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (!sasl) return;
  sasl->mechanisms = pn_strdup(mechanisms);
  pni_emit(sasl0);
}

const char *pn_sasl_remote_mechanisms(pn_sasl_t *sasl0)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  return sasl ? sasl->remote_mechanisms : NULL;
}

ssize_t pn_sasl_send(pn_sasl_t *sasl0, const char *bytes, size_t size)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl) {
    if (pn_buffer_size(sasl->send_data)) {
      // XXX: need better error
      return PN_STATE_ERR;
    }
    int err = pn_buffer_append(sasl->send_data, bytes, size);
    if (err) return err;
    pni_emit(sasl0);
    return size;
  } else {
    return PN_ARG_ERR;
  }
}

size_t pn_sasl_pending(pn_sasl_t *sasl0)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl && pn_buffer_size(sasl->recv_data)) {
    return pn_buffer_size(sasl->recv_data);
  } else {
    return 0;
  }
}

ssize_t pn_sasl_recv(pn_sasl_t *sasl0, char *bytes, size_t size)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (!sasl) return PN_ARG_ERR;

  size_t bsize = pn_buffer_size(sasl->recv_data);
  if (bsize) {
    if (bsize > size) return PN_OVERFLOW;
    pn_buffer_get(sasl->recv_data, 0, bsize, bytes);
    pn_buffer_clear(sasl->recv_data);
    return bsize;
  } else {
    return PN_EOS;
  }
}

void pn_sasl_client(pn_sasl_t *sasl)
{
}

void pn_sasl_server(pn_sasl_t *sasl0)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl) {
    sasl->client = false;
  }
}

void pn_sasl_allow_skip(pn_sasl_t *sasl0, bool allow)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl)
    sasl->allow_skip = allow;
}

bool pn_sasl_skipping_allowed(pn_transport_t *transport)
{
  return transport && transport->sasl && transport->sasl->allow_skip;
}

void pn_sasl_plain(pn_sasl_t *sasl0, const char *username, const char *password)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (!sasl) return;

  const char *user = username ? username : "";
  const char *pass = password ? password : "";
  size_t usize = strlen(user);
  size_t psize = strlen(pass);
  size_t size = usize + psize + 2;
  char *iresp = (char *) malloc(size);

  iresp[0] = 0;
  memmove(iresp + 1, user, usize);
  iresp[usize + 1] = 0;
  memmove(iresp + usize + 2, pass, psize);

  pn_sasl_mechanisms(sasl0, "PLAIN");
  pn_sasl_send(sasl0, iresp, size);
  free(iresp);
}

void pn_sasl_done(pn_sasl_t *sasl0, pn_sasl_outcome_t outcome)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl) {
    sasl->outcome = outcome;
    // If we do this on the client it is a hack to tell us that
    // no actual negatiation is going to happen and we can go
    // straight to the AMQP layer
    if (sasl->client) {
      sasl->rcvd_done = true;
      sasl->sent_done = true;
    }
    pni_emit(sasl0);
  }
}

pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl0)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  return sasl ? sasl->outcome : PN_SASL_NONE;
}

void pn_sasl_free(pn_transport_t *transport)
{
  if (transport) {
    pni_sasl_t *sasl = transport->sasl;
    if (sasl) {
      free(sasl->mechanisms);
      free(sasl->remote_mechanisms);
      pn_buffer_free(sasl->send_data);
      pn_buffer_free(sasl->recv_data);
      free(sasl);
    }
  }
}

void pn_client_init(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_buffer_memory_t bytes = pn_buffer_memory(sasl->send_data);
  pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[sz]", SASL_INIT, sasl->mechanisms,
                bytes.size, bytes.start);
  pn_buffer_clear(sasl->send_data);
  pni_emit((pn_sasl_t *) transport);
}

void pn_server_init(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  // XXX
  char *mechs[16];
  int count = 0;

  if (sasl->mechanisms) {
    char *start = sasl->mechanisms;
    char *end = start;

    while (*end) {
      if (*end == ' ') {
        if (start != end) {
          *end = '\0';
          mechs[count++] = start;
        }
        end++;
        start = end;
      } else {
        end++;
      }
    }

    if (start != end) {
      mechs[count++] = start;
    }
  }

  pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[@T[*s]]", SASL_MECHANISMS, PN_SYMBOL, count, mechs);
  pni_emit((pn_sasl_t *) transport);
}

void pn_server_done(pn_sasl_t *sasl0)
{
  pn_transport_t *transport = get_transport_internal(sasl0);
  pni_sasl_t *sasl = transport->sasl;
  pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[B]", SASL_OUTCOME, sasl->outcome);
  pni_emit(sasl0);
}

void pn_sasl_process(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  if (!sasl->sent_init) {
    if (sasl->client) {
      pn_client_init(transport);
    } else {
      pn_server_init(transport);
    }
    sasl->sent_init = true;
  }

  if (pn_buffer_size(sasl->send_data)) {
    pn_buffer_memory_t bytes = pn_buffer_memory(sasl->send_data);
    pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[z]", sasl->client ? SASL_RESPONSE : SASL_CHALLENGE,
                  bytes.size, bytes.start);
    pn_buffer_clear(sasl->send_data);
    pni_emit((pn_sasl_t *) transport);
  }

  if (!sasl->client && sasl->outcome != PN_SASL_NONE && !sasl->sent_done) {
    pn_server_done((pn_sasl_t *)transport);
    sasl->sent_done = true;
  }

  // XXX: need to finish this check when challenge/response is complete
  //      check for client is outome is received
  //      check for server is that there are no pending frames (either init
  //      or challenges) from client
  if (!sasl->client && sasl->sent_done && sasl->rcvd_init) {
    sasl->rcvd_done = true;
    sasl->halt = true;
  }
}

ssize_t pn_sasl_input(pn_transport_t *transport, const char *bytes, size_t available)
{
  pni_sasl_t *sasl = transport->sasl;
  ssize_t n = pn_dispatcher_input(transport, bytes, available, false, &sasl->halt);
  if (n < 0) return n;

  pn_sasl_process(transport);

  if (sasl->rcvd_done) {
    if (pn_sasl_state((pn_sasl_t *)transport) == PN_SASL_PASS) {
      if (n) {
        return n;
      } else {
        return PN_EOS;
      }
    } else {
      // XXX: should probably do something better here
      return PN_ERR;
    }
  } else {
    return n;
  }
}

ssize_t pn_sasl_output(pn_transport_t *transport, char *bytes, size_t size)
{
  pn_sasl_process(transport);

  pni_sasl_t *sasl = transport->sasl;
  if (transport->available == 0 && sasl->sent_done) {
    if (pn_sasl_state((pn_sasl_t *)transport) == PN_SASL_PASS) {
      return PN_EOS;
    } else {
      // XXX: should probably do something better here
      return PN_ERR;
    }
  } else {
    return pn_dispatcher_output(transport, bytes, size);
  }
}

int pn_do_init(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_bytes_t mech;
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[sz]", &mech, &recv);
  if (err) return err;
  sasl->remote_mechanisms = pn_strndup(mech.start, mech.size);
  pn_buffer_append(sasl->recv_data, recv.start, recv.size);
  sasl->rcvd_init = true;
  return 0;
}

int pn_do_mechanisms(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->rcvd_init = true;
  return 0;
}

int pn_do_recv(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[z]", &recv);
  if (err) return err;
  pn_buffer_append(sasl->recv_data, recv.start, recv.size);
  return 0;
}

int pn_do_challenge(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  return pn_do_recv(transport, frame_type, channel, args, payload);
}

int pn_do_response(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  return pn_do_recv(transport, frame_type, channel, args, payload);
}

int pn_do_outcome(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  uint8_t outcome;
  int err = pn_data_scan(args, "D.[B]", &outcome);
  if (err) return err;
  sasl->outcome = (pn_sasl_outcome_t) outcome;
  sasl->rcvd_done = true;
  sasl->sent_done = true;
  sasl->halt = true;
  pni_emit((pn_sasl_t *) transport);
  return 0;
}

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")
#define SASL_HEADER_LEN 8

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  bool eos = pn_transport_capacity(transport)==PN_EOS;
  pni_protocol_type_t protocol = pni_sniff_header(bytes, available);
  switch (protocol) {
  case PNI_PROTOCOL_AMQP_SASL:
    if (transport->io_layers[layer] == &sasl_read_header_layer) {
        transport->io_layers[layer] = &sasl_layer;
    } else {
        transport->io_layers[layer] = &sasl_write_header_layer;
    }
    if (transport->trace & PN_TRACE_FRM)
        pn_transport_logf(transport, "  <- %s", "SASL");
    return SASL_HEADER_LEN;
  case PNI_PROTOCOL_INSUFFICIENT:
    if (!eos) return 0;
    /* Fallthru */
  default:
    break;
  }
  char quoted[1024];
  pn_quote_data(quoted, 1024, bytes, available);
  pn_do_error(transport, "amqp:connection:framing-error",
              "%s header mismatch: %s ['%s']%s", "SASL", pni_protocol_name(protocol), quoted,
              !eos ? "" : " (connection aborted)");
  return PN_EOS;
}

static ssize_t pn_input_read_sasl(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  pni_sasl_t *sasl = transport->sasl;
  if (!sasl->input_bypass) {
    ssize_t n = pn_sasl_input(transport, bytes, available);
    if (n != PN_EOS) return n;

    sasl->input_bypass = true;
    if (sasl->output_bypass)
        transport->io_layers[layer] = &pni_passthru_layer;
  }
  return pni_passthru_layer.process_input(transport, layer, bytes, available );
}

static ssize_t pn_output_write_sasl_header(pn_transport_t *transport, unsigned int layer, char *bytes, size_t size)
{
  if (transport->trace & PN_TRACE_FRM)
    pn_transport_logf(transport, "  -> %s", "SASL");
  assert(size >= SASL_HEADER_LEN);
  memmove(bytes, SASL_HEADER, SASL_HEADER_LEN);
  if (transport->io_layers[layer]==&sasl_write_header_layer) {
      transport->io_layers[layer] = &sasl_layer;
  } else {
      transport->io_layers[layer] = &sasl_read_header_layer;
  }
  return SASL_HEADER_LEN;
}

static ssize_t pn_output_write_sasl(pn_transport_t* transport, unsigned int layer, char* bytes, size_t available)
{
  pni_sasl_t *sasl = transport->sasl;
  if (!sasl->output_bypass) {
    // this accounts for when pn_do_error is invoked, e.g. by idle timeout
    ssize_t n;
    if (transport->close_sent) {
        n = PN_EOS;
    } else {
        n = pn_sasl_output(transport, bytes, available);
    }
    if (n != PN_EOS) return n;

    sasl->output_bypass = true;
    if (sasl->input_bypass)
        transport->io_layers[layer] = &pni_passthru_layer;
  }
  return pni_passthru_layer.process_output(transport, layer, bytes, available );
}

