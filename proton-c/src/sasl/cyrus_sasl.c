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

#include "sasl-internal.h"

#include "protocol.h"
#include "dispatch_actions.h"
#include "engine/engine-internal.h"
#include "proton/codec.h"

#include <sasl/sasl.h>

// If the version of Cyrus SASL is too early for sasl_client_done()/sasl_server_done()
// don't do any global clean up as it's not safe to use just sasl_done() for an
// executable that uses both client and server parts of Cyrus SASL, because it can't
// be called twice.
#if SASL_VERSION_FULL<0x020118
# define sasl_client_done()
# define sasl_server_done()
#endif

enum pni_sasl_state {
  SASL_NONE,
  SASL_POSTED_INIT,
  SASL_POSTED_MECHANISMS,
  SASL_POSTED_RESPONSE,
  SASL_POSTED_CHALLENGE,
  SASL_PRETEND_OUTCOME,
  SASL_RECVED_OUTCOME,
  SASL_POSTED_OUTCOME
};

struct pni_sasl_t {
    // Client selected mechanism
    char *selected_mechanism;
    char *included_mechanisms;
    const char *username;
    char *password;
    const char *config_name;
    char *config_dir;
    const char *remote_fqdn;
    sasl_conn_t *cyrus_conn;
    pn_sasl_outcome_t outcome;
    pn_bytes_t cyrus_out;
    enum pni_sasl_state desired_state;
    enum pni_sasl_state last_state;
    bool client;
    bool halt;
};

static bool pni_sasl_is_server_state(enum pni_sasl_state state)
{
  return state==SASL_NONE
      || state==SASL_POSTED_MECHANISMS
      || state==SASL_POSTED_CHALLENGE
      || state==SASL_POSTED_OUTCOME;
}

static bool pni_sasl_is_client_state(enum pni_sasl_state state)
{
  return state==SASL_NONE
      || state==SASL_POSTED_INIT
      || state==SASL_POSTED_RESPONSE
      || state==SASL_PRETEND_OUTCOME
      || state==SASL_RECVED_OUTCOME;
}

static bool pni_sasl_is_final_input_state(pni_sasl_t *sasl)
{
  enum pni_sasl_state last_state = sasl->last_state;
  enum pni_sasl_state desired_state = sasl->desired_state;
  return last_state==SASL_RECVED_OUTCOME
      || desired_state==SASL_POSTED_OUTCOME;
}

static bool pni_sasl_is_final_output_state(pni_sasl_t *sasl)
{
  enum pni_sasl_state last_state = sasl->last_state;
  return last_state==SASL_PRETEND_OUTCOME
      || last_state==SASL_RECVED_OUTCOME
      || last_state==SASL_POSTED_OUTCOME;
}

static const char *amqp_service = "amqp";

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

static void pni_emit(pn_transport_t *transport)
{
  if (transport->connection && transport->connection->collector) {
    pn_collector_t *collector = transport->connection->collector;
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT);
  }
}

// Look for symbol in the mech include list - not particlarly efficient,
// but probably not used enough to matter.
//
// Note that if there is no inclusion list then every mech is implicitly included.
static bool pni_included_mech(const char *included_mech_list, pn_bytes_t s)
{
  if (!included_mech_list) return true;

  const char * end_list = included_mech_list+strlen(included_mech_list);
  size_t len = s.size;
  const char *c = included_mech_list;
  while (c!=NULL) {
    // If there are not enough chars left in the list no matches
    if ((ptrdiff_t)len > end_list-c) return false;

    // Is word equal with a space or end of string afterwards?
    if (strncasecmp(c, s.start, len)==0 && (c[len]==' ' || c[len]==0) ) return true;

    c = strchr(c, ' ');
    c = c ? c+1 : NULL;
  }
  return false;
}

