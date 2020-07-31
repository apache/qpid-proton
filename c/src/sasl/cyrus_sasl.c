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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "core/logger_private.h"

#include "proton/sasl.h"
#include "proton/sasl_plugin.h"
#include "proton/transport.h"

#include <sasl/sasl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifndef CYRUS_SASL_MAX_BUFFSIZE
# define CYRUS_SASL_MAX_BUFFSIZE (32768) /* bytes */
#endif

// SASL implementation entry points
static void cyrus_sasl_prepare(pn_transport_t *transport);
static void cyrus_sasl_free(pn_transport_t *transport);
static const char *cyrus_sasl_list_mechs(pn_transport_t *transport);

static bool cyrus_sasl_init_server(pn_transport_t *transport);
static void cyrus_sasl_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv);
static void cyrus_sasl_process_response(pn_transport_t *transport, const pn_bytes_t *recv);

static bool cyrus_sasl_init_client(pn_transport_t *transport);
static bool cyrus_sasl_process_mechanisms(pn_transport_t *transport, const char *mechs);
static void cyrus_sasl_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv);
static void cyrus_sasl_process_outcome(pn_transport_t *transport, const pn_bytes_t *recv);

static bool cyrus_sasl_can_encrypt(pn_transport_t *transport);
static ssize_t cyrus_sasl_max_encrypt_size(pn_transport_t *transport);
static ssize_t cyrus_sasl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);
static ssize_t cyrus_sasl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);

static const pnx_sasl_implementation sasl_impl = {
    cyrus_sasl_free,

    cyrus_sasl_list_mechs,

    cyrus_sasl_init_server,
    cyrus_sasl_init_client,

    cyrus_sasl_prepare,

    cyrus_sasl_process_init,
    cyrus_sasl_process_response,

    cyrus_sasl_process_mechanisms,
    cyrus_sasl_process_challenge,
    cyrus_sasl_process_outcome,

    cyrus_sasl_can_encrypt,
    cyrus_sasl_max_encrypt_size,
    cyrus_sasl_encode,
    cyrus_sasl_decode
};

extern const pnx_sasl_implementation * const cyrus_sasl_impl;
const pnx_sasl_implementation * const cyrus_sasl_impl = &sasl_impl;

// If the version of Cyrus SASL is too early for sasl_client_done()/sasl_server_done()
// don't do any global clean up as it's not safe to use just sasl_done() for an
// executable that uses both client and server parts of Cyrus SASL, because it can't
// be called twice.
#if SASL_VERSION_FULL<0x020118
# define sasl_client_done()
# define sasl_server_done()
#endif

static const char amqp_service[] = "amqp";

static inline bool pni_check_result(sasl_conn_t *conn, int r, pn_transport_t *logger, const char* condition_name)
{
    if (r==SASL_OK) return true;

    const char* err = conn ? sasl_errdetail(conn) : sasl_errstring(r, NULL, NULL);
    pnx_sasl_error(logger, err, condition_name);
    return false;
}

static bool pni_check_io_result(sasl_conn_t *conn, int r, pn_transport_t *logger)
{
    return pni_check_result(conn, r, logger, "proton:io:sasl_error");
}

static bool pni_check_sasl_result(sasl_conn_t *conn, int r, pn_transport_t *logger)
{
    return pni_check_result(conn, r, logger, "amqp:unauthorized-access");
}

// Cyrus wrappers
static void pni_cyrus_interact(pn_transport_t *transport, sasl_interact_t *interact)
{
  for (sasl_interact_t *i = interact; i->id!=SASL_CB_LIST_END; i++) {
    switch (i->id) {
    case SASL_CB_USER: {
      const char *authzid = pnx_sasl_get_authorization(transport);
      i->result = authzid;
      i->len = authzid ? strlen(authzid) : 0;
      break;
    }
    case SASL_CB_AUTHNAME: {
      const char *username = pnx_sasl_get_username(transport);
      i->result = username;
      i->len = strlen(username);
      break;
    }
    case SASL_CB_PASS: {
      const char *password = pnx_sasl_get_password(transport);
      i->result = password;
      i->len = strlen(password);
      break;
    }
    default:
      pnx_sasl_logf(transport, PN_LEVEL_ERROR, "(%s): %s - %s", i->challenge, i->prompt, i->defresult);
    }
  }
}

