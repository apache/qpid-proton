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

#include "core/autodetect.h"
#include "core/dispatch_actions.h"
#include "core/engine-internal.h"
#include "core/util.h"
#include "platform/platform_fmt.h"
#include "protocol.h"

#include "proton/ssl.h"
#include "proton/types.h"

#include <assert.h>

// Machinery to allow plugin SASL implementations
// Change this to &default_sasl_impl when we change cyrus to opt in
static const pnx_sasl_implementation *global_sasl_impl = NULL;

// List of SASL mechansms to exclude by default as they cause user pain
static const char pni_excluded_mechs[] = "GSSAPI GSS-SPNEGO GS2-KRB5 GS2-IAKERB";

//-----------------------------------------------------------------------------
// pnx_sasl: API for SASL implementations to use

void pnx_sasl_logf(pn_transport_t *logger, pn_log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (PN_SHOULD_LOG(&logger->logger, PN_SUBSYSTEM_SASL, level))
        pni_logger_vlogf(&logger->logger, PN_SUBSYSTEM_SASL, level, fmt, ap);
    va_end(ap);
}

void pnx_sasl_error(pn_transport_t *logger, const char* err, const char* condition_name)
{
    pnx_sasl_logf(logger, PN_LEVEL_ERROR, "sasl error: %s", err);
    pn_condition_t* c = pn_transport_condition(logger);
    pn_condition_set_name(c, condition_name);
    pn_condition_set_description(c, err);
}

void *pnx_sasl_get_context(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->impl_context : NULL;
}

void  pnx_sasl_set_context(pn_transport_t *transport, void *context)
{
  if (transport->sasl) transport->sasl->impl_context = context;
}

bool  pnx_sasl_is_client(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->client : false;
}

bool  pnx_sasl_is_transport_encrypted(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->external_ssf>0 : false;
}

bool  pnx_sasl_get_allow_insecure_mechanisms(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->allow_insecure_mechs : false;
}

bool  pnx_sasl_get_authentication_required(pn_transport_t *transport)
{
  return transport->auth_required;
}

const char *pnx_sasl_get_username(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->username : NULL;
}

const char *pnx_sasl_get_authorization(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->authzid : NULL;
}

const char *pnx_sasl_get_external_username(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->external_auth : NULL;
}

int   pnx_sasl_get_external_ssf(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->external_ssf : 0;
}

const char *pnx_sasl_get_password(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->password : NULL;
}

void  pnx_sasl_clear_password(pn_transport_t *transport)
{
  if (transport->sasl) {
    char *password = transport->sasl->password;
    free(memset(password, 0, strlen(password)));
    transport->sasl->password = NULL;
  }
}

const char *pnx_sasl_get_remote_fqdn(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->remote_fqdn : NULL;
}

const char *pnx_sasl_get_selected_mechanism(pn_transport_t *transport)
{
  return transport->sasl ? transport->sasl->selected_mechanism : NULL;
}

void  pnx_sasl_set_bytes_out(pn_transport_t *transport, pn_bytes_t bytes)
{
  if (transport->sasl) {
    transport->sasl->bytes_out = bytes;
  }
}

void  pnx_sasl_set_selected_mechanism(pn_transport_t *transport, const char *mechanism)
{
  if (transport->sasl) {
    transport->sasl->selected_mechanism = pn_strdup(mechanism);
  }
}

void  pnx_sasl_set_succeeded(pn_transport_t *transport, const char *username, const char *authzid)
{
  if (transport->sasl) {
    transport->sasl->username = username;
    transport->sasl->authzid = authzid;
    transport->sasl->outcome = PN_SASL_OK;
    transport->authenticated = true;

    if (authzid) {
      PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_INFO, "Authenticated user: %s for %s with mechanism %s",
             username, authzid, transport->sasl->selected_mechanism);
    } else {
      PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_INFO, "Authenticated user: %s with mechanism %s",
             username, transport->sasl->selected_mechanism);
    }
  }
}

