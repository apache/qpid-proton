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
#include <proton/value.h>
#include <proton/engine.h> // XXX: just needed for PN_EOS
#include <proton/sasl.h>
#include "protocol.h"
#include "../dispatcher/dispatcher.h"
#include "../util.h"

#define SCRATCH (1024)

struct pn_sasl_t {
  pn_dispatcher_t *disp;
  bool client;
  bool configured;
  char *mechanisms;
  char *remote_mechanisms;
  pn_binary_t *send_data;
  pn_binary_t *recv_data;
  pn_sasl_outcome_t outcome;
  bool sent_init;
  bool rcvd_init;
  bool sent_done;
  bool rcvd_done;
  char scratch[SCRATCH];
};

void pn_do_init(pn_dispatcher_t *disp);
void pn_do_mechanisms(pn_dispatcher_t *disp);
void pn_do_challenge(pn_dispatcher_t *disp);
void pn_do_response(pn_dispatcher_t *disp);
void pn_do_outcome(pn_dispatcher_t *disp);

pn_sasl_t *pn_sasl()
{
  pn_sasl_t *sasl = malloc(sizeof(pn_sasl_t));
  sasl->disp = pn_dispatcher(1, sasl);
  sasl->disp->batch = false;

  pn_dispatcher_action(sasl->disp, SASL_INIT, "SASL-INIT", pn_do_init);
  pn_dispatcher_action(sasl->disp, SASL_MECHANISMS, "SASL-MECHANISMS", pn_do_mechanisms);
  pn_dispatcher_action(sasl->disp, SASL_CHALLENGE, "SASL-CHALLENGE", pn_do_challenge);
  pn_dispatcher_action(sasl->disp, SASL_RESPONSE, "SASL-RESPONSE", pn_do_response);
  pn_dispatcher_action(sasl->disp, SASL_OUTCOME, "SASL-OUTCOME", pn_do_outcome);

  sasl->client = false;
  sasl->configured = false;
  sasl->mechanisms = NULL;
  sasl->remote_mechanisms = NULL;
  sasl->send_data = NULL;
  sasl->recv_data = NULL;
  sasl->outcome = PN_SASL_NONE;
  sasl->sent_init = false;
  sasl->rcvd_init = false;
  sasl->sent_done = false;
  sasl->rcvd_done = false;

  return sasl;
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
  sasl->mechanisms = strdup(mechanisms);
}

const char *pn_sasl_remote_mechanisms(pn_sasl_t *sasl)
{
  return sasl ? sasl->remote_mechanisms : NULL;
}

ssize_t pn_sasl_send(pn_sasl_t *sasl, const char *bytes, size_t size)
{
  if (sasl) {
    if (sasl->send_data) {
      // XXX: need better error
      return PN_STATE_ERR;
    }
    sasl->send_data = pn_binary(bytes, size);
    return size;
  } else {
    return PN_ARG_ERR;
  }
}

size_t pn_sasl_pending(pn_sasl_t *sasl)
{
  if (sasl && sasl->recv_data) {
    return pn_binary_size(sasl->recv_data);
  } else {
    return 0;
  }
}

ssize_t pn_sasl_recv(pn_sasl_t *sasl, char *bytes, size_t size)
{
  if (!sasl) return PN_ARG_ERR;

  if (sasl->recv_data) {
    ssize_t result = pn_binary_get(sasl->recv_data, bytes, size);
    if (result >= 0) {
      pn_free_binary(sasl->recv_data);
      sasl->recv_data = NULL;
    }
    return result;
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

void pn_sasl_plain(pn_sasl_t *sasl, const char *username, const char *password)
{
  if (!sasl) return;

  const char *user = username ? username : "";
  const char *pass = password ? password : "";
  size_t usize = strlen(user);
  size_t psize = strlen(pass);
  size_t size = usize + psize + 2;
  char iresp[size];

  iresp[0] = 0;
  memmove(iresp + 1, user, usize);
  iresp[usize + 1] = 0;
  memmove(iresp + usize + 2, pass, psize);

  pn_sasl_mechanisms(sasl, "PLAIN");
  pn_sasl_send(sasl, iresp, size);
  pn_sasl_client(sasl);
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

void pn_sasl_destroy(pn_sasl_t *sasl)
{
  free(sasl->mechanisms);
  free(sasl->remote_mechanisms);
  pn_free_binary(sasl->send_data);
  pn_free_binary(sasl->recv_data);
  pn_dispatcher_destroy(sasl->disp);
  free(sasl);
}

void pn_client_init(pn_sasl_t *sasl)
{
  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_INIT_MECHANISM, pn_from_symbol(pn_symbol(sasl->mechanisms)));
  if (sasl->send_data) {
    pn_field(sasl->disp, SASL_INIT_INITIAL_RESPONSE, pn_from_binary(sasl->send_data));
    sasl->send_data = NULL;
  }
  pn_post_frame(sasl->disp, 0, SASL_INIT);
}

void pn_server_init(pn_sasl_t *sasl)
{
  // XXX
  pn_array_t *mechs = pn_array(SYMBOL, 16);
  if (sasl->mechanisms) {
    char *start = sasl->mechanisms;
    char *end = start;

    while (*end) {
      if (*end == ' ') {
        if (start != end) {
          *end = '\0';
          pn_array_add(mechs, pn_value("s", start));
        }
        end++;
        start = end;
      } else {
        end++;
      }
    }

    if (start != end) {
      pn_array_add(mechs, pn_value("s", start));
    }
  }

  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_MECHANISMS_SASL_SERVER_MECHANISMS, pn_from_array(mechs));
  pn_post_frame(sasl->disp, 0, SASL_MECHANISMS);
}

void pn_server_done(pn_sasl_t *sasl)
{
  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_OUTCOME_CODE, pn_value("B", sasl->outcome));
  pn_post_frame(sasl->disp, 0, SASL_OUTCOME);
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

  if (!sasl->client && sasl->outcome != PN_SASL_NONE && !sasl->sent_done) {
    pn_server_done(sasl);
    sasl->sent_done = true;
    sasl->rcvd_done = true;
    sasl->disp->halt = true;
  }
}

ssize_t pn_sasl_input(pn_sasl_t *sasl, char *bytes, size_t available)
{
  ssize_t n = pn_dispatcher_input(sasl->disp, bytes, available);
  if (n < 0) return n;

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

void pn_do_init(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->context;
  pn_symbol_t *mech = pn_to_symbol(pn_list_get(disp->args, SASL_INIT_MECHANISM));
  sasl->remote_mechanisms = strdup(pn_symbol_name(mech));
  sasl->recv_data = pn_binary_dup(pn_to_binary(pn_list_get(disp->args, SASL_INIT_INITIAL_RESPONSE)));
  sasl->rcvd_init = true;
}

void pn_do_mechanisms(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->context;
  sasl->rcvd_init = true;
}

void pn_do_challenge(pn_dispatcher_t *disp)
{
  
}

void pn_do_response(pn_dispatcher_t *resp)
{
  
}

void pn_do_outcome(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->context;
  sasl->outcome = pn_to_uint8(pn_list_get(disp->args, SASL_OUTCOME_CODE));
  sasl->rcvd_done = true;
  sasl->sent_done = true;
  disp->halt = true;
}
