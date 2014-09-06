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
#include <proton/buffer.h>
#include <proton/framing.h>
#include <proton/engine.h> // XXX: just needed for PN_EOS
#include <proton/sasl.h>
#include "protocol.h"
#include "dispatch_actions.h"
#include "../dispatcher/dispatcher.h"
#include "../engine/engine-internal.h"
#include "../util.h"


struct pn_sasl_t {
  pn_transport_t *transport;
  pn_io_layer_t *io_layer;
  pn_dispatcher_t *disp;
  char *mechanisms;
  char *remote_mechanisms;
  pn_buffer_t *send_data;
  pn_buffer_t *recv_data;
  pn_sasl_outcome_t outcome;
  bool client;
  bool configured;
  bool allow_skip;
  bool sent_init;
  bool rcvd_init;
  bool sent_done;
  bool rcvd_done;
};

static ssize_t pn_input_read_sasl_header(pn_io_layer_t *io_layer, const char *bytes, size_t available);
static ssize_t pn_input_read_sasl(pn_io_layer_t *io_layer, const char *bytes, size_t available);
static ssize_t pn_output_write_sasl_header(pn_io_layer_t *io_layer, char *bytes, size_t available);
static ssize_t pn_output_write_sasl(pn_io_layer_t *io_layer, char *bytes, size_t available);

pn_sasl_t *pn_sasl(pn_transport_t *transport)
{
  if (!transport->sasl) {
    pn_sasl_t *sasl = (pn_sasl_t *) malloc(sizeof(pn_sasl_t));
    sasl->disp = pn_dispatcher(1, transport);
    sasl->disp->batch = false;

    sasl->client = false;
    sasl->configured = false;
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

    transport->sasl = sasl;
    sasl->transport = transport;
    sasl->io_layer = &transport->io_layers[PN_IO_SASL];
    sasl->io_layer->context = sasl;
    sasl->io_layer->process_input = pn_input_read_sasl_header;
    sasl->io_layer->process_output = pn_output_write_sasl_header;
    sasl->io_layer->process_tick = pn_io_layer_tick_passthru;
  }

  return transport->sasl;
}

pn_sasl_state_t pn_sasl_state(pn_sasl_t *sasl)
{
  if (sasl) {
    if (!sasl->configured) return PN_SASL_CONF;
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

void pn_sasl_mechanisms(pn_sasl_t *sasl, const char *mechanisms)
{
  if (!sasl) return;
  sasl->mechanisms = pn_strdup(mechanisms);
}

const char *pn_sasl_remote_mechanisms(pn_sasl_t *sasl)
{
  return sasl ? sasl->remote_mechanisms : NULL;
}

ssize_t pn_sasl_send(pn_sasl_t *sasl, const char *bytes, size_t size)
{
  if (sasl) {
    if (pn_buffer_size(sasl->send_data)) {
      // XXX: need better error
      return PN_STATE_ERR;
    }
    int err = pn_buffer_append(sasl->send_data, bytes, size);
    if (err) return err;
    return size;
  } else {
    return PN_ARG_ERR;
  }
}

size_t pn_sasl_pending(pn_sasl_t *sasl)
{
  if (sasl && pn_buffer_size(sasl->recv_data)) {
    return pn_buffer_size(sasl->recv_data);
  } else {
    return 0;
  }
}

ssize_t pn_sasl_recv(pn_sasl_t *sasl, char *bytes, size_t size)
{
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
  if (sasl) {
    sasl->client = true;
    sasl->configured = true;
  }
}

void pn_sasl_server(pn_sasl_t *sasl)
{
  if (sasl) {
    sasl->client = false;
    sasl->configured = true;
  }
}

void pn_sasl_allow_skip(pn_sasl_t *sasl, bool allow)
{
  if (sasl)
    sasl->allow_skip = allow;
}

void pn_sasl_plain(pn_sasl_t *sasl, const char *username, const char *password)
{
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

  pn_sasl_mechanisms(sasl, "PLAIN");
  pn_sasl_send(sasl, iresp, size);
  pn_sasl_client(sasl);
  free(iresp);
}

void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome)
{
  if (sasl) {
    sasl->outcome = outcome;
  }
}

pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl)
{
  return sasl ? sasl->outcome : PN_SASL_NONE;
}