void  pnx_sasl_set_failed(pn_transport_t *transport)
{
  if (transport->sasl) {
    transport->sasl->outcome = PN_SASL_AUTH;
  }
}

void pnx_sasl_set_implementation(pn_transport_t *transport, const pnx_sasl_implementation *i, void* context)
{
  transport->sasl->impl = i;
  transport->sasl->impl_context = context;
}

void pnx_sasl_set_default_implementation(const pnx_sasl_implementation* impl)
{
  global_sasl_impl = impl;
}

// Aliases for linkage compatibility in face of renamings
PN_EXTERN bool  pnx_sasl_is_included_mech(pn_transport_t *transport, pn_bytes_t s)
{ return pnx_sasl_is_mechanism_included(transport, s); }
PN_EXTERN bool  pnx_sasl_get_allow_insecure_mechs(pn_transport_t *transport)
{ return pnx_sasl_get_allow_insecure_mechanisms(transport); }
PN_EXTERN bool  pnx_sasl_get_auth_required(pn_transport_t *transport)
{ return pnx_sasl_get_authentication_required(transport); }
PN_EXTERN void  pnx_sasl_succeed_authentication(pn_transport_t *transport, const char *username, const char *authzid)
{ pnx_sasl_set_succeeded(transport, username, authzid);}
PN_EXTERN void  pnx_sasl_fail_authentication(pn_transport_t *transport)
{pnx_sasl_set_failed(transport); }

//-----------------------------------------------------------------------------
// pni_sasl_impl: Abstract the entry points to the SASL implementation (virtual function calls)

static inline void pni_sasl_impl_free(pn_transport_t *transport)
{
  transport->sasl->impl->free(transport);
}

static inline const char *pni_sasl_impl_list_mechs(pn_transport_t *transport)
{
  return transport->sasl->impl->list_mechanisms(transport);
}

static inline bool pni_sasl_impl_init_server(pn_transport_t *transport)
{
  return transport->sasl->impl->init_server(transport);
}

static inline void pni_sasl_impl_prepare_write(pn_transport_t *transport)
{
  transport->sasl->impl->prepare_write(transport);
}

static inline void pni_sasl_impl_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
  transport->sasl->impl->process_init(transport, mechanism, recv);
}

static inline void pni_sasl_impl_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
  transport->sasl->impl->process_response(transport, recv);
}

static inline bool pni_sasl_impl_init_client(pn_transport_t *transport)
{
  return transport->sasl->impl->init_client(transport);
}

static inline bool pni_sasl_impl_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
  return transport->sasl->impl->process_mechanisms(transport, mechs);
}

static inline void pni_sasl_impl_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
  transport->sasl->impl->process_challenge(transport, recv);
}

static inline void pni_sasl_impl_process_outcome(pn_transport_t *transport, const pn_bytes_t *recv)
{
  transport->sasl->impl->process_outcome(transport, recv);
}

static inline bool pni_sasl_impl_can_encrypt(pn_transport_t *transport)
{
    return transport->sasl->impl->can_encrypt(transport);
}

static inline ssize_t pni_sasl_impl_max_encrypt_size(pn_transport_t *transport)
{
    return transport->sasl->impl->max_encrypt_size(transport);
}

static inline ssize_t pni_sasl_impl_encode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
    return transport->sasl->impl->encode(transport, in, out);
}

static inline ssize_t pni_sasl_impl_decode(pn_transport_t *transport, pn_bytes_t in, pn_bytes_t *out)
{
    return transport->sasl->impl->decode(transport, in, out);
}

//-----------------------------------------------------------------------------
// General SASL implementation