const char *cyrus_sasl_list_mechs(pn_transport_t *transport)
{
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
  if (!cyrus_conn) return NULL;

  int count = 0;
  const char *result = NULL;
  int r = sasl_listmech(cyrus_conn, NULL, "", " ", "", &result, NULL, &count);
  pni_check_sasl_result(cyrus_conn, r, transport);
  return result;
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

static const sasl_callback_t pni_authzid_callbacks[] = {
  {SASL_CB_USER, NULL, NULL},
  {SASL_CB_LIST_END, NULL, NULL},
};

static int pni_authorize(sasl_conn_t *conn,
    void *context,
    const char *requested_user, unsigned rlen,
    const char *auth_identity, unsigned alen,
    const char *def_realm, unsigned urlen,
    struct propctx *propctx)
{
  PN_LOG_DEFAULT(PN_SUBSYSTEM_SASL, PN_LEVEL_TRACE, "Authorized: userid=%*s by authuser=%*s @ %*s",
    rlen, requested_user,
    alen, auth_identity,
    urlen, def_realm);
  return SASL_OK;
}

static const sasl_callback_t pni_server_callbacks[] = {
    {SASL_CB_PROXY_POLICY, (int(*)(void)) pni_authorize, NULL},
    {SASL_CB_LIST_END, NULL, NULL},
};

// Machinery to initialise the cyrus library only once even in a multithreaded environment
// Relies on pthreads.
static const char default_config_name[] = "proton-server";
static char *pni_cyrus_config_dir = NULL;
static char *pni_cyrus_config_name = NULL;
static pthread_mutex_t pni_cyrus_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool pni_cyrus_client_started = false;
static bool pni_cyrus_server_started = false;

bool pn_sasl_extended(void)
{
  return true;
}

void pn_sasl_config_name(pn_sasl_t *sasl0, const char *name)
{
    if (!pni_cyrus_config_name) {
      pni_cyrus_config_name = strdup(name);
    }
}

void pn_sasl_config_path(pn_sasl_t *sasl0, const char *dir)
{
  if (!pni_cyrus_config_dir) {
    pni_cyrus_config_dir = strdup(dir);
  }
}

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
  } else {
    char *config_dir = getenv("PN_SASL_CONFIG_PATH");
    if (config_dir) {
      result = sasl_set_path(SASL_PATH_TYPE_CONFIG, config_dir);
    }
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
  } else {
    char *config_dir = getenv("PN_SASL_CONFIG_PATH");
    if (config_dir) {
      result = sasl_set_path(SASL_PATH_TYPE_CONFIG, config_dir);
    }
  }
  if (result==SASL_OK) {
    result = sasl_server_init(pni_server_callbacks, pni_cyrus_config_name ? pni_cyrus_config_name : default_config_name);
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

void cyrus_sasl_prepare(pn_transport_t* transport)
{
}

bool cyrus_sasl_init_client(pn_transport_t* transport) {
  int result;
  sasl_conn_t *cyrus_conn = NULL;
  do {
    pni_cyrus_client_start();
    result = pni_cyrus_client_init_rc;
    if (result!=SASL_OK) break;

    const sasl_callback_t *callbacks =
      pnx_sasl_get_username(transport) ? (pnx_sasl_get_password(transport) ? pni_user_password_callbacks : pni_user_callbacks) :
      pnx_sasl_get_authorization(transport) ? pni_authzid_callbacks : NULL;
    result = sasl_client_new(amqp_service,
                             pnx_sasl_get_remote_fqdn(transport),
                             NULL, NULL,
                             callbacks, 0,
                             &cyrus_conn);
    if (result!=SASL_OK) break;
    pnx_sasl_set_context(transport, cyrus_conn);

    sasl_security_properties_t secprops = {0};
    secprops.security_flags =
      ( pnx_sasl_get_allow_insecure_mechanisms(transport) ? 0 : SASL_SEC_NOPLAINTEXT ) |
      ( pnx_sasl_get_authentication_required(transport) ? SASL_SEC_NOANONYMOUS : 0 ) ;
    secprops.min_ssf = 0;
    secprops.max_ssf = 2048;
    secprops.maxbufsize = CYRUS_SASL_MAX_BUFFSIZE;

    result = sasl_setprop(cyrus_conn, SASL_SEC_PROPS, &secprops);
    if (result!=SASL_OK) break;

    sasl_ssf_t ssf = pnx_sasl_get_external_ssf(transport);
    result = sasl_setprop(cyrus_conn, SASL_SSF_EXTERNAL, &ssf);
    if (result!=SASL_OK) break;

    const char *extid = pnx_sasl_get_external_username(transport);
    if (extid) {
      result = sasl_setprop(cyrus_conn, SASL_AUTH_EXTERNAL, extid);
    }
  } while (false);
  cyrus_conn = (sasl_conn_t*) pnx_sasl_get_context(transport);
  return pni_check_sasl_result(cyrus_conn, result, transport);
}

static int pni_wrap_client_start(pn_transport_t *transport, const char *mechs, const char **mechusing)
{
    int result;
    sasl_interact_t *client_interact=NULL;
    const char *out;
    unsigned outlen;

    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    do {

        result = sasl_client_start(cyrus_conn,
                                   mechs,
                                   &client_interact,
                                   &out, &outlen,
                                   mechusing);
        if (result==SASL_INTERACT) {
            pni_cyrus_interact(transport, client_interact);
        }
    } while (result==SASL_INTERACT);

    pnx_sasl_set_bytes_out(transport, pn_bytes(outlen, out));
    return result;
}

bool cyrus_sasl_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    const char *mech_selected;
    int result = pni_wrap_client_start(transport, mechs, &mech_selected);
    switch (result) {
        case SASL_OK:
        case SASL_CONTINUE:
          pnx_sasl_set_selected_mechanism(transport, mech_selected);
          pnx_sasl_set_desired_state(transport, SASL_POSTED_INIT);
          return true;
        case SASL_NOMECH:
        default:
          pni_check_sasl_result(cyrus_conn, result, transport);
          return false;
    }
}