// This takes a space separated list and zero terminates it in place
// whilst adding pointers to the existing strings in a string array.
// This means that you can't free the original storage until you have
// finished with the resulting list.
static void pni_split_mechs(char *mechlist, const char* included_mechs, char *mechs[], int *count)
{
  char *start = mechlist;
  char *end = start;

  while (*end) {
    if (*end == ' ') {
      if (start != end) {
        *end = '\0';
        if (pni_included_mech(included_mechs, pn_bytes(end-start, start))) {
          mechs[(*count)++] = start;
        }
      }
      end++;
      start = end;
    } else {
      end++;
    }
  }

  if (start != end) {
    if (pni_included_mech(included_mechs, pn_bytes(end-start, start))) {
      mechs[(*count)++] = start;
    }
  }
}

static bool pni_check_sasl_result(sasl_conn_t *conn, int r, pn_transport_t *logger)
{
    if (r!=SASL_OK) {
        pn_transport_logf(logger, "sasl error: %s", conn ? sasl_errdetail(conn) : sasl_errstring(r, NULL, NULL));
        return false;
    }
    return true;
}

// Cyrus wrappers
static void pni_cyrus_interact(pni_sasl_t *sasl, sasl_interact_t *interact)
{
  for (sasl_interact_t *i = interact; i->id!=SASL_CB_LIST_END; i++) {
    switch (i->id) {
    case SASL_CB_USER:
      i->result = sasl->username;
      i->len = strlen(sasl->username);
      break;
    case SASL_CB_AUTHNAME:
      i->result = sasl->username;
      i->len = strlen(sasl->username);
      break;
    case SASL_CB_PASS:
      i->result = sasl->password;
      i->len = strlen(sasl->password);
      break;
    default:
      fprintf(stderr, "(%s): %s - %s\n", i->challenge, i->prompt, i->defresult);
    }
  }
}

static void pni_sasl_set_desired_state(pn_transport_t *transport, enum pni_sasl_state desired_state)
{
  pni_sasl_t *sasl = transport->sasl;
  if (sasl->last_state > desired_state) {
    pn_transport_logf(transport, "Trying to send SASL frame (%d), but illegal: already in later state (%d)", desired_state, sasl->last_state);
  } else if (sasl->client && !pni_sasl_is_client_state(desired_state)) {
    pn_transport_logf(transport, "Trying to send server SASL frame (%d) on a client", desired_state);
  } else if (!sasl->client && !pni_sasl_is_server_state(desired_state)) {
    pn_transport_logf(transport, "Trying to send client SASL frame (%d) on a server", desired_state);
  } else {
    // If we need to repeat CHALLENGE or RESPONSE frames adjust current state to seem
    // like they haven't been sent yet
    if (sasl->last_state==desired_state && desired_state==SASL_POSTED_RESPONSE) {
      sasl->last_state = SASL_POSTED_INIT;
    }
    if (sasl->last_state==desired_state && desired_state==SASL_POSTED_CHALLENGE) {
      sasl->last_state = SASL_POSTED_MECHANISMS;
    }
    sasl->desired_state = desired_state;
  }
}

// Post SASL frame
static void pni_post_sasl_frame(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_bytes_t out = sasl->cyrus_out;
  enum pni_sasl_state desired_state = sasl->desired_state;
  while (sasl->desired_state > sasl->last_state) {
    switch (desired_state) {
    case SASL_POSTED_INIT:
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[sz]", SASL_INIT, sasl->selected_mechanism,
                    out.size, out.start);
      break;
    case SASL_PRETEND_OUTCOME:
      if (sasl->last_state < SASL_POSTED_INIT) {
        desired_state = SASL_POSTED_INIT;
        continue;
      }
      break;
    case SASL_POSTED_MECHANISMS: {
      // TODO: Hardcoded limit of 16 mechanisms
      char *mechs[16];
      int count = 0;

      char *mechlist = NULL;
      if (sasl->cyrus_conn) {
        const char *result = NULL;

        int r = sasl_listmech(sasl->cyrus_conn, NULL, "", " ", "", &result, NULL, NULL);
        if (pni_check_sasl_result(sasl->cyrus_conn, r, transport)) {
          if (result && *result) {
            mechlist = strdup(result);
            pni_split_mechs(mechlist, sasl->included_mechanisms, mechs, &count);
          }
        }
      }
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[@T[*s]]", SASL_MECHANISMS, PN_SYMBOL, count, mechs);
      free(mechlist);
      break;
    }
    case SASL_POSTED_RESPONSE:
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[z]", SASL_RESPONSE, out.size, out.start);
      break;
    case SASL_POSTED_CHALLENGE:
      if (sasl->last_state < SASL_POSTED_MECHANISMS) {
        desired_state = SASL_POSTED_MECHANISMS;
        continue;
      }
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[z]", SASL_CHALLENGE, out.size, out.start);
      break;
    case SASL_POSTED_OUTCOME:
      if (sasl->last_state < SASL_POSTED_MECHANISMS) {
        desired_state = SASL_POSTED_MECHANISMS;
        continue;
      }
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[B]", SASL_OUTCOME, sasl->outcome);
      break;
    case SASL_NONE:
    case SASL_RECVED_OUTCOME:
      return;
    }
    sasl->last_state = desired_state;
    desired_state = sasl->desired_state;
  }
  pni_emit(transport);
}