static inline pni_sasl_t *get_sasl_internal(pn_sasl_t *sasl)
{
  // The external pn_sasl_t is really a pointer to the internal pni_transport_t
  return sasl ? ((pn_transport_t *)sasl)->sasl : NULL;
}

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available);
static ssize_t pn_input_read_sasl(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_input_read_sasl_encrypt(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_output_write_sasl_header(pn_transport_t* transport, unsigned int layer, char* bytes, size_t size);
static ssize_t pn_output_write_sasl(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);
static ssize_t pn_output_write_sasl_encrypt(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);
static void pn_error_sasl(pn_transport_t* transport, unsigned int layer);

const pn_io_layer_t sasl_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl_header,
    pn_error_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_write_header_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl_header,
    pn_error_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_read_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl,
    pn_error_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl,
    pn_error_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_encrypt_layer = {
    pn_input_read_sasl_encrypt,
    pn_output_write_sasl_encrypt,
    NULL,
    NULL,
    NULL
};

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")
#define SASL_HEADER_LEN 8

static bool pni_sasl_is_server_state(enum pnx_sasl_state state)
{
  return state==SASL_NONE
      || state==SASL_POSTED_MECHANISMS
      || state==SASL_POSTED_CHALLENGE
      || state==SASL_POSTED_OUTCOME
      || state==SASL_ERROR;
}

static bool pni_sasl_is_client_state(enum pnx_sasl_state state)
{
  return state==SASL_NONE
      || state==SASL_POSTED_INIT
      || state==SASL_POSTED_RESPONSE
      || state==SASL_RECVED_SUCCESS
      || state==SASL_RECVED_FAILURE
      || state==SASL_ERROR;
}

static bool pni_sasl_is_final_input_state(pni_sasl_t *sasl)
{
  enum pnx_sasl_state desired_state = sasl->desired_state;
  return desired_state==SASL_RECVED_SUCCESS
      || desired_state==SASL_RECVED_FAILURE
      || desired_state==SASL_ERROR
      || desired_state==SASL_POSTED_OUTCOME;
}

static bool pni_sasl_is_final_output_state(pni_sasl_t *sasl)
{
  enum pnx_sasl_state last_state = sasl->last_state;
  enum pnx_sasl_state desired_state = sasl->desired_state;
  return (desired_state==SASL_RECVED_SUCCESS && last_state>=SASL_POSTED_INIT)
      || last_state==SASL_RECVED_SUCCESS
      || last_state==SASL_RECVED_FAILURE
      || last_state==SASL_ERROR
      || last_state==SASL_POSTED_OUTCOME;
}

static void pni_emit(pn_transport_t *transport)
{
  if (transport->connection && transport->connection->collector) {
    pn_collector_t *collector = transport->connection->collector;
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT);
  }
}

void pnx_sasl_set_desired_state(pn_transport_t *transport, enum pnx_sasl_state desired_state)
{
  pni_sasl_t *sasl = transport->sasl;
  if (sasl->last_state > desired_state) {
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_ERROR, "Trying to send SASL frame (%d), but illegal: already in later state (%d)", desired_state, sasl->last_state);
  } else if (sasl->client && !pni_sasl_is_client_state(desired_state)) {
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_ERROR, "Trying to send server SASL frame (%d) on a client", desired_state);
  } else if (!sasl->client && !pni_sasl_is_server_state(desired_state)) {
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_ERROR, "Trying to send client SASL frame (%d) on a server", desired_state);
  } else {
    // If we need to repeat CHALLENGE or RESPONSE frames adjust current state to seem
    // like they haven't been sent yet
    if (sasl->last_state==desired_state && desired_state==SASL_POSTED_RESPONSE) {
      sasl->last_state = SASL_POSTED_INIT;
    }
    if (sasl->last_state==desired_state && desired_state==SASL_POSTED_CHALLENGE) {
      sasl->last_state = SASL_POSTED_MECHANISMS;
    }
    bool changed = sasl->desired_state != desired_state;
    sasl->desired_state = desired_state;
    // Don't emit transport event on error as there will be a TRANSPORT_ERROR event
    if (desired_state != SASL_ERROR && changed) pni_emit(transport);
  }
}

