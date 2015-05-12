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

#include "engine/engine-internal.h"

#include <sasl/sasl.h>

// If the version of Cyrus SASL is too early for sasl_client_done()/sasl_server_done()
// don't do any global clean up as it's not safe to use just sasl_done() for an
// executable that uses both client and server parts of Cyrus SASL, because it can't
// be called twice.
#if SASL_VERSION_FULL<0x020118
# define sasl_client_done()
# define sasl_server_done()
#endif

static const char *amqp_service = "amqp";

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

int pni_sasl_impl_list_mechs(pn_transport_t *transport, char **mechlist)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
  int count = 0;
  if (cyrus_conn) {
    const char *result = NULL;

    int r = sasl_listmech(cyrus_conn, NULL, "", " ", "", &result, NULL, &count);
    if (pni_check_sasl_result(cyrus_conn, r, transport)) {
      if (result && *result) {
        *mechlist = pn_strdup(result);
      }
    }
  }
  return count;
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

bool pni_init_client(pn_transport_t* transport) {
    int result;
    pni_sasl_t *sasl = transport->sasl;

    if (sasl->config_dir) {
        result = sasl_set_path(SASL_PATH_TYPE_CONFIG, sasl->config_dir);
        if (result!=SASL_OK) return false;
    }

    result = sasl_client_init(NULL);
    if (result!=SASL_OK) return false;

    const sasl_callback_t *callbacks = sasl->username ? sasl->password ? pni_user_password_callbacks : pni_user_callbacks : NULL;
    sasl_conn_t *cyrus_conn;
    result = sasl_client_new(amqp_service,
                             sasl->remote_fqdn,
                             NULL, NULL,
                             callbacks, 0,
                             &cyrus_conn);
    if (result!=SASL_OK) return false;
    sasl->impl_context = cyrus_conn;

    return true;
}

static int pni_wrap_client_start(pni_sasl_t *sasl, const char *mechs, const char **mechusing)
{
    int result;
    sasl_interact_t *client_interact=NULL;
    const char *out;
    unsigned outlen;

    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    do {

        result = sasl_client_start(cyrus_conn,
                                   mechs,
                                   &client_interact,
                                   &out, &outlen,
                                   mechusing);
        if (result==SASL_INTERACT) {
            pni_cyrus_interact(sasl, client_interact);
        }
    } while (result==SASL_INTERACT);

    sasl->bytes_out.start = out;
    sasl->bytes_out.size = outlen;
    return result;
}

bool pni_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
    pni_sasl_t *sasl = transport->sasl;
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    const char *mech_selected;
    int result = pni_wrap_client_start(sasl, mechs, &mech_selected);
    switch (result) {
        case SASL_OK:
        case SASL_CONTINUE:
          sasl->selected_mechanism = pn_strdup(mech_selected);
          return true;
        case SASL_NOMECH:
        default:
          pni_check_sasl_result(cyrus_conn, result, transport);
          return false;
    }
}


static int pni_wrap_client_step(pni_sasl_t *sasl, const pn_bytes_t *in)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    sasl_interact_t *client_interact=NULL;
    const char *out;
    unsigned outlen;

    int result;
    do {

        result = sasl_client_step(cyrus_conn,
                                  in->start, in->size,
                                  &client_interact,
                                  &out, &outlen);
        if (result==SASL_INTERACT) {
            pni_cyrus_interact(sasl, client_interact);
        }
    } while (result==SASL_INTERACT);

    sasl->bytes_out.start = out;
    sasl->bytes_out.size = outlen;
    return result;
}

void pni_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_t *sasl = transport->sasl;
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
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
            pni_check_sasl_result(cyrus_conn, result, transport);

            // Failed somehow - equivalent to failing authentication
            sasl->outcome = PN_SASL_AUTH;
            pni_sasl_set_desired_state(transport, SASL_RECVED_OUTCOME);
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

    sasl_conn_t *cyrus_conn;
    result = sasl_server_new(amqp_service, NULL, NULL, NULL, NULL, NULL, 0, &cyrus_conn);
    if (result!=SASL_OK) return result;
    sasl->impl_context = cyrus_conn;

    sasl_security_properties_t secprops = {0};
    secprops.security_flags =
    SASL_SEC_NOPLAINTEXT |
    ( transport->auth_required ? SASL_SEC_NOANONYMOUS : 0 ) ;

    result = sasl_setprop(cyrus_conn, SASL_SEC_PROPS, &secprops);
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
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    // If we didn't get any initial response, pretend we got an empty response as it seems cyrus
    // assumes this is what it will get.
    const char *in_bytes = in->start;
    size_t in_size = in->size;
    if (!in_bytes) {
        in_bytes = "";
        in_size = 0;
    }
    result = sasl_server_start(cyrus_conn,
                               mech_selected,
                               in_bytes, in_size,
                               &out, &outlen);

    sasl->bytes_out.start = out;
    sasl->bytes_out.size = outlen;
    return result;
}

static void pni_process_server_result(pn_transport_t *transport, int result)
{
    pni_sasl_t *sasl = transport->sasl;
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    switch (result) {
        case SASL_OK:
            // Authenticated
            sasl->outcome = PN_SASL_OK;
            transport->authenticated = true;
            // Get username from SASL
            const void* value;
            sasl_getprop(cyrus_conn, SASL_USERNAME, &value);
            sasl->username = (const char*) value;
            pn_transport_logf(transport, "Authenticated user: %s with mechanism %s", sasl->username, sasl->selected_mechanism);
            pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
            break;
        case SASL_CONTINUE:
            // Need to send a challenge
            pni_sasl_set_desired_state(transport, SASL_POSTED_CHALLENGE);
            break;
        default:
            pni_check_sasl_result(cyrus_conn, result, transport);

            // Failed to authenticate
            sasl->outcome = PN_SASL_AUTH;
            pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
            break;
    }
}

bool pni_init_server(pn_transport_t* transport)
{
    int r = pni_wrap_server_new(transport);
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
    return pni_check_sasl_result(cyrus_conn, r, transport);
}

void pni_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
    pni_sasl_t *sasl = transport->sasl;

    int result = pni_wrap_server_start(sasl, mechanism, recv);
    if (result==SASL_OK) {
        // We need to filter out a supplied mech in in the inclusion list
        // as the client could have used a mech that we support, but that
        // wasn't on the list we sent.
        if (!pni_included_mech(sasl->included_mechanisms, pn_bytes(strlen(mechanism), mechanism))) {
            sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
            sasl_seterror(cyrus_conn, 0, "Client mechanism not in mechanism inclusion list.");
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
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    result = sasl_server_step(cyrus_conn,
                              in->start, in->size,
                              &out, &outlen);

    sasl->bytes_out.start = out;
    sasl->bytes_out.size = outlen;
    return result;
}

void pni_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_t *sasl = transport->sasl;
    int result = pni_wrap_server_step(sasl, recv);
    pni_process_server_result(transport, result);
}

void pni_sasl_impl_free(pn_transport_t *transport)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
    sasl_dispose(&cyrus_conn);
    transport->sasl->impl_context = cyrus_conn;
    if (transport->sasl->client) {
        sasl_client_done();
    } else {
        sasl_server_done();
    }
}