// Set up callbacks to use interact
static const sasl_callback_t pni_user_password_callbacks[] = {
  {SASL_CB_USER, NULL, NULL},
  {SASL_CB_AUTHNAME, NULL, NULL},
  {SASL_CB_PASS, NULL, NULL},
  {SASL_CB_LIST_END, NULL, NULL},
};

static const sasl_callback_t pni_user_callbacks[] = {
  {SASL_CB_USER, NULL, NULL},
  {SASL_CB_AUTHNAME, NULL, NULL},
  {SASL_CB_LIST_END, NULL, NULL},
};

static int pni_wrap_client_start(pni_sasl_t *sasl, const char *mechs, const char **mechusing)
{
  int result;

  if (sasl->config_dir) {
    result = sasl_set_path(SASL_PATH_TYPE_CONFIG, sasl->config_dir);
    if (result!=SASL_OK) return result;
  }

  result = sasl_client_init(NULL);
  if (result!=SASL_OK) return result;

  const sasl_callback_t *callbacks = sasl->username ? sasl->password ? pni_user_password_callbacks : pni_user_callbacks : NULL;
  result = sasl_client_new(amqp_service,
                           sasl->remote_fqdn,
                           NULL, NULL,
                           callbacks, 0,
                           &sasl->cyrus_conn);
  if (result!=SASL_OK) return result;

  sasl_interact_t *client_interact=NULL;
  const char *out;
  unsigned outlen;

  do {

    result = sasl_client_start(sasl->cyrus_conn,
                               mechs,
                               &client_interact,
                               &out, &outlen,
                               mechusing);
    if (result==SASL_INTERACT) {
      pni_cyrus_interact(sasl, client_interact);
    }
  } while (result==SASL_INTERACT);

  sasl->cyrus_out.start = out;
  sasl->cyrus_out.size = outlen;
  return result;
}

static void pni_process_mechanisms(pn_transport_t *transport, const char *mechs, bool short_circuit)
{
  pni_sasl_t *sasl = transport->sasl;
  const char *mech_selected;
  int result = pni_wrap_client_start(sasl, mechs, &mech_selected);
  switch (result) {
  case SASL_OK:
  case SASL_CONTINUE:
    sasl->selected_mechanism = strdup(mech_selected);
    pni_sasl_set_desired_state(transport, short_circuit ? SASL_PRETEND_OUTCOME : SASL_POSTED_INIT);
    break;
  case SASL_NOMECH:
  default:
    pni_check_sasl_result(sasl->cyrus_conn, result, transport);
    sasl->last_state = SASL_RECVED_OUTCOME;
    sasl->halt = true;
    pn_transport_close_tail(transport);
    break;
  }
}