// Look for symbol in the mech include list - not particularly efficient,
// but probably not used enough to matter.
//
// Note that if there is no inclusion list then every mech is implicitly included.
static bool pni_sasl_included_mech(const char *included_mech_list, pn_bytes_t s)
{
  const char * end_list = included_mech_list+strlen(included_mech_list);
  size_t len = s.size;
  const char *c = included_mech_list;
  while (c!=NULL) {
    // If there are not enough chars left in the list no matches
    if ((ptrdiff_t)len > end_list-c) return false;

    // Is word equal with a space or end of string afterwards?
    if (pn_strncasecmp(c, s.start, len)==0 && (c[len]==' ' || c[len]==0) ) return true;

    c = strchr(c, ' ');
    c = c ? c+1 : NULL;
  }
  return false;
}

static bool pni_sasl_server_included_mech(const char *included_mech_list, pn_bytes_t s)
{
  if (!included_mech_list) return true;

  return pni_sasl_included_mech(included_mech_list, s);
}

static bool pni_sasl_client_included_mech(const char *included_mech_list, pn_bytes_t s)
{
  if (!included_mech_list) return !pni_sasl_included_mech(pni_excluded_mechs, s);

  return pni_sasl_included_mech(included_mech_list, s);
}

// Look for symbol in the mech include list - plugin API version
//
// Note that if there is no inclusion list then every mech is implicitly included.
bool pnx_sasl_is_mechanism_included(pn_transport_t* transport, pn_bytes_t s)
{
  return pni_sasl_server_included_mech(transport->sasl->included_mechanisms, s);
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
        if (pni_sasl_server_included_mech(included_mechs, pn_bytes(end-start, start))) {
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
    if (pni_sasl_server_included_mech(included_mechs, pn_bytes(end-start, start))) {
      mechs[(*count)++] = start;
    }
  }
}

// Post SASL frame
static void pni_post_sasl_frame(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  pn_bytes_t out = sasl->bytes_out;
  enum pnx_sasl_state desired_state = sasl->desired_state;
  while (sasl->desired_state > sasl->last_state) {
    switch (desired_state) {
    case SASL_POSTED_INIT:
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[szS]", SASL_INIT, sasl->selected_mechanism,
                    out.size, out.start, sasl->local_fqdn);
      pni_emit(transport);
      break;
    case SASL_POSTED_MECHANISMS: {
      // TODO(PROTON-2122) Replace magic number 32 with dynamically sized memory
      char *mechs[32];
      char *mechlist = pn_strdup(pni_sasl_impl_list_mechs(transport));

      int count = 0;
      if (mechlist) {
        pni_split_mechs(mechlist, sasl->included_mechanisms, mechs, &count);
      }

      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[@T[*s]]", SASL_MECHANISMS, PN_SYMBOL, count, mechs);
      free(mechlist);
      pni_emit(transport);
      break;
    }
    case SASL_POSTED_RESPONSE:
      if (sasl->last_state != SASL_POSTED_RESPONSE) {
        pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[Z]", SASL_RESPONSE, out.size, out.start);
        pni_emit(transport);
      }
      break;
    case SASL_POSTED_CHALLENGE:
      if (sasl->last_state < SASL_POSTED_MECHANISMS) {
        desired_state = SASL_POSTED_MECHANISMS;
        continue;
      } else if (sasl->last_state != SASL_POSTED_CHALLENGE) {
        pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[Z]", SASL_CHALLENGE, out.size, out.start);
        pni_emit(transport);
      }
      break;
    case SASL_POSTED_OUTCOME:
      if (sasl->last_state < SASL_POSTED_MECHANISMS) {
        desired_state = SASL_POSTED_MECHANISMS;
        continue;
      }
      pn_post_frame(transport, SASL_FRAME_TYPE, 0, "DL[Bz]", SASL_OUTCOME, sasl->outcome, out.size, out.start);
      pni_emit(transport);
      if (sasl->outcome!=PN_SASL_OK) {
        pn_do_error(transport, "amqp:unauthorized-access", "Failed to authenticate client [mech=%s]",
                    transport->sasl->selected_mechanism ? transport->sasl->selected_mechanism : "none");
        desired_state = SASL_ERROR;
      }
      break;
    case SASL_RECVED_SUCCESS:
      if (sasl->last_state < SASL_POSTED_INIT) {
        desired_state = SASL_POSTED_INIT;
        continue;
      }
      break;
    case SASL_RECVED_FAILURE:
      pn_do_error(transport, "amqp:unauthorized-access", "Authentication failed [mech=%s]",
                  transport->sasl->selected_mechanism ? transport->sasl->selected_mechanism : "none");
      desired_state = SASL_ERROR;
      break;
    case SASL_ERROR:
      break;
    case SASL_NONE:
      return;
    }
    sasl->last_state = desired_state;
    desired_state = sasl->desired_state;
  }
}