static int pni_wrap_client_step(pn_transport_t *transport, const pn_bytes_t *in)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
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
            pni_cyrus_interact(transport, client_interact);
        }
    } while (result==SASL_INTERACT);

    pnx_sasl_set_bytes_out(transport, pn_bytes(outlen, out));
    return result;
}

void cyrus_sasl_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    int result = pni_wrap_client_step(transport, recv);
    switch (result) {
        case SASL_OK:
            // Potentially authenticated
        case SASL_CONTINUE:
            // Need to send a response
            pnx_sasl_set_desired_state(transport, SASL_POSTED_RESPONSE);
            break;
        default:
            pni_check_sasl_result(cyrus_conn, result, transport);

            // Failed somehow - equivalent to failing authentication
            pnx_sasl_set_failed(transport);
            pnx_sasl_set_desired_state(transport, SASL_RECVED_FAILURE);
            break;
    }
}

void cyrus_sasl_process_outcome(pn_transport_t* transport, const pn_bytes_t *recv)
{
}

bool cyrus_sasl_init_server(pn_transport_t* transport)
{
  int result;
  sasl_conn_t *cyrus_conn = NULL;
  do {
    pni_cyrus_server_start();
    result = pni_cyrus_server_init_rc;
    if (result!=SASL_OK) break;

    result = sasl_server_new(amqp_service, NULL, NULL, NULL, NULL, NULL, 0, &cyrus_conn);
    if (result!=SASL_OK) break;
    pnx_sasl_set_context(transport, cyrus_conn);

    sasl_security_properties_t secprops = {0};
    secprops.security_flags =
      ( pnx_sasl_get_allow_insecure_mechanisms(transport) ? 0 : SASL_SEC_NOPLAINTEXT ) |
      ( pnx_sasl_get_authentication_required(transport) ? SASL_SEC_NOANONYMOUS : 0 ) ;
    secprops.min_ssf = 0;
    secprops.max_ssf = 2048;
    secprops.maxbufsize = CYRUS_SASL_MAX_BUFFSIZE;

    result = sasl_setprop(cyrus_conn, SASL_SEC_PROPS, &secprops);
    if (result!=SASL_OK) break;

    sasl_ssf_t ssf = pnx_sasl_get_external_ssf(transport);
    result = sasl_setprop(cyrus_conn, SASL_SSF_EXTERNAL, &ssf);
    if (result!=SASL_OK) break;

    const char *extid = pnx_sasl_get_external_username(transport);
    if (extid) {
    result = sasl_setprop(cyrus_conn, SASL_AUTH_EXTERNAL, extid);
    }
  } while (false);
  cyrus_conn = (sasl_conn_t*) pnx_sasl_get_context(transport);
  if (pni_check_sasl_result(cyrus_conn, result, transport)) {
      // Setup to send SASL mechanisms frame
      pnx_sasl_set_desired_state(transport, SASL_POSTED_MECHANISMS);
      return true;
  } else {
      return false;
  }
}

