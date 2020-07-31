#ifndef PROTON_SASL_PLUGIN_H
#define PROTON_SASL_PLUGIN_H 1

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

#include <proton/import_export.h>
#include <proton/logger.h>
#include <proton/type_compat.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL */

/*
  Internal SASL authenticator interface: These are the entry points to a SASL implementations

  Free up all data structures allocated by the SASL implementation
  void free(pn_transport_t *transport);

  Return space separated list of supported mechanisms (client and server)
  If the returned string is dynamically allocated by the SASL implemetation
  it must stay valid until the free entry point is called.
  const char *list_mechanisms(pn_transport_t *transport);

  Initialise for either client or server (can't call both for a
  given transport/connection):
  bool init_server(pn_transport_t *transport);
  bool init_client(pn_transport_t *transport);

  Writing:
  void prepare_write(pn_transport_t *transport);

  Reading:
  Server side (process server SASL messages):
  void process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv);
  void process_response(pn_transport_t *transport, const pn_bytes_t *recv);

  Client side (process client SASL messages)
  bool process_mechanisms(pn_transport_t *transport, const char *mechs);
  void process_challenge(pn_transport_t *transport, const pn_bytes_t *recv);
  void process_outcome(pn_transport_t *transport);

  Security layer interface (active after SASL succeeds)
  bool    can_encrypt(pn_transport_t *transport);
  ssize_t max_encrypt_size(pn_transport_t *transport);
  ssize_t encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);
  ssize_t decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);
*/

typedef struct pnx_sasl_implementation
{
    void (*free)(pn_transport_t *transport);

    const char*  (*list_mechanisms)(pn_transport_t *transport);

    bool (*init_server)(pn_transport_t *transport);
    bool (*init_client)(pn_transport_t *transport);

    void (*prepare_write)(pn_transport_t *transport);

    void (*process_init)(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv);
    void (*process_response)(pn_transport_t *transport, const pn_bytes_t *recv);

    bool (*process_mechanisms)(pn_transport_t *transport, const char *mechs);
    void (*process_challenge)(pn_transport_t *transport, const pn_bytes_t *recv);
    void (*process_outcome)(pn_transport_t *transport, const pn_bytes_t *recv);

    bool    (*can_encrypt)(pn_transport_t *transport);
    ssize_t (*max_encrypt_size)(pn_transport_t *transport);
    ssize_t (*encode)(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);
    ssize_t (*decode)(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out);

} pnx_sasl_implementation;

/* Shared SASL API used by the actual SASL authenticators */
enum pnx_sasl_state {
  SASL_NONE,
  SASL_POSTED_INIT,
  SASL_POSTED_MECHANISMS,
  SASL_POSTED_RESPONSE,
  SASL_POSTED_CHALLENGE,
  SASL_RECVED_SUCCESS,
  SASL_RECVED_FAILURE,
  SASL_POSTED_OUTCOME,
  SASL_ERROR
};

/* APIs used by sasl implementations */
PN_EXTERN void  pnx_sasl_logf(pn_transport_t *transport, pn_log_level_t level, const char *format, ...);
PN_EXTERN void  pnx_sasl_error(pn_transport_t *transport, const char* err, const char* condition_name);

PN_EXTERN void *pnx_sasl_get_context(pn_transport_t *transport);
PN_EXTERN void  pnx_sasl_set_context(pn_transport_t *transport, void *context);

PN_EXTERN bool  pnx_sasl_is_client(pn_transport_t *transport);
PN_EXTERN bool  pnx_sasl_is_mechanism_included(pn_transport_t *transport, pn_bytes_t s);
PN_EXTERN bool  pnx_sasl_is_transport_encrypted(pn_transport_t *transport);
PN_EXTERN bool  pnx_sasl_get_allow_insecure_mechanisms(pn_transport_t *transport);
PN_EXTERN bool  pnx_sasl_get_authentication_required(pn_transport_t *transport);
PN_EXTERN const char *pnx_sasl_get_external_username(pn_transport_t *transport);
PN_EXTERN int   pnx_sasl_get_external_ssf(pn_transport_t *transport);

PN_EXTERN const char *pnx_sasl_get_username(pn_transport_t *transport);
PN_EXTERN const char *pnx_sasl_get_password(pn_transport_t *transport);
PN_EXTERN const char *pnx_sasl_get_authorization(pn_transport_t *transport);
PN_EXTERN void  pnx_sasl_clear_password(pn_transport_t *transport);
PN_EXTERN const char *pnx_sasl_get_remote_fqdn(pn_transport_t *transport);
PN_EXTERN const char *pnx_sasl_get_selected_mechanism(pn_transport_t *transport);

PN_EXTERN void  pnx_sasl_set_bytes_out(pn_transport_t *transport, pn_bytes_t bytes);
PN_EXTERN void  pnx_sasl_set_desired_state(pn_transport_t *transport, enum pnx_sasl_state desired_state);
PN_EXTERN void  pnx_sasl_set_selected_mechanism(pn_transport_t *transport, const char *mechanism);
PN_EXTERN void  pnx_sasl_set_local_hostname(pn_transport_t * transport, const char * fqdn);
PN_EXTERN void  pnx_sasl_set_succeeded(pn_transport_t *transport, const char *username, const char *authzid);
PN_EXTERN void  pnx_sasl_set_failed(pn_transport_t *transport);

PN_EXTERN void  pnx_sasl_set_implementation(pn_transport_t *transport, const pnx_sasl_implementation *impl, void *context);
PN_EXTERN void  pnx_sasl_set_default_implementation(const pnx_sasl_implementation *impl);

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* sasl_plugin.h */