void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace)
{
  sasl->disp->trace = trace;
}

void pn_sasl_free(pn_sasl_t *sasl)
{
  if (sasl) {
    free(sasl->mechanisms);
    free(sasl->remote_mechanisms);
    pn_buffer_free(sasl->send_data);
    pn_buffer_free(sasl->recv_data);
    pn_dispatcher_free(sasl->disp);
    free(sasl);
  }
}

void pn_client_init(pn_sasl_t *sasl)
{
  pn_buffer_memory_t bytes = pn_buffer_memory(sasl->send_data);
  pn_post_frame(sasl->disp, 0, "DL[sz]", SASL_INIT, sasl->mechanisms,
                bytes.size, bytes.start);
  pn_buffer_clear(sasl->send_data);
}

void pn_server_init(pn_sasl_t *sasl)
{
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

  pn_post_frame(sasl->disp, 0, "DL[@T[*s]]", SASL_MECHANISMS, PN_SYMBOL, count, mechs);
}

void pn_server_done(pn_sasl_t *sasl)
{
  pn_post_frame(sasl->disp, 0, "DL[B]", SASL_OUTCOME, sasl->outcome);
}

void pn_sasl_process(pn_sasl_t *sasl)
{
  if (!sasl->configured) return;

  if (!sasl->sent_init) {
    if (sasl->client) {
      pn_client_init(sasl);
    } else {
      pn_server_init(sasl);
    }
    sasl->sent_init = true;
  }

  if (pn_buffer_size(sasl->send_data)) {
    pn_buffer_memory_t bytes = pn_buffer_memory(sasl->send_data);
    pn_post_frame(sasl->disp, 0, "DL[z]", sasl->client ? SASL_RESPONSE : SASL_CHALLENGE,
                  bytes.size, bytes.start);
    pn_buffer_clear(sasl->send_data);
  }

  if (!sasl->client && sasl->outcome != PN_SASL_NONE && !sasl->sent_done) {
    pn_server_done(sasl);
    sasl->sent_done = true;
  }

  // XXX: need to finish this check when challenge/response is complete
  //      check for client is outome is received
  //      check for server is that there are no pending frames (either init
  //      or challenges) from client
  if (!sasl->client && sasl->sent_done && sasl->rcvd_init) {
    sasl->rcvd_done = true;
    sasl->disp->halt = true;
  }
}