static int pni_wrap_client_step(pni_sasl_t *sasl, const pn_bytes_t *in)
{
  sasl_interact_t *client_interact=NULL;
  const char *out;
  unsigned outlen;

  int result;
  do {

    result = sasl_client_step(sasl->cyrus_conn,
                              in->start, in->size,
                              &client_interact,
                              &out, &outlen);
    if (result==SASL_INTERACT) {
      pni_cyrus_interact(sasl, client_interact);
    }
  } while (result==SASL_INTERACT);

  sasl->cyrus_out.start = out;
  sasl->cyrus_out.size = outlen;
  return result;
}

static void pni_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
  pni_sasl_t *sasl = transport->sasl;
  int result = pni_wrap_client_step(sasl, recv);
  switch (result) {
  case SASL_OK:
    // Authenticated
    // TODO: Documented that we need to call sasl_client_step() again to be sure!;
  case SASL_CONTINUE:
    // Need to send a response
    pni_sasl_set_desired_state(transport, SASL_POSTED_RESPONSE);
    break;
  default:
    pni_check_sasl_result(sasl->cyrus_conn, result, transport);

    // Failed somehow
    sasl->halt = true;
    pn_transport_close_tail(transport);
    break;
  }
}

static int pni_wrap_server_new(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  int result;

  if (sasl->config_dir) {
    result = sasl_set_path(SASL_PATH_TYPE_CONFIG, sasl->config_dir);
    if (result!=SASL_OK) return result;
  }

  result = sasl_server_init(NULL, sasl->config_name);
  if (result!=SASL_OK) return result;

  result = sasl_server_new(amqp_service, NULL, NULL, NULL, NULL, NULL, 0, &sasl->cyrus_conn);
  if (result!=SASL_OK) return result;

  sasl_security_properties_t secprops = {0};
  secprops.security_flags =
    SASL_SEC_NOPLAINTEXT |
    ( transport->auth_required ? SASL_SEC_NOANONYMOUS : 0 ) ;

  result = sasl_setprop(sasl->cyrus_conn, SASL_SEC_PROPS, &secprops);
  if (result!=SASL_OK) return result;

  // EXTERNAL not implemented yet
#if 0
  sasl_ssf_t ssf = 128;
  result = sasl_setprop(sasl->cyrus_conn, SASL_SSF_EXTERNAL, &ssf);
  if (result!=SASL_OK) return result;

  const char *extid = "user";
  result = sasl_setprop(sasl->cyrus_conn, SASL_AUTH_EXTERNAL, extid);
  if (result!=SASL_OK) return result;
#endif

  return result;
}

static int pni_wrap_server_start(pni_sasl_t *sasl, const char *mech_selected, const pn_bytes_t *in)
{
  int result;
  const char *out;
  unsigned outlen;
  result = sasl_server_start(sasl->cyrus_conn,
                             mech_selected,
                             in->start, in->size,
                             &out, &outlen);

  sasl->cyrus_out.start = out;
  sasl->cyrus_out.size = outlen;
  return result;
}

static void pni_process_server_result(pn_transport_t *transport, int result)
{
  pni_sasl_t *sasl = transport->sasl;
  switch (result) {
  case SASL_OK:
    // Authenticated
    sasl->outcome = PN_SASL_OK;
    transport->authenticated = true;
    // Get username from SASL
    const void* value;
    sasl_getprop(sasl->cyrus_conn, SASL_USERNAME, &value);
    sasl->username = (const char*) value;
    pn_transport_logf(transport, "Authenticated user: %s with mechanism %s", sasl->username, sasl->selected_mechanism);
    pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
    break;
  case SASL_CONTINUE:
    // Need to send a challenge
    pni_sasl_set_desired_state(transport, SASL_POSTED_CHALLENGE);
    break;
  default:
    pni_check_sasl_result(sasl->cyrus_conn, result, transport);

    // Failed to authenticate
    sasl->outcome = PN_SASL_AUTH;
    pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
    break;
  }
  pni_emit(transport);
}

