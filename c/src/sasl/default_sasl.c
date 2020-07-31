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

#include "proton/sasl.h"
#include "proton/sasl_plugin.h"

#include <stdlib.h>
#include <string.h>

// SASL implementation interface
static void default_sasl_prepare(pn_transport_t *transport);
static void default_sasl_impl_free(pn_transport_t *transport);
static const char *default_sasl_impl_list_mechs(pn_transport_t *transport);

static bool default_sasl_init_server(pn_transport_t *transport);
static void default_sasl_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv);
static void default_sasl_process_response(pn_transport_t *transport, const pn_bytes_t *recv);

static bool default_sasl_init_client(pn_transport_t *transport);
static bool default_sasl_process_mechanisms(pn_transport_t *transport, const char *mechs);
static void default_sasl_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv);
static void default_sasl_process_outcome(pn_transport_t *transport, const pn_bytes_t *recv);

static bool default_sasl_impl_can_encrypt(pn_transport_t *transport);
static ssize_t default_sasl_impl_max_encrypt_size(pn_transport_t *transport);
static ssize_t default_sasl_impl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);
static ssize_t default_sasl_impl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);

extern const pnx_sasl_implementation default_sasl_impl;
const pnx_sasl_implementation default_sasl_impl = {
    default_sasl_impl_free,
    default_sasl_impl_list_mechs,

    default_sasl_init_server,
    default_sasl_init_client,

    default_sasl_prepare,

    default_sasl_process_init,
    default_sasl_process_response,

    default_sasl_process_mechanisms,
    default_sasl_process_challenge,
    default_sasl_process_outcome,

    default_sasl_impl_can_encrypt,
    default_sasl_impl_max_encrypt_size,
    default_sasl_impl_encode,
    default_sasl_impl_decode
};

static const char ANONYMOUS[] = "ANONYMOUS";
static const char EXTERNAL[] = "EXTERNAL";
static const char PLAIN[] = "PLAIN";

void default_sasl_prepare(pn_transport_t* transport)
{
}

bool default_sasl_init_server(pn_transport_t* transport)
{
  // Setup to send SASL mechanisms frame
  pnx_sasl_set_desired_state(transport, SASL_POSTED_MECHANISMS);
  return true;
}

bool default_sasl_init_client(pn_transport_t* transport)
{
  return true;
}

void default_sasl_impl_free(pn_transport_t *transport)
{
  free(pnx_sasl_get_context(transport));
}