static void pn_error_sasl(pn_transport_t* transport, unsigned int layer)
{
  transport->close_sent = true;
  pnx_sasl_set_desired_state(transport, SASL_ERROR);
}

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  bool eos = transport->tail_closed;
  if (eos && available==0) {
    pn_do_error(transport, "amqp:connection:framing-error",
                "Expected SASL protocol header: no protocol header found (connection aborted)");
    pn_set_error_layer(transport);
    return PN_EOS;
  }
  pni_protocol_type_t protocol = pni_sniff_header(bytes, available);
  switch (protocol) {
  case PNI_PROTOCOL_AMQP_SASL:
    transport->present_layers |= LAYER_AMQPSASL;
    if (transport->io_layers[layer] == &sasl_read_header_layer) {
        transport->io_layers[layer] = &sasl_layer;
    } else {
        transport->io_layers[layer] = &sasl_write_header_layer;
    }
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_FRAME, "  <- %s", "SASL");
    pni_sasl_set_external_security(transport, pn_ssl_get_ssf((pn_ssl_t*)transport), pn_ssl_get_remote_subject((pn_ssl_t*)transport));
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
              "Expected SASL protocol header got: %s ['%s']%s", pni_protocol_name(protocol), quoted,
              !eos ? "" : " (connection aborted)");
  pn_set_error_layer(transport);
  return PN_EOS;
}

static void pni_sasl_start_server_if_needed(pn_transport_t *transport)
{
  pni_sasl_t *sasl = transport->sasl;
  if (!sasl->client && sasl->desired_state<SASL_POSTED_MECHANISMS) {
    pni_sasl_impl_init_server(transport);
  }
}

static ssize_t pn_input_read_sasl(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  pni_sasl_t *sasl = transport->sasl;

  bool eos = transport->tail_closed;
  if (eos) {
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    pn_set_error_layer(transport);
    return PN_EOS;
  }

  pni_sasl_start_server_if_needed(transport);

  if (!pni_sasl_is_final_input_state(sasl)) {
    ssize_t n = pn_dispatcher_input(transport, bytes, available, false, &transport->halt);
    if (n < 0 || transport->close_rcvd) {
      return PN_EOS;
    } else {
      return n;
    }
  }

  if (!pni_sasl_is_final_output_state(sasl)) {
    return pni_passthru_layer.process_input(transport, layer, bytes, available);
  }

  if (pni_sasl_impl_can_encrypt(transport)) {
    sasl->max_encrypt_size = pni_sasl_impl_max_encrypt_size(transport);
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_INFO, "Encryption enabled: buffer=%" PN_ZU, sasl->max_encrypt_size);
    transport->io_layers[layer] = &sasl_encrypt_layer;
  } else {
    transport->io_layers[layer] = &pni_passthru_layer;
  }
  return transport->io_layers[layer]->process_input(transport, layer, bytes, available);
}