static void pni_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
  pni_sasl_t *sasl = transport->sasl;

  int result = pni_wrap_server_start(sasl, mechanism, recv);
  if (result==SASL_OK) {
    // We need to filter out a supplied mech in in the inclusion list
    // as the client could have used a mech that we support, but that
    // wasn't on the list we sent.
    if (!pni_included_mech(sasl->included_mechanisms, pn_bytes(strlen(mechanism), mechanism))) {
      sasl_seterror(sasl->cyrus_conn, 0, "Client mechanism not in mechanism inclusion list.");
      result = SASL_FAIL;
    }
  }
  pni_process_server_result(transport, result);
}

static int pni_wrap_server_step(pni_sasl_t *sasl, const pn_bytes_t *in)
{
    int result;
    const char *out;
    unsigned outlen;
    result = sasl_server_step(sasl->cyrus_conn,
                              in->start, in->size,
                              &out, &outlen);

    sasl->cyrus_out.start = out;
    sasl->cyrus_out.size = outlen;
    return result;
}

static void pni_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
  pni_sasl_t *sasl = transport->sasl;
  int result = pni_wrap_server_step(sasl, recv);
  pni_process_server_result(transport, result);
}

pn_sasl_t *pn_sasl(pn_transport_t *transport)
{
  if (!transport->sasl) {
    pni_sasl_t *sasl = (pni_sasl_t *) malloc(sizeof(pni_sasl_t));

    const char *sasl_config_path = getenv("PN_SASL_CONFIG_PATH");

    sasl->client = !transport->server;
    sasl->selected_mechanism = NULL;
    sasl->included_mechanisms = NULL;
    sasl->username = NULL;
    sasl->password = NULL;
    sasl->config_name = sasl->client ? "proton-client" : "proton-server";
    sasl->config_dir =  sasl_config_path ? strdup(sasl_config_path) : NULL;
    sasl->remote_fqdn = NULL;
    sasl->outcome = PN_SASL_NONE;
    sasl->cyrus_conn = NULL;
    sasl->cyrus_out.size = 0;
    sasl->cyrus_out.start = NULL;
    sasl->desired_state = SASL_NONE;
    sasl->last_state = SASL_NONE;
    sasl->halt = false;

    transport->sasl = sasl;
  }

  // The actual external pn_sasl_t pointer is a pointer to its enclosing pn_transport_t
  return (pn_sasl_t *)transport;
}

// This is a hack to tell us that
// no actual negotiation is going to happen and we can go
// straight to the AMQP layer; it can only work on the client side
// As the server doesn't know if SASL is even active until it sees
// the SASL header from the client first.
static void pni_sasl_force_anonymous(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  if (sasl->client) {
    // Pretend we got sasl mechanisms frame with just ANONYMOUS
    pni_process_mechanisms(transport, "ANONYMOUS", true);
  }
  pni_emit(transport);
}

void pni_sasl_set_remote_hostname(pn_transport_t * transport, const char * fqdn)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->remote_fqdn = fqdn;
}

void pni_sasl_set_user_password(pn_transport_t *transport, const char *user, const char *password)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->username = user;
  free(sasl->password);
  sasl->password = password ? strdup(password) : NULL;
}

const char *pn_sasl_get_user(pn_sasl_t *sasl0)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    return sasl->username;
}

const char *pn_sasl_get_mech(pn_sasl_t *sasl0)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    return sasl->selected_mechanism;
}

void pn_sasl_allowed_mechs(pn_sasl_t *sasl0, const char *mechs)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    free(sasl->included_mechanisms);
    sasl->included_mechanisms = mechs ? strdup(mechs) : NULL;
    if (strcmp(mechs, "ANONYMOUS")==0 ) {
      pn_transport_t *transport = get_transport_internal(sasl0);
      pni_sasl_force_anonymous(transport);
    }
}

void pn_sasl_config_name(pn_sasl_t *sasl0, const char *name)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    sasl->config_name = name;
}

void pn_sasl_config_path(pn_sasl_t *sasl0, const char *dir)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    free(sasl->config_dir);
    sasl->config_dir = strdup(dir);
}