// Client handles ANONYMOUS or PLAIN mechanisms if offered
// Offered mechanisms have already been filtered against the "allowed" list
bool default_sasl_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
  const char *username = pnx_sasl_get_username(transport);
  const char *password = pnx_sasl_get_password(transport);
  const char *authzid  = pnx_sasl_get_authorization(transport);

  // Check whether offered EXTERNAL, PLAIN or ANONYMOUS
  // Look for "EXTERNAL" in mechs
  const char *found = strstr(mechs, EXTERNAL);
  // Make sure that string is separated and terminated
  if (found && (found==mechs || found[-1]==' ') && (found[8]==0 || found[8]==' ')) {
    pnx_sasl_set_selected_mechanism(transport, EXTERNAL);
    if (authzid) {
      size_t size = strlen(authzid);
      char *iresp = (char *) malloc(size);
      if (!iresp) return false;

      pnx_sasl_set_context(transport, iresp);

      memmove(iresp, authzid, size);
      pnx_sasl_set_bytes_out(transport, pn_bytes(size, iresp));
    } else {
      static const char empty[] = "";
      pnx_sasl_set_bytes_out(transport, pn_bytes(0, empty));
    }
    pnx_sasl_set_desired_state(transport, SASL_POSTED_INIT);
    return true;
  }

  // Look for "PLAIN" in mechs
  found = strstr(mechs, PLAIN);
  // Make sure that string is separated and terminated, allowed
  // and we have a username and password and connection is encrypted or we allow insecure
  if (found && (found==mechs || found[-1]==' ') && (found[5]==0 || found[5]==' ') &&
      (pnx_sasl_is_transport_encrypted(transport) || pnx_sasl_get_allow_insecure_mechanisms(transport)) &&
      username && password) {
    pnx_sasl_set_selected_mechanism(transport, PLAIN);
    size_t zsize = authzid ? strlen(authzid) : 0;
    size_t usize = strlen(username);
    size_t psize = strlen(password);
    size_t size = zsize + usize + psize + 2;
    char *iresp = (char *) malloc(size);
    if (!iresp) return false;

    pnx_sasl_set_context(transport, iresp);

    if (authzid) memmove(&iresp[0], authzid, zsize);
    iresp[zsize] = 0;
    memmove(&iresp[zsize + 1], username, usize);
    iresp[zsize + usize + 1] = 0;
    memmove(&iresp[zsize + usize + 2], password, psize);
    pnx_sasl_set_bytes_out(transport, pn_bytes(size, iresp));

    // Zero out password and dealloc
    pnx_sasl_clear_password(transport);

    pnx_sasl_set_desired_state(transport, SASL_POSTED_INIT);
    return true;
  }

  // Look for "ANONYMOUS" in mechs
  found = strstr(mechs, ANONYMOUS);
  // Make sure that string is separated and terminated and allowed
  if (found && (found==mechs || found[-1]==' ') && (found[9]==0 || found[9]==' ')) {
    pnx_sasl_set_selected_mechanism(transport, ANONYMOUS);
    if (username) {
      size_t size = strlen(username);
      char *iresp = (char *) malloc(size);
      if (!iresp) return false;

      pnx_sasl_set_context(transport, iresp);

      memmove(iresp, username, size);
      pnx_sasl_set_bytes_out(transport, pn_bytes(size, iresp));
    } else {
      static const char anon[] = "anonymous";
      pnx_sasl_set_bytes_out(transport, pn_bytes(sizeof anon-1, anon));
    }
    pnx_sasl_set_desired_state(transport, SASL_POSTED_INIT);
    return true;
  }
  return false;
}

// Server will offer only ANONYMOUS and EXTERNAL if appropriate
const char *default_sasl_impl_list_mechs(pn_transport_t *transport)
{
  // If we have an external authid then we can offer EXTERNAL
  if (pnx_sasl_get_external_username(transport)) {
    return "EXTERNAL ANONYMOUS";
  } else {
    return "ANONYMOUS";
  }
}

// The selected mechanism has already been verified against the "allowed" list
void default_sasl_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
  // Check that mechanism is ANONYMOUS
  if (strcmp(mechanism, ANONYMOUS)==0) {
    pnx_sasl_set_succeeded(transport, "anonymous", "anonymous");
    pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
    return;
  }

  // Or maybe EXTERNAL
  const char *ext_username = pnx_sasl_get_external_username(transport);
  if (strcmp(mechanism, EXTERNAL)==0 && ext_username) {

    char *authzid = NULL;

    if (recv->size) {
      authzid = (char *) malloc(recv->size+1);
      pnx_sasl_set_context(transport, authzid);

      // If we didn't get memory pretend no authzid
      if (authzid) {
        memcpy(&authzid[0], recv->start, recv->size);
        authzid[recv->size] = 0;
      }
    }

    pnx_sasl_set_succeeded(transport, ext_username, authzid ? authzid : ext_username);
    pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
    return;
  }

  // Otherwise authentication failed
  pnx_sasl_set_failed(transport);
  pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);

}

/* The default implementation neither sends nor receives challenges or responses */
void default_sasl_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
}

void default_sasl_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
}

void default_sasl_process_outcome(pn_transport_t* transport, const pn_bytes_t *recv)
{
}

bool default_sasl_impl_can_encrypt(pn_transport_t *transport)
{
  return false;
}

ssize_t default_sasl_impl_max_encrypt_size(pn_transport_t *transport)
{
  return 0;
}

ssize_t default_sasl_impl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  return 0;
}

ssize_t default_sasl_impl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  return 0;
}
