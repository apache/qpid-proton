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

#include "core/config.h"
#include "core/engine-internal.h"
#include "sasl-internal.h"


#include <sasl/sasl.h>
#include <pthread.h>

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
    if (r==SASL_OK) return true;

    const char* err = conn ? sasl_errdetail(conn) : sasl_errstring(r, NULL, NULL);
    if (logger->trace & PN_TRACE_DRV)
        pn_transport_logf(logger, "sasl error: %s", err);
    pn_condition_t* c = pn_transport_condition(logger);
    pn_condition_set_name(c, "proton:io:sasl_error");
    pn_condition_set_description(c, err);
    return false;
}

// Cyrus wrappers
static void pni_cyrus_interact(pni_sasl_t *sasl, sasl_interact_t *interact)
{
  for (sasl_interact_t *i = interact; i->id!=SASL_CB_LIST_END; i++) {
    switch (i->id) {
    case SASL_CB_USER:
      i->result = 0;
      i->len = 0;
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

// Machinery to initialise the cyrus library only once even in a multithreaded environment
// Relies on pthreads.
static const char * const default_config_name = "proton-server";
static char *pni_cyrus_config_dir = NULL;
static char *pni_cyrus_config_name = NULL;
static pthread_mutex_t pni_cyrus_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool pni_cyrus_client_started = false;
static bool pni_cyrus_server_started = false;

__attribute__((destructor))
static void pni_cyrus_finish(void) {
  pthread_mutex_lock(&pni_cyrus_mutex);
  if (pni_cyrus_client_started) sasl_client_done();
  if (pni_cyrus_server_started) sasl_server_done();
  free(pni_cyrus_config_dir);
  free(pni_cyrus_config_name);
  pthread_mutex_unlock(&pni_cyrus_mutex);
}

static int pni_cyrus_client_init_rc = SASL_OK;
static void pni_cyrus_client_once(void) {
  pthread_mutex_lock(&pni_cyrus_mutex);
  int result = SASL_OK;
  if (pni_cyrus_config_dir) {
    result = sasl_set_path(SASL_PATH_TYPE_CONFIG, pni_cyrus_config_dir);
  }
  if (result==SASL_OK) {
    result = sasl_client_init(NULL);
  }
  pni_cyrus_client_started = true;
  pni_cyrus_client_init_rc = result;
  pthread_mutex_unlock(&pni_cyrus_mutex);
}

static int pni_cyrus_server_init_rc = SASL_OK;
static void pni_cyrus_server_once(void) {
  pthread_mutex_lock(&pni_cyrus_mutex);
  int result = SASL_OK;
  if (pni_cyrus_config_dir) {
    result = sasl_set_path(SASL_PATH_TYPE_CONFIG, pni_cyrus_config_dir);
  }
  if (result==SASL_OK) {
    result = sasl_server_init(NULL, pni_cyrus_config_name ? pni_cyrus_config_name : default_config_name);
  }
  pni_cyrus_server_started = true;
  pni_cyrus_server_init_rc = result;
  pthread_mutex_unlock(&pni_cyrus_mutex);
}

static pthread_once_t pni_cyrus_client_init = PTHREAD_ONCE_INIT;
static void pni_cyrus_client_start(void) {
    pthread_once(&pni_cyrus_client_init, pni_cyrus_client_once);
}
static pthread_once_t pni_cyrus_server_init = PTHREAD_ONCE_INIT;
static void pni_cyrus_server_start(void) {
  pthread_once(&pni_cyrus_server_init, pni_cyrus_server_once);
}

bool pni_init_client(pn_transport_t* transport) {
  pni_sasl_t *sasl = transport->sasl;
  int result;
  sasl_conn_t *cyrus_conn = NULL;
  do {
    // If pni_cyrus_config_dir already set then we already called pni_cyrus_client_start or pni_cyrus_server_start
    // and the directory is already fixed - don't change
    if (sasl->config_dir && !pni_cyrus_config_dir) {
      pni_cyrus_config_dir = pn_strdup(sasl->config_dir);
    }

    pni_cyrus_client_start();
    result = pni_cyrus_client_init_rc;
    if (result!=SASL_OK) break;

    const sasl_callback_t *callbacks = sasl->username ? sasl->password ? pni_user_password_callbacks : pni_user_callbacks : NULL;
    result = sasl_client_new(amqp_service,
                             sasl->remote_fqdn,
                             NULL, NULL,
                             callbacks, 0,
                             &cyrus_conn);
    if (result!=SASL_OK) break;
    sasl->impl_context = cyrus_conn;

    sasl_security_properties_t secprops = {0};
    secprops.security_flags =
      ( sasl->allow_insecure_mechs ? 0 : SASL_SEC_NOPLAINTEXT ) |
      ( transport->auth_required ? SASL_SEC_NOANONYMOUS : 0 ) ;
    secprops.min_ssf = 0;
    secprops.max_ssf = 2048;
    secprops.maxbufsize = PN_SASL_MAX_BUFFSIZE;

    result = sasl_setprop(cyrus_conn, SASL_SEC_PROPS, &secprops);
    if (result!=SASL_OK) break;

    sasl_ssf_t ssf = sasl->external_ssf;
    result = sasl_setprop(cyrus_conn, SASL_SSF_EXTERNAL, &ssf);
    if (result!=SASL_OK) break;

    const char *extid = sasl->external_auth;
    if (extid) {
      result = sasl_setprop(cyrus_conn, SASL_AUTH_EXTERNAL, extid);
    }
  } while (false);
  cyrus_conn = (sasl_conn_t*) sasl->impl_context;
  return pni_check_sasl_result(cyrus_conn, result, transport);
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
            pni_sasl_set_desired_state(transport, SASL_RECVED_OUTCOME_FAIL);
            break;
    }
}

bool pni_init_server(pn_transport_t* transport)
{
  pni_sasl_t *sasl = transport->sasl;
  int result;
  sasl_conn_t *cyrus_conn = NULL;
  do {
    // If pni_cyrus_config_dir already set then we already called pni_cyrus_client_start or pni_cyrus_server_start
    // and the directory is already fixed - don't change
    if (sasl->config_dir && !pni_cyrus_config_dir) {
      pni_cyrus_config_dir = pn_strdup(sasl->config_dir);
    }

    // If pni_cyrus_config_name already set then we already called pni_cyrus_server_start
    // and the name is already fixed - don't change
    if (sasl->config_name && !pni_cyrus_config_name) {
      pni_cyrus_config_name = pn_strdup(sasl->config_name);
    }

    pni_cyrus_server_start();
    result = pni_cyrus_server_init_rc;
    if (result!=SASL_OK) break;

    result = sasl_server_new(amqp_service, NULL, NULL, NULL, NULL, NULL, 0, &cyrus_conn);
    if (result!=SASL_OK) break;
    sasl->impl_context = cyrus_conn;

    sasl_security_properties_t secprops = {0};
    secprops.security_flags =
      ( sasl->allow_insecure_mechs ? 0 : SASL_SEC_NOPLAINTEXT ) |
      ( transport->auth_required ? SASL_SEC_NOANONYMOUS : 0 ) ;
    secprops.min_ssf = 0;
    secprops.max_ssf = 2048;
    secprops.maxbufsize = PN_SASL_MAX_BUFFSIZE;

    result = sasl_setprop(cyrus_conn, SASL_SEC_PROPS, &secprops);
    if (result!=SASL_OK) break;

    sasl_ssf_t ssf = sasl->external_ssf;
    result = sasl_setprop(cyrus_conn, SASL_SSF_EXTERNAL, &ssf);
    if (result!=SASL_OK) break;

    const char *extid = sasl->external_auth;
    if (extid) {
    result = sasl_setprop(cyrus_conn, SASL_AUTH_EXTERNAL, extid);
    }
  } while (false);
  cyrus_conn = (sasl_conn_t*) sasl->impl_context;
  return pni_check_sasl_result(cyrus_conn, result, transport);
}

static int pni_wrap_server_start(pni_sasl_t *sasl, const char *mech_selected, const pn_bytes_t *in)
{
    int result;
    const char *out;
    unsigned outlen;
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)sasl->impl_context;
    const char *in_bytes = in->start;
    size_t in_size = in->size;
    // Interop hack for ANONYMOUS - some of the earlier versions of proton will send and no data
    // with an anonymous init because it is optional. It seems that Cyrus wants an empty string here
    // or it will challenge, which the earlier implementation is not prepared for.
    // However we can't just always use an empty string as the CRAM-MD5 mech won't allow any data in the server start
    if (!in_bytes && strcmp(mech_selected, "ANONYMOUS")==0) {
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
            if (transport->trace & PN_TRACE_DRV)
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

bool pni_sasl_impl_can_encrypt(pn_transport_t *transport)
{
  if (!transport->sasl->impl_context) return false;

  sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
  // Get SSF to find out if we need to encrypt or not
  const void* value;
  int r = sasl_getprop(cyrus_conn, SASL_SSF, &value);
  if (r != SASL_OK) {
    // TODO: Should log an error here too, maybe assert here
    return false;
  }
  int ssf = *(int *) value;
  if (ssf > 0) {
    return true;
  }
  return false;
}

ssize_t pni_sasl_impl_max_encrypt_size(pn_transport_t *transport)
{
  if (!transport->sasl->impl_context) return PN_ERR;

  sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
  const void* value;
  int r = sasl_getprop(cyrus_conn, SASL_MAXOUTBUF, &value);
  if (r != SASL_OK) {
    // TODO: Should log an error here too, maybe assert here
    return PN_ERR;
  }
  int outbuf_size = *(int *) value;
  return outbuf_size -
    // XXX: this  is a clientside workaround/hack to make GSSAPI work as the Cyrus SASL
    // GSSAPI plugin seems to return an incorrect value for the buffer size on the client
    // side, which is greater than the value returned on the server side. Actually using
    // the entire client side buffer will cause a server side error due to a buffer overrun.
    (transport->sasl->client? 60 : 0);
}

ssize_t pni_sasl_impl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  if ( in.size==0 ) return 0;
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
  const char *output;
  unsigned int outlen;
  int r = sasl_encode(cyrus_conn, in.start, in.size, &output, &outlen);
  if (outlen==0) return 0;
  if ( pni_check_sasl_result(cyrus_conn, r, transport) ) {
    *out = pn_bytes(outlen, output);
    return outlen;
  }
  return PN_ERR;
}

ssize_t pni_sasl_impl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  if ( in.size==0 ) return 0;
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
  const char *output;
  unsigned int outlen;
  int r = sasl_decode(cyrus_conn, in.start, in.size, &output, &outlen);
  if (outlen==0) return 0;
  if ( pni_check_sasl_result(cyrus_conn, r, transport) ) {
    *out = pn_bytes(outlen, output);
    return outlen;
  }
  return PN_ERR;
}

void pni_sasl_impl_free(pn_transport_t *transport)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)transport->sasl->impl_context;
    sasl_dispose(&cyrus_conn);
    transport->sasl->impl_context = cyrus_conn;
}

bool pn_sasl_extended(void)
{
  return true;
}