void pn_sasl_done(pn_sasl_t *sasl0, pn_sasl_outcome_t outcome)
{
  pni_sasl_t *sasl = get_sasl_internal(sasl0);
  if (sasl) {
    sasl->outcome = outcome;
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
      free(sasl->selected_mechanism);
      free(sasl->included_mechanisms);
      free(sasl->password);
      free(sasl->config_dir);

      // CYRUS_SASL
      if (sasl->cyrus_conn) {
          sasl_dispose(&sasl->cyrus_conn);
          if (sasl->client) {
              sasl_client_done();
          } else {
              sasl_server_done();
          }
      }

      free(sasl);
    }
  }
}

static void pni_sasl_server_init(pn_transport_t *transport)
{
  int r = pni_wrap_server_new(transport);

  if (!pni_check_sasl_result(transport->sasl->cyrus_conn, r, transport)) {
      return;
  }

  // Setup to send SASL mechanisms frame
  pni_sasl_set_desired_state(transport, SASL_POSTED_MECHANISMS);
}

static void pn_sasl_process(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  if (!sasl->client) {
    if (sasl->desired_state<SASL_POSTED_MECHANISMS) {
      pni_sasl_server_init(transport);
    }
  }
}

ssize_t pn_sasl_input(pn_transport_t *transport, const char *bytes, size_t available)
{
  pn_sasl_process(transport);

  pni_sasl_t *sasl = transport->sasl;
  ssize_t n = pn_dispatcher_input(transport, bytes, available, false, &sasl->halt);

  if (n==0 && pni_sasl_is_final_input_state(sasl)) {
    return PN_EOS;
  }
  return n;
}

ssize_t pn_sasl_output(pn_transport_t *transport, char *bytes, size_t size)
{
  pn_sasl_process(transport);

  pni_post_sasl_frame(transport);

  pni_sasl_t *sasl = transport->sasl;
  if (transport->available == 0 && pni_sasl_is_final_output_state(sasl)) {
    if (sasl->outcome != PN_SASL_OK && pni_sasl_is_final_input_state(sasl)) {
      pn_transport_close_tail(transport);
    }
    return PN_EOS;
  } else {
    return pn_dispatcher_output(transport, bytes, size);
  }
}

// Received Server side
int pn_do_init(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_bytes_t mech;
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[sz]", &mech, &recv);
  if (err) return err;
  sasl->selected_mechanism = pn_strndup(mech.start, mech.size);

  pni_process_init(transport, sasl->selected_mechanism, &recv);

  return 0;
}

// Received client side
int pn_do_mechanisms(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  // If we already pretended we got the ANONYMOUS mech then ignore
  if (transport->sasl->last_state==SASL_PRETEND_OUTCOME) return 0;

  pn_string_t *mechs = pn_string("");

  // This scanning relies on pn_data_scan leaving the pn_data_t cursors
  // where they are after finishing the scan
  int err = pn_data_scan(args, "D.[@[");
  if (err) return err;

  // Now keep checking for end of array and pull a symbol
  while(pn_data_next(args)) {
    pn_bytes_t s = pn_data_get_symbol(args);
    if (pni_included_mech(transport->sasl->included_mechanisms, s)) {
      pn_string_addf(mechs, "%*s ", (int)s.size, s.start);
    }
  }
  pn_string_buffer(mechs)[pn_string_size(mechs)-1] = 0;

  pni_process_mechanisms(transport, pn_string_get(mechs), false);

  pn_free(mechs);
  return 0;
}

// Received client side
int pn_do_challenge(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[z]", &recv);
  if (err) return err;

  pni_process_challenge(transport, &recv);

  return 0;
}

// Received server side
int pn_do_response(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[z]", &recv);
  if (err) return err;

  pni_process_response(transport, &recv);

  return 0;
}

// Received client side
int pn_do_outcome(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;
  uint8_t outcome;
  int err = pn_data_scan(args, "D.[B]", &outcome);
  if (err) return err;
  sasl->outcome = (pn_sasl_outcome_t) outcome;
  sasl->last_state = SASL_RECVED_OUTCOME;
  sasl->halt = true;
  pni_emit(transport);
  return 0;
}
