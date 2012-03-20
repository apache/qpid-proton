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
#include "sasl-internal.h"
#include "../protocol.h"
#include "../util.h"

void pn_do_init(pn_dispatcher_t *disp);
void pn_do_mechanisms(pn_dispatcher_t *disp);
void pn_do_challenge(pn_dispatcher_t *disp);
void pn_do_response(pn_dispatcher_t *disp);
void pn_do_outcome(pn_dispatcher_t *disp);

pn_sasl_t *pn_sasl()
{
  pn_sasl_t *sasl = malloc(sizeof(pn_sasl_t));
  sasl->disp = pn_dispatcher(1, sasl);

  pn_dispatcher_action(sasl->disp, SASL_INIT, "SASL-INIT", pn_do_init);
  pn_dispatcher_action(sasl->disp, SASL_MECHANISMS, "SASL-MECHANISMS", pn_do_mechanisms);
  pn_dispatcher_action(sasl->disp, SASL_CHALLENGE, "SASL-CHALLENGE", pn_do_challenge);
  pn_dispatcher_action(sasl->disp, SASL_RESPONSE, "SASL-RESPONSE", pn_do_response);
  pn_dispatcher_action(sasl->disp, SASL_OUTCOME, "SASL-OUTCOME", pn_do_outcome);

  sasl->init = false;
  sasl->outcome = SASL_NONE;
  sasl->mechanism = NULL;
  sasl->challenge = NULL;
  sasl->response = NULL;

  return sasl;
}

void pn_sasl_mechanism_set(pn_sasl_t *sasl, const char *mechanism)
{
  if (sasl->mechanism) pn_free_symbol(sasl->mechanism);
  sasl->mechanism = pn_symbol(mechanism);
}

const char *pn_sasl_mechanism(pn_sasl_t *sasl)
{
  if (sasl->mechanism)
    return pn_symbol_name(sasl->mechanism);
  else
    return NULL;
}

void pn_sasl_challenge_set(pn_sasl_t *sasl, pn_binary_t *challenge)
{
  if (sasl->challenge) pn_free_binary(sasl->challenge);
  sasl->challenge = pn_binary_dup(challenge);
}

pn_binary_t *pn_sasl_challenge(pn_sasl_t *sasl)
{
  return sasl->challenge;
}

void pn_sasl_response_set(pn_sasl_t *sasl, pn_binary_t *response)
{
  if (sasl->response) pn_free_binary(sasl->response);
  sasl->response = pn_binary_dup(response);
}

pn_binary_t *pn_sasl_response(pn_sasl_t *sasl)
{
  return sasl->response;
}

void pn_sasl_client(pn_sasl_t *sasl, const char *mechanism, const char *username, const char *password)
{
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

  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_INIT_MECHANISM, pn_from_symbol(pn_symbol(mechanism)));
  pn_field(sasl->disp, SASL_INIT_INITIAL_RESPONSE, pn_from_binary(pn_binary(iresp, size)));
  pn_post_frame(sasl->disp, 0, SASL_INIT);
  sasl->init = true;
}

void pn_sasl_server(pn_sasl_t *sasl)
{
  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_MECHANISMS_SASL_SERVER_MECHANISMS,
           pn_value("@s[ss]", "PLAIN", "ANONYMOUS"));
  pn_post_frame(sasl->disp, 0, SASL_MECHANISMS);
  sasl->init = true;
}

void pn_sasl_auth(pn_sasl_t *sasl, pn_sasl_outcome_t outcome)
{
  sasl->outcome = outcome;

  pn_init_frame(sasl->disp);
  pn_field(sasl->disp, SASL_OUTCOME_CODE, pn_value("B", outcome));
  pn_post_frame(sasl->disp, 0, SASL_OUTCOME);
}

bool pn_sasl_init(pn_sasl_t *sasl)
{
  return sasl->init;
}

void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace)
{
  sasl->disp->trace = trace;
}

void pn_sasl_destroy(pn_sasl_t *sasl)
{
  pn_sasl_mechanism_set(sasl, NULL);
  pn_sasl_challenge_set(sasl, NULL);
  pn_sasl_response_set(sasl, NULL);
  pn_dispatcher_destroy(sasl->disp);
  free(sasl);
}

ssize_t pn_sasl_input(pn_sasl_t *sasl, char *bytes, size_t available)
{
  if (sasl->outcome != SASL_NONE) {
    return PN_EOS;
  } else {
    return pn_dispatcher_input(sasl->disp, bytes, available);
  }
}

ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size)
{
  if (sasl->disp->available == 0 && sasl->outcome != SASL_NONE) {
    return PN_EOS;
  } else {
    return pn_dispatcher_output(sasl->disp, bytes, size);
  }
}

pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl)
{
  return sasl->outcome;
}

void pn_do_init(pn_dispatcher_t *disp)
{
  pn_sasl_t *sasl = disp->context;
  pn_symbol_t *mech = pn_to_symbol(pn_list_get(disp->args, SASL_INIT_MECHANISM));
  pn_sasl_mechanism_set(sasl, pn_symbol_name(mech));
  pn_sasl_response_set(sasl, pn_to_binary(pn_list_get(disp->args, SASL_INIT_INITIAL_RESPONSE)));
}

void pn_do_mechanisms(pn_dispatcher_t *disp)
{
  //pn_sasl_t *sasl = disp->context;
  
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
}