static ssize_t pn_input_read_sasl_encrypt(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  pn_buffer_t *in = transport->sasl->decoded_buffer;
  const size_t max_buffer = transport->sasl->max_encrypt_size;
  for (size_t processed = 0; processed<available;) {
    pn_bytes_t decoded = pn_bytes(0, NULL);
    size_t decode_size = (available-processed)<=max_buffer?(available-processed):max_buffer;
    ssize_t size = pni_sasl_impl_decode(transport, pn_bytes(decode_size, bytes+processed), &decoded);
    if (size<0) return size;
    if (size>0) {
      size = pn_buffer_append(in, decoded.start, decoded.size);
      if (size) return size;
    }
    processed += decode_size;
  }
  pn_bytes_t decoded = pn_buffer_bytes(in);
  size_t processed_size = 0;
  while (processed_size < decoded.size) {
    ssize_t size = pni_passthru_layer.process_input(transport, layer, decoded.start+processed_size, decoded.size-processed_size);
    if (size==0) break;
    if (size<0) return size;
    pn_buffer_trim(in, size, 0);
    processed_size += size;
  }
  return available;
}

static ssize_t pn_output_write_sasl_header(pn_transport_t *transport, unsigned int layer, char *bytes, size_t size)
{
  PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_FRAME, "  -> %s", "SASL");
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

  // this accounts for when pn_do_error is invoked, e.g. by idle timeout
  if (transport->close_sent) return PN_EOS;

  pni_sasl_start_server_if_needed(transport);

  pni_sasl_impl_prepare_write(transport);

  pni_post_sasl_frame(transport);

  if (pn_buffer_size(transport->output_buffer) != 0 || !pni_sasl_is_final_output_state(sasl)) {
    return pn_dispatcher_output(transport, bytes, available);
  }

  if (!pni_sasl_is_final_input_state(sasl)) {
    return pni_passthru_layer.process_output(transport, layer, bytes, available );
  }

  // We only get here if there is nothing to output and we're in a final state
  if (sasl->outcome != PN_SASL_OK) {
    return PN_EOS;
  }

  // We know that auth succeeded or we're not in final input state
  if (pni_sasl_impl_can_encrypt(transport)) {
    sasl->max_encrypt_size = pni_sasl_impl_max_encrypt_size(transport);
    PN_LOG(&transport->logger, PN_SUBSYSTEM_SASL, PN_LEVEL_INFO, "Encryption enabled: buffer=%" PN_ZU, sasl->max_encrypt_size);
    transport->io_layers[layer] = &sasl_encrypt_layer;
  } else {
    transport->io_layers[layer] = &pni_passthru_layer;
  }
  return transport->io_layers[layer]->process_output(transport, layer, bytes, available);
}

static ssize_t pn_output_write_sasl_encrypt(pn_transport_t* transport, unsigned int layer, char* bytes, size_t available)
{
  ssize_t clear_size = pni_passthru_layer.process_output(transport, layer, bytes, available );
  if (clear_size<0) return clear_size;

  const ssize_t max_buffer = transport->sasl->max_encrypt_size;
  pn_buffer_t *out = transport->sasl->encoded_buffer;
  for (ssize_t processed = 0; processed<clear_size;) {
    pn_bytes_t encoded = pn_bytes(0, NULL);
    ssize_t encode_size = (clear_size-processed)<=max_buffer?(clear_size-processed):max_buffer;
    ssize_t size = pni_sasl_impl_encode(transport, pn_bytes(encode_size, bytes+processed), &encoded);
    if (size<0) return size;
    if (size>0) {
      size = pn_buffer_append(out, encoded.start, encoded.size);
      if (size) return size;
    }
    processed += encode_size;
  }
  ssize_t size = pn_buffer_get(out, 0, available, bytes);
  pn_buffer_trim(out, size, 0);
  return size;
}