ssize_t pn_sasl_input(pn_sasl_t *sasl, const char *bytes, size_t available)
{
  ssize_t n = pn_dispatcher_input(sasl->disp, bytes, available);
  if (n < 0) return n;

  pn_sasl_process(sasl);

  if (sasl->rcvd_done) {
    if (pn_sasl_state(sasl) == PN_SASL_PASS) {
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

ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size)
{
  pn_sasl_process(sasl);

  if (sasl->disp->available == 0 && sasl->sent_done) {
    if (pn_sasl_state(sasl) == PN_SASL_PASS) {
      return PN_EOS;
    } else {
      // XXX: should probably do something better here
      return PN_ERR;
    }
  } else {
    return pn_dispatcher_output(sasl->disp, bytes, size);
  }
}

int pn_do_init(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->transport->sasl;
  pn_bytes_t mech;
  pn_bytes_t recv;
  int err = pn_scan_args(disp, "D.[sz]", &mech, &recv);
  if (err) return err;
  sasl->remote_mechanisms = pn_strndup(mech.start, mech.size);
  pn_buffer_append(sasl->recv_data, recv.start, recv.size);
  sasl->rcvd_init = true;
  return 0;
}

int pn_do_mechanisms(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->transport->sasl;
  sasl->rcvd_init = true;
  return 0;
}

int pn_do_recv(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->transport->sasl;
  pn_bytes_t recv;
  int err = pn_scan_args(disp, "D.[z]", &recv);
  if (err) return err;
  pn_buffer_append(sasl->recv_data, recv.start, recv.size);
  return 0;
}

int pn_do_challenge(pn_dispatcher_t *disp)
{
  return pn_do_recv(disp);
}

int pn_do_response(pn_dispatcher_t *disp)
{
  return pn_do_recv(disp);
}

int pn_do_outcome(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->transport->sasl;
  uint8_t outcome;
  int err = pn_scan_args(disp, "D.[B]", &outcome);
  if (err) return err;
  sasl->outcome = (pn_sasl_outcome_t) outcome;
  sasl->rcvd_done = true;
  sasl->sent_done = true;
  disp->halt = true;
  return 0;
}

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")
#define AMQP_HEADER ("AMQP\x00\x01\x00\x00")
#define SASL_HEADER_LEN 8

static ssize_t pn_input_read_sasl_header(pn_io_layer_t *io_layer, const char *bytes, size_t available)
{
  pn_sasl_t *sasl = (pn_sasl_t *)io_layer->context;
  if (available > 0) {
    if (available < SASL_HEADER_LEN) {
      if (memcmp(bytes, SASL_HEADER, available) == 0 ||
          memcmp(bytes, AMQP_HEADER, available) == 0)
        return 0;
    } else {
      if (memcmp(bytes, SASL_HEADER, SASL_HEADER_LEN) == 0) {
        sasl->io_layer->process_input = pn_input_read_sasl;
        if (sasl->disp->trace & PN_TRACE_FRM)
          pn_transport_logf(sasl->transport, "  <- %s", "SASL");
        return SASL_HEADER_LEN;
      }
      if (memcmp(bytes, AMQP_HEADER, SASL_HEADER_LEN) == 0) {
        if (sasl->allow_skip) {
          sasl->outcome = PN_SASL_SKIPPED;
          sasl->io_layer->process_input = pn_io_layer_input_passthru;
          sasl->io_layer->process_output = pn_io_layer_output_passthru;
          pn_io_layer_t *io_next = sasl->io_layer->next;
          return io_next->process_input( io_next, bytes, available );
        } else {
            pn_do_error(sasl->transport, "amqp:connection:policy-error",
                        "Client skipped SASL exchange - forbidden");
            return PN_EOS;
        }
      }
    }
  }
  char quoted[1024];
  pn_quote_data(quoted, 1024, bytes, available);
  pn_do_error(sasl->transport, "amqp:connection:framing-error",
              "%s header mismatch: '%s'", "SASL", quoted);
  return PN_EOS;
}

static ssize_t pn_input_read_sasl(pn_io_layer_t *io_layer, const char *bytes, size_t available)
{
  pn_sasl_t *sasl = (pn_sasl_t *)io_layer->context;
  ssize_t n = pn_sasl_input(sasl, bytes, available);
  if (n == PN_EOS) {
    sasl->io_layer->process_input = pn_io_layer_input_passthru;
    pn_io_layer_t *io_next = sasl->io_layer->next;
    return io_next->process_input( io_next, bytes, available );
  }
  return n;
}

static ssize_t pn_output_write_sasl_header(pn_io_layer_t *io_layer, char *bytes, size_t size)
{
  pn_sasl_t *sasl = (pn_sasl_t *)io_layer->context;
  if (sasl->disp->trace & PN_TRACE_FRM)
    pn_transport_logf(sasl->transport, "  -> %s", "SASL");
  assert(size >= SASL_HEADER_LEN);
  memmove(bytes, SASL_HEADER, SASL_HEADER_LEN);
  sasl->io_layer->process_output = pn_output_write_sasl;
  return SASL_HEADER_LEN;
}

static ssize_t pn_output_write_sasl(pn_io_layer_t *io_layer, char *bytes, size_t size)
{
  pn_sasl_t *sasl = (pn_sasl_t *)io_layer->context;
  ssize_t n = pn_sasl_output(sasl, bytes, size);
  if (n == PN_EOS) {
    sasl->io_layer->process_output = pn_io_layer_output_passthru;
    pn_io_layer_t *io_next = sasl->io_layer->next;
    return io_next->process_output( io_next, bytes, size );
  }
  return n;
}