static int pni_wrap_server_start(pn_transport_t *transport, const char *mech_selected, const pn_bytes_t *in)
{
    int result;
    const char *out;
    unsigned outlen;
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    const char *in_bytes = in->start;
    size_t in_size = in->size;
    // Interop hack for ANONYMOUS - some of the earlier versions of proton will send and no data
    // with an anonymous init because it is optional. It seems that Cyrus wants an empty string here
    // or it will challenge, which the earlier implementation is not prepared for.
    // However we can't just always use an empty string as the CRAM-MD5 mech won't allow any data in the server start
    if (!in_bytes && strcmp(mech_selected, "ANONYMOUS")==0) {
        in_bytes = "";
        in_size = 0;
    } else if (in_bytes && strcmp(mech_selected, "CRAM-MD5")==0) {
        in_bytes = 0;
        in_size = 0;
    }
    result = sasl_server_start(cyrus_conn,
                               mech_selected,
                               in_bytes, in_size,
                               &out, &outlen);

    pnx_sasl_set_bytes_out(transport, pn_bytes(outlen, out));
    return result;
}

static void pni_process_server_result(pn_transport_t *transport, int result)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    switch (result) {
        case SASL_OK: {
            // Authenticated
            // Get username from SASL
            const void* authcid;
            sasl_getprop(cyrus_conn, SASL_AUTHUSER, &authcid);
            // Get authzid from SASL
            const void* authzid;
            sasl_getprop(cyrus_conn, SASL_USERNAME, &authzid);
            pnx_sasl_set_succeeded(transport, (const char*) authcid, (const char*) authzid);
            pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
            break;
        }
        case SASL_CONTINUE:
            // Need to send a challenge
            pnx_sasl_set_desired_state(transport, SASL_POSTED_CHALLENGE);
            break;
        default:
            pni_check_sasl_result(cyrus_conn, result, transport);

            // Failed to authenticate
            pnx_sasl_set_failed(transport);
            pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
            break;
    }
}

void cyrus_sasl_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
    int result = pni_wrap_server_start(transport, mechanism, recv);
    pni_process_server_result(transport, result);
}

static int pni_wrap_server_step(pn_transport_t *transport, const pn_bytes_t *in)
{
    int result;
    const char *out;
    unsigned outlen = 0; // Initialise to defend against buggy cyrus mech plugins
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    result = sasl_server_step(cyrus_conn,
                              in->start, in->size,
                              &out, &outlen);

    pnx_sasl_set_bytes_out(transport, pn_bytes(outlen, outlen ? out : NULL));
    return result;
}

void cyrus_sasl_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
    int result = pni_wrap_server_step(transport, recv);
    pni_process_server_result(transport, result);
}

bool cyrus_sasl_can_encrypt(pn_transport_t *transport)
{
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
  if (!cyrus_conn) return false;

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

ssize_t cyrus_sasl_max_encrypt_size(pn_transport_t *transport)
{
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
  if (!cyrus_conn) return PN_ERR;

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
    (pnx_sasl_is_client(transport)? 60 : 0);
}

ssize_t cyrus_sasl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  if ( in.size==0 ) return 0;
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
  const char *output;
  unsigned int outlen;
  int r = sasl_encode(cyrus_conn, in.start, in.size, &output, &outlen);
  if (outlen==0) return 0;
  if ( pni_check_io_result(cyrus_conn, r, transport) ) {
    *out = pn_bytes(outlen, output);
    return outlen;
  }
  return PN_ERR;
}

ssize_t cyrus_sasl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
  if ( in.size==0 ) return 0;
  sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
  const char *output;
  unsigned int outlen;
  int r = sasl_decode(cyrus_conn, in.start, in.size, &output, &outlen);
  if (outlen==0) return 0;
  if ( pni_check_io_result(cyrus_conn, r, transport) ) {
    *out = pn_bytes(outlen, output);
    return outlen;
  }
  return PN_ERR;
}

void cyrus_sasl_free(pn_transport_t *transport)
{
    sasl_conn_t *cyrus_conn = (sasl_conn_t*)pnx_sasl_get_context(transport);
    sasl_dispose(&cyrus_conn);
    pnx_sasl_set_context(transport, cyrus_conn);
}