pn_sasl_t *pn_sasl(pn_transport_t *transport)
{
  if (!transport->sasl) {
    pni_sasl_t *sasl = (pni_sasl_t *) malloc(sizeof(pni_sasl_t));

    sasl->impl_context = NULL;
    // Change this to just global_sasl_impl when we make cyrus opt in
    sasl->impl = global_sasl_impl ? global_sasl_impl : cyrus_sasl_impl ? cyrus_sasl_impl : &default_sasl_impl;
    sasl->client = !transport->server;
    sasl->selected_mechanism = NULL;
    sasl->included_mechanisms = NULL;
    sasl->username = NULL;
    sasl->authzid = NULL;
    sasl->password = NULL;
    sasl->remote_fqdn = NULL;
    sasl->local_fqdn = NULL;
    sasl->external_auth = NULL;
    sasl->external_ssf = 0;
    sasl->outcome = PN_SASL_NONE;
    sasl->decoded_buffer = pn_buffer(0);
    sasl->encoded_buffer = pn_buffer(0);
    sasl->bytes_out.size = 0;
    sasl->bytes_out.start = NULL;
    sasl->desired_state = SASL_NONE;
    sasl->last_state = SASL_NONE;
    sasl->allow_insecure_mechs = false;

    transport->sasl = sasl;
  }

  // The actual external pn_sasl_t pointer is a pointer to its enclosing pn_transport_t
  return (pn_sasl_t *)transport;
}

void pn_sasl_free(pn_transport_t *transport)
{
  if (transport) {
    pni_sasl_t *sasl = transport->sasl;
    if (sasl) {
      free(sasl->selected_mechanism);
      free(sasl->included_mechanisms);
      free(sasl->password);
      free(sasl->external_auth);
      free(sasl->local_fqdn);

      if (sasl->impl_context) {
        pni_sasl_impl_free(transport);
      }
      pn_buffer_free(sasl->decoded_buffer);
      pn_buffer_free(sasl->encoded_buffer);
      free(sasl);
    }
  }
}

void pni_sasl_set_remote_hostname(pn_transport_t * transport, const char * fqdn)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->remote_fqdn = fqdn;
}

void pnx_sasl_set_local_hostname(pn_transport_t * transport, const char * fqdn)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->local_fqdn = pn_strdup(fqdn);
}

void pni_sasl_set_user_password(pn_transport_t *transport, const char *user, const char *authzid, const char *password)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->username = user;
  sasl->authzid = authzid;
  free(sasl->password);
  sasl->password = password ? pn_strdup(password) : NULL;
}

void pni_sasl_set_external_security(pn_transport_t *transport, int ssf, const char *authid)
{
  pni_sasl_t *sasl = transport->sasl;
  sasl->external_ssf = ssf;
  free(sasl->external_auth);
  sasl->external_auth = authid ? pn_strdup(authid) : NULL;
}

const char *pn_sasl_get_user(pn_sasl_t *sasl0)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    return sasl->username;
}

const char *pn_sasl_get_authorization(pn_sasl_t *sasl0)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    return sasl->authzid;
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
    sasl->included_mechanisms = mechs ? pn_strdup(mechs) : NULL;
}

void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl0, bool insecure)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    sasl->allow_insecure_mechs = insecure;
}

bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl0)
{
    pni_sasl_t *sasl = get_sasl_internal(sasl0);
    return sasl->allow_insecure_mechs;
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

// Received Server side
int pn_do_init(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;

  // If we haven't got an sasl struct yet we've in an error state
  // This can only happen if our peer sent SASL frames even though he didn't send the SASL header
  if (!sasl) return PN_ERR;

  // We should only receive this if we are a sasl server
  if (sasl->client) return PN_ERR;

  pn_bytes_t mech;
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[sz]", &mech, &recv);
  if (err) return err;
  sasl->selected_mechanism = pn_strndup(mech.start, mech.size);

  // We need to filter out a supplied mech in in the inclusion list
  // as the client could have used a mech that we support, but that
  // wasn't on the list we sent.
  if (!pni_sasl_server_included_mech(sasl->included_mechanisms, mech)) {
    pnx_sasl_error(transport, "Client mechanism not in mechanism inclusion list.", "amqp:unauthorized-access");
    sasl->outcome = PN_SASL_AUTH;
    pnx_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
  } else {
    pni_sasl_impl_process_init(transport, sasl->selected_mechanism, &recv);
  }

  return 0;
}

// Received client side
int pn_do_mechanisms(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;

  // If we haven't got an sasl struct yet we've in an error state
  // This can only happen if our peer sent SASL frames even though we didn't send the SASL header
  if (!sasl) return PN_ERR;

  // We should only receive this if we are a sasl client
  if (!sasl->client) return PN_ERR;

  // This scanning relies on pn_data_scan leaving the pn_data_t cursors
  // where they are after finishing the scan
  pn_string_t *mechs = pn_string("");

  // Try array of symbols for mechanism list
  bool array = false;
  int err = pn_data_scan(args, "D.[?@[", &array);
  if (err) return err;

  if (array) {
    // Now keep checking for end of array and pull a symbol
    while(pn_data_next(args)) {
      pn_bytes_t s = pn_data_get_symbol(args);
      if (pni_sasl_client_included_mech(sasl->included_mechanisms, s)) {
        pn_string_addf(mechs, "%*s ", (int)s.size, s.start);
      }
    }

    if (pn_string_size(mechs)) {
        pn_string_buffer(mechs)[pn_string_size(mechs)-1] = 0;
    }
  } else {
    // No array of symbols; try single symbol
    pn_data_rewind(args);
    pn_bytes_t symbol;
    int err = pn_data_scan(args, "D.[s]", &symbol);
    if (err) return err;

    if (pni_sasl_client_included_mech(sasl->included_mechanisms, symbol)) {
      pn_string_setn(mechs, symbol.start, symbol.size);
    }
  }

  if (!(pni_sasl_impl_init_client(transport) &&
        pn_string_size(mechs) &&
        pni_sasl_impl_process_mechanisms(transport, pn_string_get(mechs)))) {
    sasl->outcome = PN_SASL_PERM;
    pnx_sasl_set_desired_state(transport, SASL_RECVED_FAILURE);
  }

  pn_free(mechs);
  return 0;
}

// Received client side
int pn_do_challenge(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;

  // If we haven't got an sasl struct yet we've in an error state
  // This can only happen if our peer sent SASL frames even though we didn't send the SASL header
  if (!sasl) return PN_ERR;

  // We should only receive this if we are a sasl client
  if (!sasl->client) return PN_ERR;

  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[z]", &recv);
  if (err) return err;

  pni_sasl_impl_process_challenge(transport, &recv);

  return 0;
}

// Received server side
int pn_do_response(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;

  // If we haven't got an sasl struct yet we've in an error state
  // This can only happen if our peer sent SASL frames even though he didn't send the SASL header
  if (!sasl) return PN_ERR;

  // We should only receive this if we are a sasl server
  if (sasl->client) return PN_ERR;

  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[z]", &recv);
  if (err) return err;

  pni_sasl_impl_process_response(transport, &recv);

  return 0;
}

// Received client side
int pn_do_outcome(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pni_sasl_t *sasl = transport->sasl;

  // If we haven't got an sasl struct yet we've in an error state
  // This can only happen if our peer sent SASL frames even though we didn't send the SASL header
  if (!sasl) return PN_ERR;

  // We should only receive this if we are a sasl client
  if (!sasl->client) return PN_ERR;

  uint8_t outcome;
  pn_bytes_t recv;
  int err = pn_data_scan(args, "D.[Bz]", &outcome, &recv);
  if (err) return err;

  // Preset the outcome to what the server sent us - the plugin can alter this.
  // In practise the plugin processing here should only fail because it fails
  // to authenticate the server id after the server authenticates our user.
  // It should never succeed after the server outcome was failure.
  sasl->outcome = (pn_sasl_outcome_t) outcome;

  pni_sasl_impl_process_outcome(transport, &recv);

  bool authenticated = sasl->outcome==PN_SASL_OK;
  transport->authenticated = authenticated;
  pnx_sasl_set_desired_state(transport, authenticated ? SASL_RECVED_SUCCESS : SASL_RECVED_FAILURE);
  return 0;
}

