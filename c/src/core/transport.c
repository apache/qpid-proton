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

#include "engine-internal.h"
#include "framing.h"
#include "platform/platform.h"
#include "platform/platform_fmt.h"
#include "sasl/sasl-internal.h"
#include "ssl/ssl-internal.h"

#include "autodetect.h"
#include "protocol.h"
#include "dispatch_actions.h"
#include "config.h"
#include "log_private.h"

#include "proton/event.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

static ssize_t transport_consume(pn_transport_t *transport);

// delivery buffers

/*
 * Call this any time anything happens that may affect channel_max:
 * i.e. when the app indicates a preference, or when we receive the
 * OPEN frame from the remote peer.  And call it to do the final
 * calculation just before we communicate our limit to the remote
 * peer by sending our OPEN frame.
 */
static void pni_calculate_channel_max(pn_transport_t *transport) {
  /*
   * The application cannot make the limit larger than
   * what this library will allow.
   */
  transport->channel_max = (PN_IMPL_CHANNEL_MAX < transport->local_channel_max)
                           ? PN_IMPL_CHANNEL_MAX
                           : transport->local_channel_max;

  /*
   * The remote peer's constraint is not valid until the
   * peer's open frame has been received.
   */
  if(transport->open_rcvd) {
    transport->channel_max = (transport->channel_max < transport->remote_channel_max)
                             ? transport->channel_max
                             : transport->remote_channel_max;
  }
}

void pn_delivery_map_init(pn_delivery_map_t *db, pn_sequence_t next)
{
  db->deliveries = pn_hash(PN_WEAKREF, 0, 0.75);
  db->next = next;
}

void pn_delivery_map_free(pn_delivery_map_t *db)
{
  pn_free(db->deliveries);
}

static inline uintptr_t pni_sequence_make_hash ( pn_sequence_t i )
{
  return i & 0x00000000FFFFFFFFUL;
}


static pn_delivery_t *pni_delivery_map_get(pn_delivery_map_t *db, pn_sequence_t id)
{
  return (pn_delivery_t *) pn_hash_get(db->deliveries, pni_sequence_make_hash(id) );
}

static void pn_delivery_state_init(pn_delivery_state_t *ds, pn_delivery_t *delivery, pn_sequence_t id)
{
  ds->id = id;
  ds->sending = false;
  ds->sent = false;
  ds->init = true;
}

static pn_delivery_state_t *pni_delivery_map_push(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  pn_delivery_state_t *ds = &delivery->state;
  pn_delivery_state_init(ds, delivery, db->next++);
  pn_hash_put(db->deliveries, pni_sequence_make_hash(ds->id), delivery);
  return ds;
}

void pn_delivery_map_del(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  if (delivery->state.init) {
    delivery->state.init = false;
    delivery->state.sending = false;
    delivery->state.sent = false;
    pn_hash_del(db->deliveries, pni_sequence_make_hash(delivery->state.id) );
  }
}

static void pni_delivery_map_clear(pn_delivery_map_t *dm)
{
  pn_hash_t *hash = dm->deliveries;
  for (pn_handle_t entry = pn_hash_head(hash);
       entry;
       entry = pn_hash_next(hash, entry))
  {
    pn_delivery_t *dlv = (pn_delivery_t *) pn_hash_value(hash, entry);
    pn_delivery_map_del(dm, dlv);
  }
  dm->next = 0;
}

static void pni_default_tracer(pn_transport_t *transport, const char *message)
{
  fprintf(stderr, "[%p]:%s\n", (void *) transport, message);
}

static ssize_t pn_io_layer_input_passthru(pn_transport_t *, unsigned int, const char *, size_t );
static ssize_t pn_io_layer_output_passthru(pn_transport_t *, unsigned int, char *, size_t );

static ssize_t pn_io_layer_input_error(pn_transport_t *, unsigned int, const char *, size_t );
static ssize_t pn_io_layer_output_error(pn_transport_t *, unsigned int, char *, size_t );

static ssize_t pn_io_layer_input_setup(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_io_layer_output_setup(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);

static ssize_t pn_input_read_amqp_header(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_input_read_amqp(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_output_write_amqp_header(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);
static ssize_t pn_output_write_amqp(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);
static void pn_error_amqp(pn_transport_t *transport, unsigned int layer);
static pn_timestamp_t pn_tick_amqp(pn_transport_t *transport, unsigned int layer, pn_timestamp_t now);

static ssize_t pn_io_layer_input_autodetect(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_io_layer_output_null(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);

const pn_io_layer_t amqp_header_layer = {
    pn_input_read_amqp_header,
    pn_output_write_amqp_header,
    NULL,
    pn_tick_amqp,
    NULL
};

const pn_io_layer_t amqp_write_header_layer = {
    pn_input_read_amqp,
    pn_output_write_amqp_header,
    NULL,
    pn_tick_amqp,
    NULL
};

const pn_io_layer_t amqp_read_header_layer = {
    pn_input_read_amqp_header,
    pn_output_write_amqp,
    pn_error_amqp,
    pn_tick_amqp,
    NULL
};

const pn_io_layer_t amqp_layer = {
    pn_input_read_amqp,
    pn_output_write_amqp,
    pn_error_amqp,
    pn_tick_amqp,
    NULL
};

const pn_io_layer_t pni_setup_layer = {
    pn_io_layer_input_setup,
    pn_io_layer_output_setup,
    NULL,
    NULL,
    NULL
};

const pn_io_layer_t pni_autodetect_layer = {
    pn_io_layer_input_autodetect,
    pn_io_layer_output_null,
    NULL,
    NULL,
    NULL
};

const pn_io_layer_t pni_passthru_layer = {
    pn_io_layer_input_passthru,
    pn_io_layer_output_passthru,
    NULL,
    NULL,
    NULL
};

const pn_io_layer_t pni_header_error_layer = {
    pn_io_layer_input_error,
    pn_output_write_amqp_header,
    NULL,
    NULL,
    NULL
};

const pn_io_layer_t pni_error_layer = {
    pn_io_layer_input_error,
    pn_io_layer_output_error,
    pn_error_amqp,
    NULL,
    NULL
};

/* Set up the transport protocol layers depending on what is configured */
static void pn_io_layer_setup(pn_transport_t *transport, unsigned int layer)
{
  assert(layer == 0);
  // Figure out if we are server or not
  if (transport->server) {
      transport->io_layers[layer++] = &pni_autodetect_layer;
      return;
  }
  if (transport->ssl) {
    transport->io_layers[layer++] = &ssl_layer;
  }
  if (transport->sasl) {
    transport->io_layers[layer++] = &sasl_header_layer;
  }
  transport->io_layers[layer++] = &amqp_header_layer;
}

ssize_t pn_io_layer_input_setup(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available)
{
  pn_io_layer_setup(transport, layer);
  return transport->io_layers[layer]->process_input(transport, layer, bytes, available);
}

ssize_t pn_io_layer_output_setup(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available)
{
  pn_io_layer_setup(transport, layer);
  return transport->io_layers[layer]->process_output(transport, layer, bytes, available);
}

void pn_set_error_layer(pn_transport_t *transport)
{
  // Set every layer to the error layer in case we manually
  // pass through (happens from SASL to AMQP)
  for (int layer=0; layer<PN_IO_LAYER_CT; ++layer) {
    transport->io_layers[layer] = &pni_error_layer;
  }
}

// Autodetect the layer by reading the protocol header
ssize_t pn_io_layer_input_autodetect(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available)
{
  const char* error;
  bool eos = pn_transport_capacity(transport)==PN_EOS;
  if (eos && available==0) {
    pn_do_error(transport, "amqp:connection:framing-error", "No valid protocol header found");
    pn_set_error_layer(transport);
    return PN_EOS;
  }
  pni_protocol_type_t protocol = pni_sniff_header(bytes, available);
  if (transport->trace & PN_TRACE_DRV)
      pn_transport_logf(transport, "%s detected", pni_protocol_name(protocol));
  switch (protocol) {
  case PNI_PROTOCOL_SSL:
    if (!(transport->allowed_layers & LAYER_SSL)) {
      error = "SSL protocol header not allowed (maybe detected twice)";
      break;
    }
    transport->present_layers |= LAYER_SSL;
    transport->allowed_layers &= LAYER_AMQP1 | LAYER_AMQPSASL;
    if (!transport->ssl) {
      pn_ssl(transport);
    }
    transport->io_layers[layer] = &ssl_layer;
    transport->io_layers[layer+1] = &pni_autodetect_layer;
    return ssl_layer.process_input(transport, layer, bytes, available);
  case PNI_PROTOCOL_AMQP_SSL:
    if (!(transport->allowed_layers & LAYER_AMQPSSL)) {
      error = "AMQP SSL protocol header not allowed (maybe detected twice)";
      break;
    }
    transport->present_layers |= LAYER_AMQPSSL;
    transport->allowed_layers &= LAYER_AMQP1 | LAYER_AMQPSASL;
    if (!transport->ssl) {
      pn_ssl(transport);
    }
    transport->io_layers[layer] = &ssl_layer;
    transport->io_layers[layer+1] = &pni_autodetect_layer;
    return 8;
  case PNI_PROTOCOL_AMQP_SASL:
    if (!(transport->allowed_layers & LAYER_AMQPSASL)) {
      error = "AMQP SASL protocol header not allowed (maybe detected twice)";
      break;
    }
    transport->present_layers |= LAYER_AMQPSASL;
    transport->allowed_layers &= LAYER_AMQP1 | LAYER_AMQPSSL;
    if (!transport->sasl) {
      pn_sasl(transport);
    }
    transport->io_layers[layer] = &sasl_write_header_layer;
    transport->io_layers[layer+1] = &pni_autodetect_layer;
    if (transport->trace & PN_TRACE_FRM)
        pn_transport_logf(transport, "  <- %s", "SASL");
    pni_sasl_set_external_security(transport, pn_ssl_get_ssf((pn_ssl_t*)transport), pn_ssl_get_remote_subject((pn_ssl_t*)transport));
    return 8;
  case PNI_PROTOCOL_AMQP1:
    if (!(transport->allowed_layers & LAYER_AMQP1)) {
      error = "AMQP1.0 protocol header not allowed (maybe detected twice)";
      break;
    }
    transport->present_layers |= LAYER_AMQP1;
    transport->allowed_layers = LAYER_NONE;
    if (transport->auth_required && !pn_transport_is_authenticated(transport)) {
      pn_do_error(transport, "amqp:connection:policy-error",
                  "Client skipped authentication - forbidden");
      pn_set_error_layer(transport);
      return 8;
    }
    if (transport->encryption_required && !pn_transport_is_encrypted(transport)) {
      pn_do_error(transport, "amqp:connection:policy-error",
                  "Client connection unencrypted - forbidden");
      pn_set_error_layer(transport);
      return 8;
    }
    transport->io_layers[layer] = &amqp_write_header_layer;
    if (transport->trace & PN_TRACE_FRM)
        pn_transport_logf(transport, "  <- %s", "AMQP");
    return 8;
  case PNI_PROTOCOL_INSUFFICIENT:
    if (!eos) return 0;
    error = "End of input stream before protocol detection";
    break;
  case PNI_PROTOCOL_AMQP_OTHER:
    error = "Incompatible AMQP connection detected";
    break;
  case PNI_PROTOCOL_UNKNOWN:
  default:
    error = "Unknown protocol detected";
    break;
  }
  transport->io_layers[layer] = &pni_header_error_layer;
  char quoted[1024];
  pn_quote_data(quoted, 1024, bytes, available);
  pn_do_error(transport, "amqp:connection:framing-error",
              "%s: '%s'%s", error, quoted,
              !eos ? "" : " (connection aborted)");
  return 0;
}

// We don't know what the output should be - do nothing
ssize_t pn_io_layer_output_null(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available)
{
  return 0;
}

/** Pass through input handler */
ssize_t pn_io_layer_input_passthru(pn_transport_t *transport, unsigned int layer, const char *data, size_t available)
{
    if (layer+1<PN_IO_LAYER_CT)
        return transport->io_layers[layer+1]->process_input(transport, layer+1, data, available);
    return PN_EOS;
}

/** Pass through output handler */
ssize_t pn_io_layer_output_passthru(pn_transport_t *transport, unsigned int layer, char *data, size_t available)
{
    if (layer+1<PN_IO_LAYER_CT)
        return transport->io_layers[layer+1]->process_output(transport, layer+1, data, available);
    return PN_EOS;
}

/** Input handler after detected error */
ssize_t pn_io_layer_input_error(pn_transport_t *transport, unsigned int layer, const char *data, size_t available)
{
    return PN_EOS;
}

/** Output handler after detected error */
ssize_t pn_io_layer_output_error(pn_transport_t *transport, unsigned int layer, char *data, size_t available)
{
    return PN_EOS;
}

static void pn_transport_initialize(void *object)
{
  pn_transport_t *transport = (pn_transport_t *)object;
  transport->freed = false;
  transport->output_buf = NULL;
  transport->output_size = PN_TRANSPORT_INITIAL_BUFFER_SIZE;
  transport->input_buf = NULL;
  transport->input_size =  PN_TRANSPORT_INITIAL_BUFFER_SIZE;
  transport->tracer = pni_default_tracer;
  transport->sasl = NULL;
  transport->ssl = NULL;

  transport->scratch = pn_string(NULL);
  transport->args = pn_data(16);
  transport->output_args = pn_data(16);
  transport->frame = pn_buffer(PN_TRANSPORT_INITIAL_FRAME_SIZE);
  transport->input_frames_ct = 0;
  transport->output_frames_ct = 0;

  transport->connection = NULL;
  transport->context = pn_record();

  for (int layer=0; layer<PN_IO_LAYER_CT; ++layer) {
    transport->io_layers[layer] = NULL;
  }

  transport->allowed_layers = LAYER_AMQP1 | LAYER_AMQPSASL | LAYER_AMQPSSL | LAYER_SSL;
  transport->present_layers = LAYER_NONE;

  // Defer setting up the layers until the first data arrives or is sent
  transport->io_layers[0] = &pni_setup_layer;

  transport->open_sent = false;
  transport->open_rcvd = false;
  transport->close_sent = false;
  transport->close_rcvd = false;
  transport->tail_closed = false;
  transport->head_closed = false;
  transport->remote_container = NULL;
  transport->remote_hostname = NULL;
  transport->local_max_frame = PN_DEFAULT_MAX_FRAME_SIZE;
  transport->remote_max_frame = OPEN_MAX_FRAME_SIZE_DEFAULT;

  /*
   * We set the local limit on channels to 2^15, because
   * parts of the code use the topmost bit (of a short)
   * as a flag.
   * The peer that this transport connects to may also
   * place its own limit on max channel number, and the
   * application may also set a limit.
   * The maximum that we use will be the minimum of all
   * these constraints.
   */
  // There is no constraint yet from remote peer,
  // so set to max possible.
  transport->remote_channel_max = OPEN_CHANNEL_MAX_DEFAULT;
  transport->local_channel_max  = PN_IMPL_CHANNEL_MAX;
  transport->channel_max        = transport->local_channel_max;

  transport->local_idle_timeout = 0;
  transport->dead_remote_deadline = 0;
  transport->last_bytes_input = 0;
  transport->remote_idle_timeout = 0;
  transport->keepalive_deadline = 0;
  transport->last_bytes_output = 0;
  transport->remote_offered_capabilities = pn_data(0);
  transport->remote_desired_capabilities = pn_data(0);
  transport->remote_properties = pn_data(0);
  transport->disp_data = pn_data(0);
  pn_condition_init(&transport->remote_condition);
  pn_condition_init(&transport->condition);
  transport->error = pn_error();

  transport->local_channels = pn_hash(PN_WEAKREF, 0, 0.75);
  transport->remote_channels = pn_hash(PN_WEAKREF, 0, 0.75);

  transport->bytes_input = 0;
  transport->bytes_output = 0;

  transport->input_pending = 0;
  transport->output_pending = 0;

  transport->done_processing = false;

  transport->posted_idle_timeout = false;

  transport->server = false;
  transport->halt = false;
  transport->auth_required = false;
  transport->authenticated = false;
  transport->encryption_required = false;

  transport->referenced = true;

  transport->trace =
    (pn_env_bool("PN_TRACE_RAW") ? PN_TRACE_RAW : PN_TRACE_OFF) |
    (pn_env_bool("PN_TRACE_FRM") ? PN_TRACE_FRM : PN_TRACE_OFF) |
    (pn_env_bool("PN_TRACE_DRV") ? PN_TRACE_DRV : PN_TRACE_OFF) |
    (pn_env_bool("PN_TRACE_EVT") ? PN_TRACE_EVT : PN_TRACE_OFF) ;
}


static pn_session_t *pni_channel_state(pn_transport_t *transport, uint16_t channel)
{
  return (pn_session_t *) pn_hash_get(transport->remote_channels, channel);
}

static void pni_map_remote_channel(pn_session_t *session, uint16_t channel)
{
  pn_transport_t *transport = session->connection->transport;
  pn_hash_put(transport->remote_channels, channel, session);
  session->state.remote_channel = channel;
  pn_ep_incref(&session->endpoint);
}

void pni_transport_unbind_handles(pn_hash_t *handles, bool reset_state);

static void pni_unmap_remote_channel(pn_session_t *ssn)
{
  // XXX: should really update link state also
  pni_delivery_map_clear(&ssn->state.incoming);
  pni_transport_unbind_handles(ssn->state.remote_handles, false);
  pn_transport_t *transport = ssn->connection->transport;
  uint16_t channel = ssn->state.remote_channel;
  ssn->state.remote_channel = -2;
  if (pn_hash_get(transport->remote_channels, channel)) {
    pn_ep_decref(&ssn->endpoint);
  }
  // note: may free the session:
  pn_hash_del(transport->remote_channels, channel);
}

static void pn_transport_incref(void *object)
{
  pn_transport_t *transport = (pn_transport_t *) object;
  if (!transport->referenced) {
    transport->referenced = true;
    if (transport->connection) {
      pn_incref(transport->connection);
    } else {
      pn_object_incref(object);
    }
  } else {
    pn_object_incref(object);
  }
}

static void pn_transport_finalize(void *object);
#define pn_transport_new pn_object_new
#define pn_transport_refcount pn_object_refcount
#define pn_transport_decref pn_object_decref
#define pn_transport_reify pn_object_reify
#define pn_transport_hashcode NULL
#define pn_transport_compare NULL
#define pn_transport_inspect NULL

pn_transport_t *pn_transport(void)
{
#define pn_transport_free pn_object_free
  static const pn_class_t clazz = PN_METACLASS(pn_transport);
#undef pn_transport_free
  pn_transport_t *transport =
    (pn_transport_t *) pn_class_new(&clazz, sizeof(pn_transport_t));
  if (!transport) return NULL;

  transport->output_buf = (char *) malloc(transport->output_size);
  if (!transport->output_buf) {
    pn_transport_free(transport);
    return NULL;
  }

  transport->input_buf = (char *) malloc(transport->input_size);
  if (!transport->input_buf) {
    pn_transport_free(transport);
    return NULL;
  }

  transport->output_buffer = pn_buffer(4*1024);
  if (!transport->output_buffer) {
    pn_transport_free(transport);
    return NULL;
  }

  return transport;
}

void pn_transport_set_server(pn_transport_t *transport)
{
  assert(transport);
  transport->server = true;
}

const char *pn_transport_get_user(pn_transport_t *transport)
{
  assert(transport);
  // Client - just return whatever we gave to sasl
  if (!transport->server) {
    if (transport->sasl) return pn_sasl_get_user((pn_sasl_t *)transport);
    return "anonymous";
  }

  // Server
  // Not finished authentication yet
  if (!(transport->present_layers & LAYER_AMQP1)) return 0;
  // We have SASL so it takes precedence
  if (transport->present_layers & LAYER_AMQPSASL) return pn_sasl_get_user((pn_sasl_t *)transport);
  // No SASL but we may have a SSL remote_subject
  if (transport->present_layers & (LAYER_AMQPSSL | LAYER_SSL)) return pn_ssl_get_remote_subject((pn_ssl_t *)transport);
  // otherwise it's just an unauthenticated anonymous connection
  return "anonymous";
}

void pn_transport_require_auth(pn_transport_t *transport, bool required)
{
  assert(transport);
  transport->auth_required = required;
}

bool pn_transport_is_authenticated(pn_transport_t *transport)
{
  return transport && transport->authenticated;
}

void pn_transport_require_encryption(pn_transport_t *transport, bool required)
{
  assert(transport);
  transport->encryption_required = required;
}

bool pn_transport_is_encrypted(pn_transport_t *transport)
{
    return transport && transport->ssl && pn_ssl_get_ssf((pn_ssl_t*)transport)>0;
}

void pn_transport_free(pn_transport_t *transport)
{
  if (!transport) return;
  assert(!transport->freed);
  transport->freed = true;
  pn_decref(transport);
}

static void pn_transport_finalize(void *object)
{
  pn_transport_t *transport = (pn_transport_t *) object;

  if (transport->referenced && transport->connection && pn_refcount(transport->connection) > 1) {
    pn_object_incref(transport);
    transport->referenced = false;
    pn_decref(transport->connection);
    return;
  }

  // once the application frees the transport, no further I/O
  // processing can be done to the connection:
  pn_transport_unbind(transport);
  // we may have posted events, so stay alive until they are processed
  if (pn_refcount(transport) > 0) return;

  pn_ssl_free(transport);
  pn_sasl_free(transport);
  free(transport->remote_container);
  free(transport->remote_hostname);
  pn_free(transport->remote_offered_capabilities);
  pn_free(transport->remote_desired_capabilities);
  pn_free(transport->remote_properties);
  pn_free(transport->disp_data);
  pn_condition_tini(&transport->remote_condition);
  pn_condition_tini(&transport->condition);
  pn_error_free(transport->error);
  pn_free(transport->local_channels);
  pn_free(transport->remote_channels);
  if (transport->input_buf) free(transport->input_buf);
  if (transport->output_buf) free(transport->output_buf);
  pn_free(transport->scratch);
  pn_data_free(transport->args);
  pn_data_free(transport->output_args);
  pn_buffer_free(transport->frame);
  pn_free(transport->context);
  pn_buffer_free(transport->output_buffer);
}

static void pni_post_remote_open_events(pn_transport_t *transport, pn_connection_t *connection) {
    pn_collector_put(connection->collector, PN_OBJECT, connection, PN_CONNECTION_REMOTE_OPEN);
    if (transport->remote_idle_timeout) {
      pn_collector_put(connection->collector, PN_OBJECT, transport, PN_TRANSPORT);
    }
}

int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection)
{
  assert(transport);
  assert(connection);

  if (transport->connection) return PN_STATE_ERR;
  if (connection->transport) return PN_STATE_ERR;

  transport->connection = connection;
  connection->transport = transport;

  pn_incref(connection);

  pn_connection_bound(connection);

  // set the hostname/user/password
  if (pn_string_size(connection->auth_user)) {
    pn_sasl(transport);
    pni_sasl_set_user_password(transport, pn_string_get(connection->auth_user), pn_string_get(connection->auth_password));
  }

  if (pn_string_size(connection->hostname)) {
      if (transport->sasl) {
          pni_sasl_set_remote_hostname(transport, pn_string_get(connection->hostname));
      }

      // be sure not to overwrite a hostname already set by the user via
      // pn_ssl_set_peer_hostname() called before the bind
      if (transport->ssl) {
          size_t name_len = 0;
          pn_ssl_get_peer_hostname((pn_ssl_t*) transport, NULL, &name_len);
          if (name_len == 0) {
              pn_ssl_set_peer_hostname((pn_ssl_t*) transport, pn_string_get(connection->hostname));
          }
      }
  }

  if (transport->open_rcvd) {
    PN_SET_REMOTE(connection->endpoint.state, PN_REMOTE_ACTIVE);
    pni_post_remote_open_events(transport, connection);
    transport->halt = false;
    transport_consume(transport);        // blech - testBindAfterOpen
  }

  return 0;
}

void pni_transport_unbind_handles(pn_hash_t *handles, bool reset_state)
{
  for (pn_handle_t h = pn_hash_head(handles); h; h = pn_hash_next(handles, h)) {
    uintptr_t key = pn_hash_key(handles, h);
    pn_link_t *link = (pn_link_t *) pn_hash_value(handles, h);
    if (reset_state) {
      pn_link_unbound(link);
    }
    pn_ep_decref(&link->endpoint);
    pn_hash_del(handles, key);
  }
}

void pni_transport_unbind_channels(pn_hash_t *channels)
{
  for (pn_handle_t h = pn_hash_head(channels); h; h = pn_hash_next(channels, h)) {
    uintptr_t key = pn_hash_key(channels, h);
    pn_session_t *ssn = (pn_session_t *) pn_hash_value(channels, h);
    pni_delivery_map_clear(&ssn->state.incoming);
    pni_delivery_map_clear(&ssn->state.outgoing);
    pni_transport_unbind_handles(ssn->state.local_handles, true);
    pni_transport_unbind_handles(ssn->state.remote_handles, true);
    pn_session_unbound(ssn);
    pn_ep_decref(&ssn->endpoint);
    pn_hash_del(channels, key);
  }
}

int pn_transport_unbind(pn_transport_t *transport)
{
  assert(transport);
  if (!transport->connection) return 0;


  pn_connection_t *conn = transport->connection;
  transport->connection = NULL;
  bool was_referenced = transport->referenced;

  pn_collector_put(conn->collector, PN_OBJECT, conn, PN_CONNECTION_UNBOUND);

  // XXX: what happens if the endpoints are freed before we get here?
  pn_session_t *ssn = pn_session_head(conn, 0);
  while (ssn) {
    pni_delivery_map_clear(&ssn->state.incoming);
    pni_delivery_map_clear(&ssn->state.outgoing);
    ssn = pn_session_next(ssn, 0);
  }

  pn_endpoint_t *endpoint = conn->endpoint_head;
  while (endpoint) {
    pn_condition_clear(&endpoint->remote_condition);
    pn_modified(conn, endpoint, true);
    endpoint = endpoint->endpoint_next;
  }

  pni_transport_unbind_channels(transport->local_channels);
  pni_transport_unbind_channels(transport->remote_channels);

  pn_connection_unbound(conn);
  if (was_referenced) {
    pn_decref(conn);
  }
  return 0;
}

pn_error_t *pn_transport_error(pn_transport_t *transport)
{
  assert(transport);
  if (pn_condition_is_set(&transport->condition)) {
    pn_error_format(transport->error, PN_ERR, "%s: %s",
                    pn_condition_get_name(&transport->condition),
                    pn_condition_get_description(&transport->condition));
  } else {
    pn_error_clear(transport->error);
  }
  return transport->error;
}

pn_condition_t *pn_transport_condition(pn_transport_t *transport)
{
  assert(transport);
  return &transport->condition;
}

static void pni_map_remote_handle(pn_link_t *link, uint32_t handle)
{
  link->state.remote_handle = handle;
  pn_hash_put(link->session->state.remote_handles, handle, link);
  pn_ep_incref(&link->endpoint);
}

static void pni_unmap_remote_handle(pn_link_t *link)
{
  uintptr_t handle = link->state.remote_handle;
  link->state.remote_handle = -2;
  if (pn_hash_get(link->session->state.remote_handles, handle)) {
    pn_ep_decref(&link->endpoint);
  }
  // may delete link:
  pn_hash_del(link->session->state.remote_handles, handle);
}

static pn_link_t *pni_handle_state(pn_session_t *ssn, uint32_t handle)
{
  return (pn_link_t *) pn_hash_get(ssn->state.remote_handles, handle);
}

bool pni_disposition_batchable(pn_disposition_t *disposition)
{
  switch (disposition->type) {
  case PN_ACCEPTED:
    return true;
  case PN_RELEASED:
    return true;
  default:
    return false;
  }
}

static int pni_disposition_encode(pn_disposition_t *disposition, pn_data_t *data)
{
  pn_condition_t *cond = &disposition->condition;
  switch (disposition->type) {
  case PN_RECEIVED:
    PN_RETURN_IF_ERROR(pn_data_put_list(data));
    pn_data_enter(data);
    PN_RETURN_IF_ERROR(pn_data_put_uint(data, disposition->section_number));
    PN_RETURN_IF_ERROR(pn_data_put_ulong(data, disposition->section_offset));
    pn_data_exit(data);
    return 0;
  case PN_ACCEPTED:
  case PN_RELEASED:
    return 0;
  case PN_REJECTED:
    return pn_data_fill(data, "[?DL[sSC]]", pn_condition_is_set(cond), ERROR,
                 pn_condition_get_name(cond),
                 pn_condition_get_description(cond),
                 pn_condition_info(cond));
  case PN_MODIFIED:
    return pn_data_fill(data, "[ooC]",
                 disposition->failed,
                 disposition->undeliverable,
                 disposition->annotations);
  default:
    return pn_data_copy(data, disposition->data);
  }
}


void pn_do_trace(pn_transport_t *transport, uint16_t ch, pn_dir_t dir,
                 pn_data_t *args, const char *payload, size_t size)
{
  if (transport->trace & PN_TRACE_FRM) {
    pn_string_format(transport->scratch, "%u %s ", ch, dir == OUT ? "->" : "<-");
    pn_inspect(args, transport->scratch);

    if (pn_data_size(args)==0) {
        pn_string_addf(transport->scratch, "(EMPTY FRAME)");
    }

    if (size) {
      char buf[1024];
      int e = pn_quote_data(buf, 1024, payload, size);
      pn_string_addf(transport->scratch, " (%" PN_ZU ") \"%s\"%s", size, buf,
                     e == PN_OVERFLOW ? "... (truncated)" : "");
    }

    pn_transport_log(transport, pn_string_get(transport->scratch));
  }
}

int pn_post_frame(pn_transport_t *transport, uint8_t type, uint16_t ch, const char *fmt, ...)
{
  pn_buffer_t *frame_buf = transport->frame;
  va_list ap;
  va_start(ap, fmt);
  pn_data_clear(transport->output_args);
  int err = pn_data_vfill(transport->output_args, fmt, ap);
  va_end(ap);
  if (err) {
    pn_transport_logf(transport,
                      "error posting frame: %s, %s: %s", fmt, pn_code(err),
                      pn_error_text(pn_data_error(transport->output_args)));
    return PN_ERR;
  }

  pn_do_trace(transport, ch, OUT, transport->output_args, NULL, 0);

 encode_performatives:
  pn_buffer_clear( frame_buf );
  pn_rwbytes_t buf = pn_buffer_memory( frame_buf );
  buf.size = pn_buffer_available( frame_buf );

  ssize_t wr = pn_data_encode( transport->output_args, buf.start, buf.size );
  if (wr < 0) {
    if (wr == PN_OVERFLOW) {
      pn_buffer_ensure( frame_buf, pn_buffer_available( frame_buf ) * 2 );
      goto encode_performatives;
    }
    pn_transport_logf(transport,
                      "error posting frame: %s", pn_code(wr));
    return PN_ERR;
  }

  pn_frame_t frame = {AMQP_FRAME_TYPE};
  frame.type = type;
  frame.channel = ch;
  frame.payload = buf.start;
  frame.size = wr;
  pn_buffer_ensure(transport->output_buffer, AMQP_HEADER_SIZE+frame.ex_size+frame.size);
  pn_write_frame(transport->output_buffer, frame);
  transport->output_frames_ct += 1;
  if (transport->trace & PN_TRACE_RAW) {
    pn_string_set(transport->scratch, "RAW: \"");
    pn_buffer_quote(transport->output_buffer, transport->scratch, AMQP_HEADER_SIZE+frame.ex_size+frame.size);
    pn_string_addf(transport->scratch, "\"");
    pn_transport_log(transport, pn_string_get(transport->scratch));
  }

  return 0;
}

static int pni_post_amqp_transfer_frame(pn_transport_t *transport, uint16_t ch,
                                        uint32_t handle,
                                        pn_sequence_t id,
                                        pn_bytes_t *payload,
                                        const pn_bytes_t *tag,
                                        uint32_t message_format,
                                        bool settled,
                                        bool more,
                                        pn_sequence_t frame_limit,
                                        uint64_t code,
                                        pn_data_t* state,
                                        bool resume,
                                        bool aborted,
                                        bool batchable)
{
  bool more_flag = more;
  int framecount = 0;
  pn_buffer_t *frame = transport->frame;

  // create preformatives, assuming 'more' flag need not change

 compute_performatives:
  pn_data_clear(transport->output_args);
  int err = pn_data_fill(transport->output_args, "DL[IIzI?o?on?DLC?o?o?o]", TRANSFER,
                         handle,
                         id,
                         tag->size, tag->start,
                         message_format,
                         settled, settled,
                         more_flag, more_flag,
                         (bool)code, code, state,
                         resume, resume,
                         aborted, aborted,
                         batchable, batchable);
  if (err) {
    pn_transport_logf(transport,
                      "error posting transfer frame: %s: %s", pn_code(err),
                      pn_error_text(pn_data_error(transport->output_args)));
    return PN_ERR;
  }

  do { // send as many frames as possible without changing the 'more' flag...

  encode_performatives:
    pn_buffer_clear( frame );
    pn_rwbytes_t buf = pn_buffer_memory( frame );
    buf.size = pn_buffer_available( frame );

    ssize_t wr = pn_data_encode(transport->output_args, buf.start, buf.size);
    if (wr < 0) {
      if (wr == PN_OVERFLOW) {
        pn_buffer_ensure( frame, pn_buffer_available( frame ) * 2 );
        goto encode_performatives;
      }
      pn_transport_logf(transport, "error posting frame: %s", pn_code(wr));
      return PN_ERR;
    }
    buf.size = wr;

    // check if we need to break up the outbound frame
    size_t available = payload->size;
    if (transport->remote_max_frame) {
      if ((available + buf.size) > transport->remote_max_frame - 8) {
        available = transport->remote_max_frame - 8 - buf.size;
        if (more_flag == false) {
          more_flag = true;
          goto compute_performatives;  // deal with flag change
        }
      } else if (more_flag == true && more == false) {
        // caller has no more, and this is the last frame
        more_flag = false;
        goto compute_performatives;
      }
    }

    if (pn_buffer_available( frame ) < (available + buf.size)) {
      // not enough room for payload - try again...
      pn_buffer_ensure( frame, available + buf.size );
      goto encode_performatives;
    }

    pn_do_trace(transport, ch, OUT, transport->output_args, payload->start, available);

    memmove( buf.start + buf.size, payload->start, available);
    payload->start += available;
    payload->size -= available;
    buf.size += available;

    pn_frame_t frame = {AMQP_FRAME_TYPE};
    frame.channel = ch;
    frame.payload = buf.start;
    frame.size = buf.size;

    pn_buffer_ensure(transport->output_buffer, AMQP_HEADER_SIZE+frame.ex_size+frame.size);
    pn_write_frame(transport->output_buffer, frame);
    transport->output_frames_ct += 1;
    framecount++;
    if (transport->trace & PN_TRACE_RAW) {
      pn_string_set(transport->scratch, "RAW: \"");
      pn_buffer_quote(transport->output_buffer, transport->scratch, AMQP_HEADER_SIZE+frame.ex_size+frame.size);
      pn_string_addf(transport->scratch, "\"");
      pn_transport_log(transport, pn_string_get(transport->scratch));
    }
  } while (payload->size > 0 && framecount < frame_limit);

  return framecount;
}

static int pni_post_close(pn_transport_t *transport, pn_condition_t *cond)
{
  if (!cond && transport->connection) {
    cond = pn_connection_condition(transport->connection);
  }
  const char *condition = NULL;
  const char *description = NULL;
  pn_data_t *info = NULL;
  if (pn_condition_is_set(cond)) {
    condition = pn_condition_get_name(cond);
    description = pn_condition_get_description(cond);
    info = pn_condition_info(cond);
  }

  return pn_post_frame(transport, AMQP_FRAME_TYPE, 0, "DL[?DL[sSC]]", CLOSE,
                       (bool) condition, ERROR, condition, description, info);
}

static pn_collector_t *pni_transport_collector(pn_transport_t *transport)
{
  if (transport->connection && transport->connection->collector) {
    return transport->connection->collector;
  } else {
    return NULL;
  }
}

static void pni_maybe_post_closed(pn_transport_t *transport)
{
  pn_collector_t *collector = pni_transport_collector(transport);
  if (transport->head_closed && transport->tail_closed) {
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT_CLOSED);
  }
}

static void pni_close_tail(pn_transport_t *transport)
{
  if (!transport->tail_closed) {
    transport->tail_closed = true;
    pn_collector_t *collector = pni_transport_collector(transport);
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT_TAIL_CLOSED);
    pni_maybe_post_closed(transport);
  }
}

int pn_do_error(pn_transport_t *transport, const char *condition, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[1024];
  if (fmt) {
    // XXX: result
    pni_vsnprintf(buf, 1024, fmt, ap);
  } else {
    buf[0] = '\0';
  }
  va_end(ap);
  pn_condition_t *cond = &transport->condition;
  if (!pn_condition_is_set(cond)) {
    pn_condition_set_name(cond, condition);
    if (fmt) {
      pn_condition_set_description(cond, buf);
    }
  } else {
    const char *first = pn_condition_get_description(cond);
    if (first && fmt) {
      char extended[2048];
      pni_snprintf(extended, 2048, "%s (%s)", first, buf);
      pn_condition_set_description(cond, extended);
    } else if (fmt) {
      pn_condition_set_description(cond, buf);
    }
  }
  pn_collector_t *collector = pni_transport_collector(transport);
  pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT_ERROR);
  if (transport->trace & PN_TRACE_DRV) {
    pn_transport_logf(transport, "ERROR %s %s", condition, buf);
  }

  for (int i = 0; i<PN_IO_LAYER_CT; ++i) {
    if (transport->io_layers[i] && transport->io_layers[i]->handle_error)
        transport->io_layers[i]->handle_error(transport, i);
  }

  pni_close_tail(transport);
  return PN_ERR;
}

static char *pn_bytes_strdup(pn_bytes_t str)
{
  return pn_strndup(str.start, str.size);
}

int pn_do_open(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_connection_t *conn = transport->connection;
  bool container_q, hostname_q, remote_channel_max_q, remote_max_frame_q;
  uint16_t remote_channel_max;
  uint32_t remote_max_frame;
  pn_bytes_t remote_container, remote_hostname;
  pn_data_clear(transport->remote_offered_capabilities);
  pn_data_clear(transport->remote_desired_capabilities);
  pn_data_clear(transport->remote_properties);
  int err = pn_data_scan(args, "D.[?S?S?I?HI..CCC]",
                         &container_q, &remote_container,
                         &hostname_q, &remote_hostname,
                         &remote_max_frame_q, &remote_max_frame,
                         &remote_channel_max_q, &remote_channel_max,
                         &transport->remote_idle_timeout,
                         transport->remote_offered_capabilities,
                         transport->remote_desired_capabilities,
                         transport->remote_properties);
  if (err) return err;
  /*
   * The default value is already stored in the variable.
   * But the scanner zeroes out values if it does not
   * find them in the args, so don't give the variable
   * directly to the scanner.
   */
  transport->remote_channel_max = remote_channel_max_q ? remote_channel_max : OPEN_CHANNEL_MAX_DEFAULT;
  transport->remote_max_frame = remote_max_frame_q ? remote_max_frame : OPEN_MAX_FRAME_SIZE_DEFAULT;

  if (transport->remote_max_frame > 0) {
    if (transport->remote_max_frame < AMQP_MIN_MAX_FRAME_SIZE) {
      pn_transport_logf(transport, "Peer advertised bad max-frame (%u), forcing to %u",
                        transport->remote_max_frame, AMQP_MIN_MAX_FRAME_SIZE);
      transport->remote_max_frame = AMQP_MIN_MAX_FRAME_SIZE;
    }
  }
  if (container_q) {
    transport->remote_container = pn_bytes_strdup(remote_container);
  } else {
    transport->remote_container = NULL;
  }
  if (hostname_q) {
    transport->remote_hostname = pn_bytes_strdup(remote_hostname);
  } else {
    transport->remote_hostname = NULL;
  }

  if (conn) {
    PN_SET_REMOTE(conn->endpoint.state, PN_REMOTE_ACTIVE);
    pni_post_remote_open_events(transport, conn);
  } else {
    transport->halt = true;
  }
  transport->open_rcvd = true;
  pni_calculate_channel_max(transport);
  return 0;
}

int pn_do_begin(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  bool reply;
  uint16_t remote_channel;
  pn_sequence_t next;
  int err = pn_data_scan(args, "D.[?HI]", &reply, &remote_channel, &next);
  if (err) return err;

  // AMQP 1.0 section 2.7.1 - if the peer doesn't honor our channel_max --
  // express our displeasure by closing the connection with a framing error.
  if (channel > transport->channel_max) {
    pn_do_error(transport,
                "amqp:connection:framing-error",
                "remote channel %d is above negotiated channel_max %d.",
                channel,
                transport->channel_max
               );
    return PN_TRANSPORT_ERROR;
  }

  pn_session_t *ssn;
  if (reply) {
    ssn = (pn_session_t *) pn_hash_get(transport->local_channels, remote_channel);
    if (ssn == 0) {
      pn_do_error(transport,
                "amqp:invalid-field",
                "begin reply to unknown channel %d.",
                remote_channel
               );
      return PN_TRANSPORT_ERROR;
    }
  } else {
    ssn = pn_session(transport->connection);
  }
  ssn->state.incoming_transfer_count = next;
  pni_map_remote_channel(ssn, channel);
  PN_SET_REMOTE(ssn->endpoint.state, PN_REMOTE_ACTIVE);
  pn_collector_put(transport->connection->collector, PN_OBJECT, ssn, PN_SESSION_REMOTE_OPEN);
  return 0;
}

pn_link_t *pn_find_link(pn_session_t *ssn, pn_bytes_t name, bool is_sender)
{
  pn_endpoint_type_t type = is_sender ? SENDER : RECEIVER;

  for (size_t i = 0; i < pn_list_size(ssn->links); i++)
  {
    pn_link_t *link = (pn_link_t *) pn_list_get(ssn->links, i);
    if (link->endpoint.type == type &&
        // This function is used to locate the link object for an
        // incoming attach. If a link object of the same name is found
        // which is closed both locally and remotely, assume that is
        // no longer in use.
        !((link->endpoint.state & PN_LOCAL_CLOSED) && (link->endpoint.state & PN_REMOTE_CLOSED)) &&
        pn_bytes_equal(name, pn_string_bytes(link->name)))
    {
      return link;
    }
  }
  return NULL;
}

static pn_expiry_policy_t symbol2policy(pn_bytes_t symbol)
{
  if (!symbol.start)
    return PN_EXPIRE_WITH_SESSION;

  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(link-detach)))
    return PN_EXPIRE_WITH_LINK;
  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(session-end)))
    return PN_EXPIRE_WITH_SESSION;
  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(connection-close)))
    return PN_EXPIRE_WITH_CONNECTION;
  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(never)))
    return PN_EXPIRE_NEVER;

  return PN_EXPIRE_WITH_SESSION;
}

static pn_distribution_mode_t symbol2dist_mode(const pn_bytes_t symbol)
{
  if (!symbol.start)
    return PN_DIST_MODE_UNSPECIFIED;

  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(move)))
    return PN_DIST_MODE_MOVE;
  if (pn_bytes_equal(symbol, PN_BYTES_LITERAL(copy)))
    return PN_DIST_MODE_COPY;

  return PN_DIST_MODE_UNSPECIFIED;
}

static const char *dist_mode2symbol(const pn_distribution_mode_t mode)
{
  switch (mode)
  {
  case PN_DIST_MODE_COPY:
    return "copy";
  case PN_DIST_MODE_MOVE:
    return "move";
  default:
    return NULL;
  }
}

int pn_terminus_set_address_bytes(pn_terminus_t *terminus, pn_bytes_t address)
{
  assert(terminus);
  return pn_string_setn(terminus->address, address.start, address.size);
}

int pn_do_attach(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_bytes_t name;
  uint32_t handle;
  bool is_sender;
  pn_bytes_t source, target;
  pn_durability_t src_dr, tgt_dr;
  pn_bytes_t src_exp, tgt_exp;
  pn_seconds_t src_timeout, tgt_timeout;
  bool src_dynamic, tgt_dynamic;
  pn_sequence_t idc;
  pn_bytes_t dist_mode;
  bool snd_settle, rcv_settle;
  uint8_t snd_settle_mode, rcv_settle_mode;
  uint64_t max_msgsz;
  int err = pn_data_scan(args, "D.[SIo?B?BD.[SIsIo.s]D.[SIsIo]..IL]", &name, &handle,
                         &is_sender,
                         &snd_settle, &snd_settle_mode,
                         &rcv_settle, &rcv_settle_mode,
                         &source, &src_dr, &src_exp, &src_timeout, &src_dynamic, &dist_mode,
                         &target, &tgt_dr, &tgt_exp, &tgt_timeout, &tgt_dynamic,
                         &idc, &max_msgsz);
  if (err) return err;
  char strbuf[128];      // avoid malloc for most link names
  char *strheap = (name.size >= sizeof(strbuf)) ? (char *) malloc(name.size + 1) : NULL;
  char *strname = strheap ? strheap : strbuf;
  if (name.size > 0) strncpy(strname, name.start, name.size);
  strname[name.size] = '\0';

  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
      pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
      if (strheap) free(strheap);
      return PN_EOS;
  }
  pn_link_t *link = pn_find_link(ssn, name, is_sender);
  if (!link) {
    if (is_sender) {
      link = (pn_link_t *) pn_sender(ssn, strname);
    } else {
      link = (pn_link_t *) pn_receiver(ssn, strname);
    }
  }

  if (strheap) {
    free(strheap);
  }

  pni_map_remote_handle(link, handle);
  PN_SET_REMOTE(link->endpoint.state, PN_REMOTE_ACTIVE);
  pn_terminus_t *rsrc = &link->remote_source;
  if (source.start || src_dynamic) {
    pn_terminus_set_type(rsrc, PN_SOURCE);
    pn_terminus_set_address_bytes(rsrc, source);
    pn_terminus_set_durability(rsrc, src_dr);
    pn_terminus_set_expiry_policy(rsrc, symbol2policy(src_exp));
    pn_terminus_set_timeout(rsrc, src_timeout);
    pn_terminus_set_dynamic(rsrc, src_dynamic);
    pn_terminus_set_distribution_mode(rsrc, symbol2dist_mode(dist_mode));
  } else {
    pn_terminus_set_type(rsrc, PN_UNSPECIFIED);
  }
  pn_terminus_t *rtgt = &link->remote_target;
  if (target.start || tgt_dynamic) {
    pn_terminus_set_type(rtgt, PN_TARGET);
    pn_terminus_set_address_bytes(rtgt, target);
    pn_terminus_set_durability(rtgt, tgt_dr);
    pn_terminus_set_expiry_policy(rtgt, symbol2policy(tgt_exp));
    pn_terminus_set_timeout(rtgt, tgt_timeout);
    pn_terminus_set_dynamic(rtgt, tgt_dynamic);
  } else {
    uint64_t code = 0;
    pn_data_clear(link->remote_target.capabilities);
    err = pn_data_scan(args, "D.[.....D..DL[C]...]", &code,
                       link->remote_target.capabilities);
    if (err) return err;
    if (code == COORDINATOR) {
      pn_terminus_set_type(rtgt, PN_COORDINATOR);
    } else if (code == TARGET) {
      pn_terminus_set_type(rtgt, PN_TARGET);
    } else {
      pn_terminus_set_type(rtgt, PN_UNSPECIFIED);
    }
  }

  if (snd_settle)
    link->remote_snd_settle_mode = snd_settle_mode;
  if (rcv_settle)
    link->remote_rcv_settle_mode = rcv_settle_mode;

  pn_data_clear(link->remote_source.properties);
  pn_data_clear(link->remote_source.filter);
  pn_data_clear(link->remote_source.outcomes);
  pn_data_clear(link->remote_source.capabilities);
  pn_data_clear(link->remote_target.properties);
  pn_data_clear(link->remote_target.capabilities);

  err = pn_data_scan(args, "D.[.....D.[.....C.C.CC]D.[.....CC]",
                     link->remote_source.properties,
                     link->remote_source.filter,
                     link->remote_source.outcomes,
                     link->remote_source.capabilities,
                     link->remote_target.properties,
                     link->remote_target.capabilities);
  if (err) return err;

  pn_data_rewind(link->remote_source.properties);
  pn_data_rewind(link->remote_source.filter);
  pn_data_rewind(link->remote_source.outcomes);
  pn_data_rewind(link->remote_source.capabilities);
  pn_data_rewind(link->remote_target.properties);
  pn_data_rewind(link->remote_target.capabilities);

  if (!is_sender) {
    link->state.delivery_count = idc;
  }

  if (max_msgsz) {
    link->remote_max_message_size = max_msgsz;
  }

  pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_REMOTE_OPEN);
  return 0;
}

static int pni_post_flow(pn_transport_t *transport, pn_session_t *ssn, pn_link_t *link);

// free the delivery
static void pn_full_settle(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  assert(!delivery->work);
  pn_clear_tpwork(delivery);
  pn_delivery_map_del(db, delivery);
  pn_incref(delivery);
  pn_decref(delivery);
}

int pn_do_transfer(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  // XXX: multi transfer
  uint32_t handle;
  pn_bytes_t tag;
  bool id_present;
  pn_sequence_t id;
  bool settled;
  bool more;
  bool has_type;
  bool resume, aborted, batchable;
  uint64_t type;
  pn_data_clear(transport->disp_data);
  int err = pn_data_scan(args, "D.[I?Iz.oo.D?LCooo]", &handle, &id_present, &id, &tag,
                         &settled, &more, &has_type, &type, transport->disp_data,
                         &resume, &aborted, &batchable);
  if (err) return err;
  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
  }

  if (!ssn->state.incoming_window) {
    return pn_do_error(transport, "amqp:session:window-violation", "incoming session window exceeded");
  }

  pn_link_t *link = pni_handle_state(ssn, handle);
  if (!link) {
    return pn_do_error(transport, "amqp:invalid-field", "no such handle: %u", handle);
  }
  pn_delivery_t *delivery;
  if (link->unsettled_tail && !link->unsettled_tail->done) {
    delivery = link->unsettled_tail;
  } else {
    pn_delivery_map_t *incoming = &ssn->state.incoming;

    if (!ssn->state.incoming_init) {
      incoming->next = id;
      ssn->state.incoming_init = true;
      ssn->incoming_deliveries++;
    }

    delivery = pn_delivery(link, pn_dtag(tag.start, tag.size));
    pn_delivery_state_t *state = pni_delivery_map_push(incoming, delivery);
    if (id_present && id != state->id) {
      return pn_do_error(transport, "amqp:session:invalid-field",
                         "sequencing error, expected delivery-id %u, got %u",
                         state->id, id);
    }
    if (has_type) {
      delivery->remote.type = type;
      pn_data_copy(delivery->remote.data, transport->disp_data);
    }

    link->state.delivery_count++;
    link->state.link_credit--;
    link->queued++;

    // XXX: need to fill in remote state: delivery->remote.state = ...;
    delivery->remote.settled = settled;
    if (settled) {
      delivery->updated = true;
      pn_work_update(transport->connection, delivery);
    }
  }

  pn_buffer_append(delivery->bytes, payload->start, payload->size);
  ssn->incoming_bytes += payload->size;
  delivery->done = !more;

  ssn->state.incoming_transfer_count++;
  ssn->state.incoming_window--;

  // XXX: need better policy for when to refresh window
  if (!ssn->state.incoming_window && (int32_t) link->state.local_handle >= 0) {
    pni_post_flow(transport, ssn, link);
  }

  if ((delivery->aborted = aborted)) {
    delivery->remote.settled = true;
    delivery->done = true;
    delivery->updated = true;
    pn_work_update(transport->connection, delivery);
  }
  pn_collector_put(transport->connection->collector, PN_OBJECT, delivery, PN_DELIVERY);
  return 0;
}

int pn_do_flow(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_sequence_t onext, inext, delivery_count;
  uint32_t iwin, owin, link_credit;
  uint32_t handle;
  bool inext_init, handle_init, dcount_init, drain;
  int err = pn_data_scan(args, "D.[?IIII?I?II.o]", &inext_init, &inext, &iwin,
                         &onext, &owin, &handle_init, &handle, &dcount_init,
                         &delivery_count, &link_credit, &drain);
  if (err) return err;

  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
  }

  if (inext_init) {
    ssn->state.remote_incoming_window = inext + iwin - ssn->state.outgoing_transfer_count;
  } else {
    ssn->state.remote_incoming_window = iwin;
  }

  if (handle_init) {
    pn_link_t *link = pni_handle_state(ssn, handle);
    if (!link) {
      return pn_do_error(transport, "amqp:invalid-field", "no such handle: %u", handle);
    }
    if (link->endpoint.type == SENDER) {
      pn_sequence_t receiver_count;
      if (dcount_init) {
        receiver_count = delivery_count;
      } else {
        // our initial delivery count
        receiver_count = 0;
      }
      pn_sequence_t old = link->state.link_credit;
      link->state.link_credit = receiver_count + link_credit - link->state.delivery_count;
      link->credit += link->state.link_credit - old;
      link->drain = drain;
      pn_delivery_t *delivery = pn_link_current(link);
      if (delivery) pn_work_update(transport->connection, delivery);
    } else {
      pn_sequence_t delta = delivery_count - link->state.delivery_count;
      if (delta > 0) {
        link->state.delivery_count += delta;
        link->state.link_credit -= delta;
        link->credit -= delta;
        link->drained += delta;
      }
    }

    pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_FLOW);
  }

  return 0;
}

#define SCAN_ERROR_DEFAULT ("D.[D.[sSC]")
#define SCAN_ERROR_DETACH ("D.[..D.[sSC]")
#define SCAN_ERROR_DISP ("[D.[sSC]")

static int pn_scan_error(pn_data_t *data, pn_condition_t *condition, const char *fmt)
{
  pn_bytes_t cond;
  pn_bytes_t desc;
  pn_condition_clear(condition);
  int err = pn_data_scan(data, fmt, &cond, &desc, condition->info);
  if (err) return err;
  pn_string_setn(condition->name, cond.start, cond.size);
  pn_string_setn(condition->description, desc.start, desc.size);
  pn_data_rewind(condition->info);
  return 0;
}

/*
  This operator, inspired by code for the qpid cpp broker, gives the correct
  result when comparing sequence numbers implemented in a signed integer type.
*/

static inline int sequence_cmp(pn_sequence_t a, pn_sequence_t b) {
  return a-b;
}

int pn_do_disposition(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  bool role;
  pn_sequence_t first, last;
  uint64_t type = 0;
  bool last_init, settled, type_init;
  pn_data_clear(transport->disp_data);
  int err = pn_data_scan(args, "D.[oI?IoD?LC]", &role, &first, &last_init,
                         &last, &settled, &type_init, &type,
                         transport->disp_data);
  if (err) return err;
  if (!last_init) last = first;

  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
  }

  pn_delivery_map_t *deliveries;
  if (role) {
    deliveries = &ssn->state.outgoing;
  } else {
    deliveries = &ssn->state.incoming;
  }

  pn_data_rewind(transport->disp_data);
  bool remote_data = (pn_data_next(transport->disp_data) &&
                      pn_data_get_list(transport->disp_data) > 0);

  for (pn_sequence_t id = first; sequence_cmp(id, last) <= 0; ++id) {
    pn_delivery_t *delivery = pni_delivery_map_get(deliveries, id);
    pn_disposition_t *remote = &delivery->remote;
    if (delivery) {
      if (type_init) remote->type = type;
      if (remote_data) {
        switch (type) {
        case PN_RECEIVED:
          pn_data_rewind(transport->disp_data);
          pn_data_next(transport->disp_data);
          pn_data_enter(transport->disp_data);
          if (pn_data_next(transport->disp_data))
            remote->section_number = pn_data_get_uint(transport->disp_data);
          if (pn_data_next(transport->disp_data))
            remote->section_offset = pn_data_get_ulong(transport->disp_data);
          break;
        case PN_ACCEPTED:
          break;
        case PN_REJECTED:
          err = pn_scan_error(transport->disp_data, &remote->condition, SCAN_ERROR_DISP);
          if (err) return err;
          break;
        case PN_RELEASED:
          break;
        case PN_MODIFIED:
          pn_data_rewind(transport->disp_data);
          pn_data_next(transport->disp_data);
          pn_data_enter(transport->disp_data);
          if (pn_data_next(transport->disp_data))
            remote->failed = pn_data_get_bool(transport->disp_data);
          if (pn_data_next(transport->disp_data))
            remote->undeliverable = pn_data_get_bool(transport->disp_data);
          pn_data_narrow(transport->disp_data);
          pn_data_clear(remote->data);
          pn_data_appendn(remote->annotations, transport->disp_data, 1);
          pn_data_widen(transport->disp_data);
          break;
        default:
          pn_data_copy(remote->data, transport->disp_data);
          break;
        }
      }
      remote->settled = settled;
      delivery->updated = true;
      pn_work_update(transport->connection, delivery);

      pn_collector_put(transport->connection->collector, PN_OBJECT, delivery, PN_DELIVERY);
    }
  }

  return 0;
}

int pn_do_detach(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  uint32_t handle;
  bool closed;
  int err = pn_data_scan(args, "D.[Io]", &handle, &closed);
  if (err) return err;

  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
  }
  pn_link_t *link = pni_handle_state(ssn, handle);
  if (!link) {
    return pn_do_error(transport, "amqp:invalid-field", "no such handle: %u", handle);
  }

  err = pn_scan_error(args, &link->endpoint.remote_condition, SCAN_ERROR_DETACH);
  if (err) return err;

  if (closed)
  {
    PN_SET_REMOTE(link->endpoint.state, PN_REMOTE_CLOSED);
    pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_REMOTE_CLOSE);
  } else {
    pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_REMOTE_DETACH);
  }

  pni_unmap_remote_handle(link);
  return 0;
}

int pn_do_end(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_session_t *ssn = pni_channel_state(transport, channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:not-allowed", "no such channel: %u", channel);
  }
  int err = pn_scan_error(args, &ssn->endpoint.remote_condition, SCAN_ERROR_DEFAULT);
  if (err) return err;
  PN_SET_REMOTE(ssn->endpoint.state, PN_REMOTE_CLOSED);
  pn_collector_put(transport->connection->collector, PN_OBJECT, ssn, PN_SESSION_REMOTE_CLOSE);
  pni_unmap_remote_channel(ssn);
  return 0;
}

int pn_do_close(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_connection_t *conn = transport->connection;
  int err = pn_scan_error(args, &transport->remote_condition, SCAN_ERROR_DEFAULT);
  if (err) return err;
  transport->close_rcvd = true;
  PN_SET_REMOTE(conn->endpoint.state, PN_REMOTE_CLOSED);
  pn_collector_put(transport->connection->collector, PN_OBJECT, conn, PN_CONNECTION_REMOTE_CLOSE);
  return 0;
}

// deprecated
ssize_t pn_transport_input(pn_transport_t *transport, const char *bytes, size_t available)
{
  if (!transport) return PN_ARG_ERR;
  if (available == 0) {
    return pn_transport_close_tail(transport);
  }
  const size_t original = available;
  ssize_t capacity = pn_transport_capacity(transport);
  if (capacity < 0) return capacity;
  while (available && capacity) {
    char *dest = pn_transport_tail(transport);
    assert(dest);
    size_t count = pn_min( (size_t)capacity, available );
    memmove( dest, bytes, count );
    available -= count;
    bytes += count;
    int rc = pn_transport_process( transport, count );
    if (rc < 0) return rc;
    capacity = pn_transport_capacity(transport);
    if (capacity < 0) return capacity;
  }

  return original - available;
}

// process pending input until none remaining or EOS
static ssize_t transport_consume(pn_transport_t *transport)
{
  // This allows whatever is driving the I/O to set the error
  // condition on the transport before doing pn_transport_close_head()
  // or pn_transport_close_tail(). This allows all transport errors to
  // flow to the app the same way, but provides cleaner error messages
  // since we don't try to look for a protocol header when, e.g. the
  // connection was refused.
  if (!transport->bytes_input && transport->tail_closed &&
      pn_condition_is_set(&transport->condition)) {
    pn_do_error(transport, NULL, NULL);
    return PN_EOS;
  }

  size_t consumed = 0;

  while (transport->input_pending || transport->tail_closed) {
    ssize_t n;
    n = transport->io_layers[0]->
      process_input( transport, 0,
                     transport->input_buf + consumed,
                     transport->input_pending );
    if (n > 0) {
      consumed += n;
      transport->input_pending -= n;
    } else if (n == 0) {
      break;
    } else {
      assert(n == PN_EOS);
      if (transport->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
        pn_transport_log(transport, "  <- EOS");
      transport->input_pending = 0;  // XXX ???
      return n;
    }
  }

  if (transport->input_pending && consumed) {
    memmove( transport->input_buf,  &transport->input_buf[consumed], transport->input_pending );
  }

  return consumed;
}

static int pni_process_conn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (!(endpoint->state & PN_LOCAL_UNINIT) && !transport->open_sent)
    {
      // as per the recommendation in the spec, advertise half our
      // actual timeout to the remote
      const pn_millis_t idle_timeout = transport->local_idle_timeout
          ? (transport->local_idle_timeout/2)
          : 0;
      pn_connection_t *connection = (pn_connection_t *) endpoint;
      const char *cid = pn_string_get(connection->container);
      pni_calculate_channel_max(transport);
      int err = pn_post_frame(transport, AMQP_FRAME_TYPE, 0, "DL[SS?I?H?InnCCC]", OPEN,
                              cid ? cid : "",
                              pn_string_get(connection->hostname),
                              // TODO: This is messy, because we also have to allow local_max_frame_ to be 0 to mean unlimited
                              // otherwise flow control goes wrong
                              transport->local_max_frame!=0 && transport->local_max_frame!=OPEN_MAX_FRAME_SIZE_DEFAULT,
                                transport->local_max_frame,
                              transport->channel_max!=OPEN_CHANNEL_MAX_DEFAULT, transport->channel_max,
                              (bool)idle_timeout, idle_timeout,
                              connection->offered_capabilities,
                              connection->desired_capabilities,
                              connection->properties);
      if (err) return err;
      transport->open_sent = true;
    }
  }

  return 0;
}

static uint16_t allocate_alias(pn_hash_t *aliases, uint32_t max_index, int * valid)
{
  for (uint32_t i = 0; i <= max_index; i++) {
    if (!pn_hash_get(aliases, i)) {
      * valid = 1;
      return i;
    }
  }

  * valid = 0;
  return 0;
}

static size_t pni_session_outgoing_window(pn_session_t *ssn)
{
  return ssn->outgoing_window;
}

static size_t pni_session_incoming_window(pn_session_t *ssn)
{
  uint32_t size = ssn->connection->transport->local_max_frame;
  if (!size) {
    return 2147483647; // biggest legal value
  } else {
    return (ssn->incoming_capacity - ssn->incoming_bytes)/size;
  }
}

static int pni_map_local_channel(pn_session_t *ssn)
{
  pn_transport_t *transport = ssn->connection->transport;
  pn_session_state_t *state = &ssn->state;
  int valid;
  uint16_t channel = allocate_alias(transport->local_channels, transport->channel_max, & valid);
  if (!valid) {
    return 0;
  }
  state->local_channel = channel;
  pn_hash_put(transport->local_channels, channel, ssn);
  pn_ep_incref(&ssn->endpoint);
  return 1;
}

static int pni_process_ssn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION && transport->open_sent)
  {
    pn_session_t *ssn = (pn_session_t *) endpoint;
    pn_session_state_t *state = &ssn->state;
    if (!(endpoint->state & PN_LOCAL_UNINIT) && state->local_channel == (uint16_t) -1)
    {
      if (! pni_map_local_channel(ssn)) {
        pn_transport_logf(transport, "unable to find an open available channel within limit of %d", transport->channel_max );
        return PN_ERR;
      }
      state->incoming_window = pni_session_incoming_window(ssn);
      state->outgoing_window = pni_session_outgoing_window(ssn);
      pn_post_frame(transport, AMQP_FRAME_TYPE, state->local_channel, "DL[?HIII]", BEGIN,
                    ((int16_t) state->remote_channel >= 0), state->remote_channel,
                    state->outgoing_transfer_count,
                    state->incoming_window,
                    state->outgoing_window);
    }
  }

  return 0;
}

static const char *expiry_symbol(pn_expiry_policy_t policy)
{
  switch (policy)
  {
  case PN_EXPIRE_WITH_LINK:
    return "link-detach";
  case PN_EXPIRE_WITH_SESSION:
    return NULL;
  case PN_EXPIRE_WITH_CONNECTION:
    return "connection-close";
  case PN_EXPIRE_NEVER:
    return "never";
  }
  return NULL;
}

static int pni_map_local_handle(pn_link_t *link) {
  pn_link_state_t *state = &link->state;
  pn_session_state_t *ssn_state = &link->session->state;
  int valid;
  // XXX TODO MICK: once changes are made to handle_max, change this hardcoded value to something reasonable.
  state->local_handle = allocate_alias(ssn_state->local_handles, 65536, & valid);
  if ( ! valid )
    return 0;
  pn_hash_put(ssn_state->local_handles, state->local_handle, link);
  pn_ep_incref(&link->endpoint);
  return 1;
}

static int pni_process_link_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (transport->open_sent && (endpoint->type == SENDER ||
                               endpoint->type == RECEIVER))
  {
    pn_link_t *link = (pn_link_t *) endpoint;
    pn_session_state_t *ssn_state = &link->session->state;
    pn_link_state_t *state = &link->state;
    if (((int16_t) ssn_state->local_channel >= 0) &&
        !(endpoint->state & PN_LOCAL_UNINIT) && state->local_handle == (uint32_t) -1)
    {
      pni_map_local_handle(link);
      const pn_distribution_mode_t dist_mode = link->source.distribution_mode;
      if (link->target.type == PN_COORDINATOR) {
        int err = pn_post_frame(transport, AMQP_FRAME_TYPE, ssn_state->local_channel,
                                "DL[SIoBB?DL[SIsIoC?sCnCC]DL[C]nnI]", ATTACH,
                                pn_string_get(link->name),
                                state->local_handle,
                                endpoint->type == RECEIVER,
                                link->snd_settle_mode,
                                link->rcv_settle_mode,
                                (bool) link->source.type, SOURCE,
                                pn_string_get(link->source.address),
                                link->source.durability,
                                expiry_symbol(link->source.expiry_policy),
                                link->source.timeout,
                                link->source.dynamic,
                                link->source.properties,
                                (dist_mode != PN_DIST_MODE_UNSPECIFIED), dist_mode2symbol(dist_mode),
                                link->source.filter,
                                link->source.outcomes,
                                link->source.capabilities,
                                COORDINATOR, link->target.capabilities,
                                0);
        if (err) return err;
      } else {
        int err = pn_post_frame(transport, AMQP_FRAME_TYPE, ssn_state->local_channel,
                                "DL[SIoBB?DL[SIsIoC?sCnCC]?DL[SIsIoCC]nnIL]", ATTACH,
                                pn_string_get(link->name),
                                state->local_handle,
                                endpoint->type == RECEIVER,
                                link->snd_settle_mode,
                                link->rcv_settle_mode,
                                (bool) link->source.type, SOURCE,
                                pn_string_get(link->source.address),
                                link->source.durability,
                                expiry_symbol(link->source.expiry_policy),
                                link->source.timeout,
                                link->source.dynamic,
                                link->source.properties,
                                (dist_mode != PN_DIST_MODE_UNSPECIFIED), dist_mode2symbol(dist_mode),
                                link->source.filter,
                                link->source.outcomes,
                                link->source.capabilities,
                                (bool) link->target.type, TARGET,
                                pn_string_get(link->target.address),
                                link->target.durability,
                                expiry_symbol(link->target.expiry_policy),
                                link->target.timeout,
                                link->target.dynamic,
                                link->target.properties,
                                link->target.capabilities,
                                0, link->max_message_size);
        if (err) return err;
      }
    }
  }

  return 0;
}

static int pni_post_flow(pn_transport_t *transport, pn_session_t *ssn, pn_link_t *link)
{
  ssn->state.incoming_window = pni_session_incoming_window(ssn);
  ssn->state.outgoing_window = pni_session_outgoing_window(ssn);
  bool linkq = (bool) link;
  pn_link_state_t *state = &link->state;
  return pn_post_frame(transport, AMQP_FRAME_TYPE, ssn->state.local_channel, "DL[?IIII?I?I?In?o]", FLOW,
                       (int16_t) ssn->state.remote_channel >= 0, ssn->state.incoming_transfer_count,
                       ssn->state.incoming_window,
                       ssn->state.outgoing_transfer_count,
                       ssn->state.outgoing_window,
                       linkq, linkq ? state->local_handle : 0,
                       linkq, linkq ? state->delivery_count : 0,
                       linkq, linkq ? state->link_credit : 0,
                       linkq, linkq ? link->drain : false);
}

static int pni_process_flow_receiver(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == RECEIVER && endpoint->state & PN_LOCAL_ACTIVE)
  {
    pn_link_t *rcv = (pn_link_t *) endpoint;
    pn_session_t *ssn = rcv->session;
    pn_link_state_t *state = &rcv->state;
    if ((int16_t) ssn->state.local_channel >= 0 &&
        (int32_t) state->local_handle >= 0 &&
        ((rcv->drain || state->link_credit != rcv->credit - rcv->queued) || !ssn->state.incoming_window)) {
      state->link_credit = rcv->credit - rcv->queued;
      return pni_post_flow(transport, ssn, rcv);
    }
  }

  return 0;
}

static int pni_flush_disp(pn_transport_t *transport, pn_session_t *ssn)
{
  uint64_t code = ssn->state.disp_code;
  bool settled = ssn->state.disp_settled;
  if (ssn->state.disp) {
    int err = pn_post_frame(transport, AMQP_FRAME_TYPE, ssn->state.local_channel, "DL[oI?I?o?DL[]]", DISPOSITION,
                            ssn->state.disp_type,
                            ssn->state.disp_first,
                            ssn->state.disp_last!=ssn->state.disp_first, ssn->state.disp_last,
                            settled, settled,
                            (bool)code, code);
    if (err) return err;
    ssn->state.disp_type = 0;
    ssn->state.disp_code = 0;
    ssn->state.disp_settled = 0;
    ssn->state.disp_first = 0;
    ssn->state.disp_last = 0;
    ssn->state.disp = false;
  }
  return 0;
}

static int pni_post_disp(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  pn_session_t *ssn = link->session;
  pn_session_state_t *ssn_state = &ssn->state;
  pn_modified(transport->connection, &link->session->endpoint, false);
  pn_delivery_state_t *state = &delivery->state;
  assert(state->init);
  bool role = (link->endpoint.type == RECEIVER);
  uint64_t code = delivery->local.type;

  if (!code && !delivery->local.settled) {
    return 0;
  }

  if (!pni_disposition_batchable(&delivery->local)) {
    pn_data_clear(transport->disp_data);
    PN_RETURN_IF_ERROR(pni_disposition_encode(&delivery->local, transport->disp_data));
    return pn_post_frame(transport, AMQP_FRAME_TYPE, ssn->state.local_channel,
      "DL[oIn?o?DLC]", DISPOSITION,
      role, state->id,
      delivery->local.settled, delivery->local.settled,
      (bool)code, code, transport->disp_data);
  }

  if (ssn_state->disp && code == ssn_state->disp_code &&
      delivery->local.settled == ssn_state->disp_settled &&
      ssn_state->disp_type == role) {
    if (state->id == ssn_state->disp_first - 1) {
      ssn_state->disp_first = state->id;
      return 0;
    } else if (state->id == ssn_state->disp_last + 1) {
      ssn_state->disp_last = state->id;
      return 0;
    }
  }

  if (ssn_state->disp) {
    int err = pni_flush_disp(transport, ssn);
    if (err) return err;
  }

  ssn_state->disp_type = role;
  ssn_state->disp_code = code;
  ssn_state->disp_settled = delivery->local.settled;
  ssn_state->disp_first = state->id;
  ssn_state->disp_last = state->id;
  ssn_state->disp = true;

  return 0;
}

static int pni_process_tpwork_sender(pn_transport_t *transport, pn_delivery_t *delivery, bool *settle)
{
  pn_link_t *link = delivery->link;
  pn_delivery_state_t *state = &delivery->state;
  if (delivery->aborted && !delivery->state.sending) {
    // Aborted delivery with no data yet sent, drop it and issue a FLOW as we may have credit.
    *settle = true;
    state->sent = true;
    pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_FLOW);
    return 0;
  }
  *settle = false;
  pn_session_state_t *ssn_state = &link->session->state;
  pn_link_state_t *link_state = &link->state;
  bool xfr_posted = false;
  if ((int16_t) ssn_state->local_channel >= 0 && (int32_t) link_state->local_handle >= 0) {
    if (!state->sent && (delivery->done || pn_buffer_size(delivery->bytes) > 0) &&
        ssn_state->remote_incoming_window > 0 && link_state->link_credit > 0) {
      if (!state->init) {
        state = pni_delivery_map_push(&ssn_state->outgoing, delivery);
      }

      pn_bytes_t bytes = pn_buffer_bytes(delivery->bytes);
      size_t full_size = bytes.size;
      pn_bytes_t tag = pn_buffer_bytes(delivery->tag);
      pn_data_clear(transport->disp_data);
      PN_RETURN_IF_ERROR(pni_disposition_encode(&delivery->local, transport->disp_data));
      int count = pni_post_amqp_transfer_frame(transport,
                                               ssn_state->local_channel,
                                               link_state->local_handle,
                                               state->id, &bytes, &tag,
                                               0, // message-format
                                               delivery->local.settled,
                                               !delivery->done,
                                               ssn_state->remote_incoming_window,
                                               delivery->local.type,
                                               transport->disp_data,
                                               false, /* Resume */
                                               delivery->aborted,
                                               false /* Batchable */
      );
      if (count < 0) return count;
      state->sending = true;
      xfr_posted = true;
      ssn_state->outgoing_transfer_count += count;
      ssn_state->remote_incoming_window -= count;

      int sent = full_size - bytes.size;
      pn_buffer_trim(delivery->bytes, sent, 0);
      link->session->outgoing_bytes -= sent;
      if (!pn_buffer_size(delivery->bytes) && delivery->done) {
        state->sent = true;
        link_state->delivery_count++;
        link_state->link_credit--;
        link->queued--;
        link->session->outgoing_deliveries--;
      }

      pn_collector_put(transport->connection->collector, PN_OBJECT, link, PN_LINK_FLOW);
    }
  }

  if (!state->init) state = NULL;
  if ((int16_t) ssn_state->local_channel >= 0 && !delivery->remote.settled
      && state && state->sent && !xfr_posted) {
    int err = pni_post_disp(transport, delivery);
    if (err) return err;
  }

  *settle = delivery->local.settled && state && state->sent;
  return 0;
}

static int pni_process_tpwork_receiver(pn_transport_t *transport, pn_delivery_t *delivery, bool *settle)
{
  *settle = false;
  pn_link_t *link = delivery->link;
  // XXX: need to prevent duplicate disposition sending
  pn_session_t *ssn = link->session;
  if ((int16_t) ssn->state.local_channel >= 0 && !delivery->remote.settled && delivery->state.init) {
    int err = pni_post_disp(transport, delivery);
    if (err) return err;
  }

  // XXX: need to centralize this policy and improve it
  if (!ssn->state.incoming_window) {
    int err = pni_post_flow(transport, ssn, link);
    if (err) return err;
  }

  *settle = delivery->local.settled;
  return 0;
}

static int pni_process_tpwork(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && !transport->close_sent)
  {
    pn_connection_t *conn = (pn_connection_t *) endpoint;
    pn_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      pn_delivery_t *tp_next = delivery->tpwork_next;
      bool settle = false;

      pn_link_t *link = delivery->link;
      pn_delivery_map_t *dm = NULL;
      if (pn_link_is_sender(link)) {
        dm = &link->session->state.outgoing;
        int err = pni_process_tpwork_sender(transport, delivery, &settle);
        if (err) return err;
      } else {
        dm = &link->session->state.incoming;
        int err = pni_process_tpwork_receiver(transport, delivery, &settle);
        if (err) return err;
      }

      if (settle) {
        pn_full_settle(dm, delivery);
      } else if (!pn_delivery_buffered(delivery)) {
        pn_clear_tpwork(delivery);
      }

      delivery = tp_next;
    }
  }

  return 0;
}

static int pni_process_flush_disp(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION) {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = &session->state;
    if ((int16_t) state->local_channel >= 0 && !transport->close_sent)
    {
      int err = pni_flush_disp(transport, session);
      if (err) return err;
    }
  }

  return 0;
}

static int pni_process_flow_sender(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SENDER && endpoint->state & PN_LOCAL_ACTIVE)
  {
    pn_link_t *snd = (pn_link_t *) endpoint;
    pn_session_t *ssn = snd->session;
    pn_link_state_t *state = &snd->state;
    if ((int16_t) ssn->state.local_channel >= 0 &&
        (int32_t) state->local_handle >= 0 &&
        snd->drain && snd->drained) {
      pn_delivery_t *tail = snd->unsettled_tail;
      if (!tail || !pn_delivery_buffered(tail)) {
        state->delivery_count += state->link_credit;
        state->link_credit = 0;
        snd->drained = 0;
        return pni_post_flow(transport, ssn, snd);
      }
    }
  }

  return 0;
}

static void pni_unmap_local_handle(pn_link_t *link) {
  pn_link_state_t *state = &link->state;
  uintptr_t handle = state->local_handle;
  state->local_handle = -2;
  if (pn_hash_get(link->session->state.local_handles, handle)) {
    pn_ep_decref(&link->endpoint);
  }
  // may delete link
  pn_hash_del(link->session->state.local_handles, handle);
}

static int pni_process_link_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SENDER || endpoint->type == RECEIVER)
  {
    pn_link_t *link = (pn_link_t *) endpoint;
    pn_session_t *session = link->session;
    pn_session_state_t *ssn_state = &session->state;
    pn_link_state_t *state = &link->state;
    if (((endpoint->state & PN_LOCAL_CLOSED) || link->detached) && (int32_t) state->local_handle >= 0 &&
        (int16_t) ssn_state->local_channel >= 0 && !transport->close_sent) {
      if (pn_link_is_sender(link) && pn_link_queued(link) &&
          (int32_t) state->remote_handle != -2 &&
          (int16_t) ssn_state->remote_channel != -2 &&
          !transport->close_rcvd) return 0;

      const char *name = NULL;
      const char *description = NULL;
      pn_data_t *info = NULL;

      if (pn_condition_is_set(&endpoint->condition)) {
        name = pn_condition_get_name(&endpoint->condition);
        description = pn_condition_get_description(&endpoint->condition);
        info = pn_condition_info(&endpoint->condition);
      }

      int err =
          pn_post_frame(transport, AMQP_FRAME_TYPE, ssn_state->local_channel,
                        "DL[I?o?DL[sSC]]", DETACH, state->local_handle,
                        !link->detached, !link->detached,
                        (bool)name, ERROR, name, description, info);
      if (err) return err;
      pni_unmap_local_handle(link);
    }

    pn_clear_modified(transport->connection, endpoint);
  }

  return 0;
}

static bool pni_pointful_buffering(pn_transport_t *transport, pn_session_t *session)
{
  if (transport->close_rcvd) return false;
  if (!transport->open_rcvd) return true;

  pn_connection_t *conn = transport->connection;
  pn_link_t *link = pn_link_head(conn, 0);
  while (link) {
    if (pn_link_is_sender(link) && pn_link_queued(link) > 0) {
      pn_session_t *ssn = link->session;
      if (session && session == ssn) {
        if ((int32_t) link->state.remote_handle != -2 &&
            (int16_t) session->state.remote_channel != -2) {
          return true;
        }
      }
    }
    link = pn_link_next(link, 0);
  }

  return false;
}

static void pni_unmap_local_channel(pn_session_t *ssn) {
  // XXX: should really update link state also
  pni_delivery_map_clear(&ssn->state.outgoing);
  pni_transport_unbind_handles(ssn->state.local_handles, false);
  pn_transport_t *transport = ssn->connection->transport;
  pn_session_state_t *state = &ssn->state;
  uintptr_t channel = state->local_channel;
  state->local_channel = -2;
  if (pn_hash_get(transport->local_channels, channel)) {
    pn_ep_decref(&ssn->endpoint);
  }
  // may delete session
  pn_hash_del(transport->local_channels, channel);
}

static int pni_process_ssn_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = &session->state;
    if (endpoint->state & PN_LOCAL_CLOSED && (int16_t) state->local_channel >= 0
        && !transport->close_sent)
    {
      if (pni_pointful_buffering(transport, session)) {
        return 0;
      }

      const char *name = NULL;
      const char *description = NULL;
      pn_data_t *info = NULL;

      if (pn_condition_is_set(&endpoint->condition)) {
        name = pn_condition_get_name(&endpoint->condition);
        description = pn_condition_get_description(&endpoint->condition);
        info = pn_condition_info(&endpoint->condition);
      }

      int err = pn_post_frame(transport, AMQP_FRAME_TYPE, state->local_channel, "DL[?DL[sSC]]", END,
                              (bool) name, ERROR, name, description, info);
      if (err) return err;
      pni_unmap_local_channel(session);
    }

    pn_clear_modified(transport->connection, endpoint);
  }
  return 0;
}

static int pni_process_conn_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (endpoint->state & PN_LOCAL_CLOSED && !transport->close_sent) {
      if (pni_pointful_buffering(transport, NULL)) return 0;
      int err = pni_post_close(transport, NULL);
      if (err) return err;
      transport->close_sent = true;
    }

    pn_clear_modified(transport->connection, endpoint);
  }
  return 0;
}

static int pni_phase(pn_transport_t *transport, int (*phase)(pn_transport_t *, pn_endpoint_t *))
{
  pn_connection_t *conn = transport->connection;
  pn_endpoint_t *endpoint = conn->transport_head;
  while (endpoint)
  {
    pn_endpoint_t *next = endpoint->transport_next;
    int err = phase(transport, endpoint);
    if (err) return err;
    endpoint = next;
  }
  return 0;
}

static int pni_process(pn_transport_t *transport)
{
  int err;
  if ((err = pni_phase(transport, pni_process_conn_setup))) return err;
  if ((err = pni_phase(transport, pni_process_ssn_setup))) return err;
  if ((err = pni_phase(transport, pni_process_link_setup))) return err;
  if ((err = pni_phase(transport, pni_process_flow_receiver))) return err;

  // XXX: this has to happen two times because we might settle stuff
  // on the first pass and create space for more work to be done on the
  // second pass
  if ((err = pni_phase(transport, pni_process_tpwork))) return err;
  if ((err = pni_phase(transport, pni_process_tpwork))) return err;

  if ((err = pni_phase(transport, pni_process_flush_disp))) return err;

  if ((err = pni_phase(transport, pni_process_flow_sender))) return err;
  if ((err = pni_phase(transport, pni_process_link_teardown))) return err;
  if ((err = pni_phase(transport, pni_process_ssn_teardown))) return err;
  if ((err = pni_phase(transport, pni_process_conn_teardown))) return err;

  if (transport->connection->tpwork_head) {
    pn_modified(transport->connection, &transport->connection->endpoint, false);
  }

  return 0;
}

#define AMQP_HEADER ("AMQP\x00\x01\x00\x00")

static void pn_error_amqp(pn_transport_t* transport, unsigned int layer)
{
  if (!transport->close_sent) {
    if (!transport->open_sent) {
      pn_post_frame(transport, AMQP_FRAME_TYPE, 0, "DL[S]", OPEN, "");
    }

    pni_post_close(transport, &transport->condition);
    transport->close_sent = true;
  }
  transport->halt = true;
  transport->done_processing = true;
}

static ssize_t pn_input_read_amqp_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  bool eos = pn_transport_capacity(transport)==PN_EOS;
  pni_protocol_type_t protocol = pni_sniff_header(bytes, available);
  switch (protocol) {
  case PNI_PROTOCOL_AMQP1:
    if (transport->io_layers[layer] == &amqp_read_header_layer) {
      transport->io_layers[layer] = &amqp_layer;
    } else {
      transport->io_layers[layer] = &amqp_write_header_layer;
    }
    if (transport->trace & PN_TRACE_FRM)
      pn_transport_logf(transport, "  <- %s", "AMQP");
    return 8;
  case PNI_PROTOCOL_INSUFFICIENT:
    if (!eos) return 0;
    /* Fallthru */
  default:
    break;
  }
  char quoted[1024];
  pn_quote_data(quoted, 1024, bytes, available);
  pn_do_error(transport, "amqp:connection:framing-error",
              "%s header mismatch: %s ['%s']%s", "AMQP", pni_protocol_name(protocol), quoted,
              !eos ? "" : " (connection aborted)");
  return PN_EOS;
}

static ssize_t pn_input_read_amqp(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  if (transport->close_rcvd) {
    if (available > 0) {
      pn_do_error(transport, "amqp:connection:framing-error", "data after close");
      return PN_EOS;
    }
  }

  if (!transport->close_rcvd && !available) {
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    return PN_EOS;
  }


  ssize_t n = pn_dispatcher_input(transport, bytes, available, true, &transport->halt);
  if (n < 0) {
    //return pn_error_set(transport->error, n, "dispatch error");
    return PN_EOS;
  } else if (transport->close_rcvd) {
    return PN_EOS;
  } else {
    return n;
  }
}

/* process AMQP related timer events */
static pn_timestamp_t pn_tick_amqp(pn_transport_t* transport, unsigned int layer, pn_timestamp_t now)
{
  pn_timestamp_t timeout = 0;

  if (transport->local_idle_timeout) {
    if (transport->dead_remote_deadline == 0 ||
        transport->last_bytes_input != transport->bytes_input) {
      transport->dead_remote_deadline = now + transport->local_idle_timeout;
      transport->last_bytes_input = transport->bytes_input;
    } else if (transport->dead_remote_deadline <= now) {
      transport->dead_remote_deadline = now + transport->local_idle_timeout;
      if (!transport->posted_idle_timeout) {
        transport->posted_idle_timeout = true;
        // Note: AMQP-1.0 really should define a generic "timeout" error, but does not.
        pn_do_error(transport, "amqp:resource-limit-exceeded", "local-idle-timeout expired");
      }
    }
    timeout = transport->dead_remote_deadline;
  }

  // Prevent remote idle timeout as describe by AMQP 1.0:
  if (transport->remote_idle_timeout && !transport->close_sent) {
    if (transport->keepalive_deadline == 0 ||
        transport->last_bytes_output != transport->bytes_output) {
      transport->keepalive_deadline = now + (pn_timestamp_t)(transport->remote_idle_timeout/2.0);
      transport->last_bytes_output = transport->bytes_output;
    } else if (transport->keepalive_deadline <= now) {
      transport->keepalive_deadline = now + (pn_timestamp_t)(transport->remote_idle_timeout/2.0);
      if (pn_buffer_size(transport->output_buffer) == 0) {    // no outbound data pending
        // so send empty frame (and account for it!)
        pn_post_frame(transport, AMQP_FRAME_TYPE, 0, "");
        transport->last_bytes_output += pn_buffer_size(transport->output_buffer);
      }
    }
    timeout = pn_timestamp_min( timeout, transport->keepalive_deadline );
  }

  return timeout;
}

static ssize_t pn_output_write_amqp_header(pn_transport_t* transport, unsigned int layer, char* bytes, size_t available)
{
  if (transport->trace & PN_TRACE_FRM)
    pn_transport_logf(transport, "  -> %s", "AMQP");
  assert(available >= 8);
  memmove(bytes, AMQP_HEADER, 8);
  if (pn_condition_is_set(&transport->condition)) {
      pn_error_amqp(transport, layer);
    transport->io_layers[layer] = &pni_error_layer;
    return pn_dispatcher_output(transport, bytes+8, available-8) + 8;
  }

  if (transport->io_layers[layer] == &amqp_write_header_layer) {
    transport->io_layers[layer] = &amqp_layer;
  } else {
    transport->io_layers[layer] = &amqp_read_header_layer;
  }
  return 8;
}

static ssize_t pn_output_write_amqp(pn_transport_t* transport, unsigned int layer, char* bytes, size_t available)
{
  if (transport->connection && !transport->done_processing) {
    int err = pni_process(transport);
    if (err) {
      pn_transport_logf(transport, "process error %i", err);
      transport->done_processing = true;
    }
  }

  // write out any buffered data _before_ returning PN_EOS, else we
  // could truncate an outgoing Close frame containing a useful error
  // status
  if (!pn_buffer_size(transport->output_buffer) && transport->close_sent) {
    return PN_EOS;
  }

  return pn_dispatcher_output(transport, bytes, available);
}

// Mark transport output as closed and send event
static void pni_close_head(pn_transport_t *transport)
{
  if (!transport->head_closed) {
    transport->head_closed = true;
    pn_collector_t *collector = pni_transport_collector(transport);
    pn_collector_put(collector, PN_OBJECT, transport, PN_TRANSPORT_HEAD_CLOSED);
    pni_maybe_post_closed(transport);
  }
}

// generate outbound data, return amount of pending output else error
static ssize_t transport_produce(pn_transport_t *transport)
{
  if (transport->head_closed) return PN_EOS;

  ssize_t space = transport->output_size - transport->output_pending;

  if (space <= 0) {     // can we expand the buffer?
    int more = 0;
    if (!transport->remote_max_frame)   // no limit, so double it
      more = transport->output_size;
    else if (transport->remote_max_frame > transport->output_size)
      more = pn_min(transport->output_size, transport->remote_max_frame - transport->output_size);
    if (more) {
      char *newbuf = (char *)realloc( transport->output_buf, transport->output_size + more );
      if (newbuf) {
        transport->output_buf = newbuf;
        transport->output_size += more;
        space += more;
      }
    }
  }

  while (space > 0) {
    ssize_t n;
    n = transport->io_layers[0]->
      process_output( transport, 0,
                      &transport->output_buf[transport->output_pending],
                      space );
    if (n > 0) {
      space -= n;
      transport->output_pending += n;
    } else if (n == 0) {
      break;
    } else {
      if (transport->output_pending)
        break;   // return what is available
      if (transport->trace & (PN_TRACE_RAW | PN_TRACE_FRM)) {
        pn_transport_log(transport, "  -> EOS");
      }
      pni_close_head(transport);
      return n;
    }
  }

  return transport->output_pending;
}

// deprecated
ssize_t pn_transport_output(pn_transport_t *transport, char *bytes, size_t size)
{
  if (!transport) return PN_ARG_ERR;
  ssize_t available = pn_transport_pending(transport);
  if (available > 0) {
    available = (ssize_t) pn_min( (size_t)available, size );
    memmove( bytes, pn_transport_head(transport), available );
    pn_transport_pop( transport, (size_t) available );
  }
  return available;
}


void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace)
{
  transport->trace = trace;
}

void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer)
{
  assert(transport);
  assert(tracer);

  transport->tracer = tracer;
}

pn_tracer_t pn_transport_get_tracer(pn_transport_t *transport)
{
  assert(transport);
  return transport->tracer;
}

void pn_transport_set_context(pn_transport_t *transport, void *context)
{
  assert(transport);
  pn_record_set(transport->context, PN_LEGCTX, context);
}

void *pn_transport_get_context(pn_transport_t *transport)
{
  assert(transport);
  return pn_record_get(transport->context, PN_LEGCTX);
}

pn_record_t *pn_transport_attachments(pn_transport_t *transport)
{
  assert(transport);
  return transport->context;
}

void pn_transport_log(pn_transport_t *transport, const char *message)
{
  assert(transport);
  transport->tracer(transport, message);
}

void pn_transport_vlogf(pn_transport_t *transport, const char *fmt, va_list ap)
{
  if (transport) {
    pn_string_vformat(transport->scratch, fmt, ap);
    pn_transport_log(transport, pn_string_get(transport->scratch));
  } else {
    pn_vlogf(fmt, ap);
  }
}

void pn_transport_logf(pn_transport_t *transport, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  pn_transport_vlogf(transport, fmt, ap);
  va_end(ap);
}

uint16_t pn_transport_get_channel_max(pn_transport_t *transport)
{
  return transport->channel_max;
}

int pn_transport_set_channel_max(pn_transport_t *transport, uint16_t requested_channel_max)
{
  /*
   * Once the OPEN frame has been sent, we have communicated our
   * wishes to the remote client and there is no way to renegotiate.
   * After that point, we do not allow the application to make changes.
   * Before that point, however, the app is free to either raise or
   * lower our local limit.  (But the app cannot raise it above the
   * limit imposed by this library.)
   * The channel-max value will be finalized just before the OPEN frame
   * is sent.
   */
  if(transport->open_sent) {
    pn_transport_logf(transport, "Cannot change local channel-max after OPEN frame sent.");
    return PN_STATE_ERR;
  }

  transport->local_channel_max = (requested_channel_max < PN_IMPL_CHANNEL_MAX)
                                 ? requested_channel_max
                                 : PN_IMPL_CHANNEL_MAX;
  pni_calculate_channel_max(transport);

  return PN_OK;
}

uint16_t pn_transport_remote_channel_max(pn_transport_t *transport)
{
  return transport->remote_channel_max;
}

uint32_t pn_transport_get_max_frame(pn_transport_t *transport)
{
  return transport->local_max_frame;
}

void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size)
{
  // if size == 0, no advertised limit to input frame size.
  if (size && size < AMQP_MIN_MAX_FRAME_SIZE)
    size = AMQP_MIN_MAX_FRAME_SIZE;
  transport->local_max_frame = size;
}

uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport)
{
  return transport->remote_max_frame;
}

pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport)
{
  return transport->local_idle_timeout;
}

void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout)
{
  transport->local_idle_timeout = timeout;
}

pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport)
{
  return transport->remote_idle_timeout;
}

pn_timestamp_t pn_transport_tick(pn_transport_t *transport, pn_timestamp_t now)
{
  pn_timestamp_t r = 0;
  for (int i = 0; i<PN_IO_LAYER_CT; ++i) {
    if (transport->io_layers[i] && transport->io_layers[i]->process_tick)
      r = pn_timestamp_min(r, transport->io_layers[i]->process_tick(transport, i, now));
  }
  return r;
}

uint64_t pn_transport_get_frames_output(const pn_transport_t *transport)
{
  if (transport)
    return transport->output_frames_ct;
  return 0;
}

uint64_t pn_transport_get_frames_input(const pn_transport_t *transport)
{
  if (transport)
    return transport->input_frames_ct;
  return 0;
}

// input
ssize_t pn_transport_capacity(pn_transport_t *transport)  /* <0 == done */
{
  if (transport->tail_closed) return PN_EOS;
  //if (pn_error_code(transport->error)) return pn_error_code(transport->error);

  ssize_t capacity = transport->input_size - transport->input_pending;
  if ( capacity<=0 ) {
    // can we expand the size of the input buffer?
    int more = 0;
    if (!transport->local_max_frame) {  // no limit (ha!)
      more = transport->input_size;
    } else if (transport->local_max_frame > transport->input_size) {
      more = pn_min(transport->input_size, transport->local_max_frame - transport->input_size);
    }
    if (more) {
      char *newbuf = (char *) realloc( transport->input_buf, transport->input_size + more );
      if (newbuf) {
        transport->input_buf = newbuf;
        transport->input_size += more;
        capacity += more;
      }
    }
  }
  return capacity;
}


char *pn_transport_tail(pn_transport_t *transport)
{
  if (transport && transport->input_pending < transport->input_size) {
    return &transport->input_buf[transport->input_pending];
  }
  return NULL;
}

ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size)
{
  assert(transport);

  ssize_t capacity = pn_transport_capacity(transport);
  if (capacity < 0) {
    return capacity;
  } else if (size > (size_t) capacity) {
    size = capacity;
  }

  char *dst = pn_transport_tail(transport);
  assert(dst);
  memmove(dst, src, size);

  int n = pn_transport_process(transport, size);
  if (n < 0) {
    return n;
  } else {
    return size;
  }
}

int pn_transport_process(pn_transport_t *transport, size_t size)
{
  assert(transport);
  size = pn_min( size, (transport->input_size - transport->input_pending) );
  transport->input_pending += size;
  transport->bytes_input += size;

  ssize_t n = transport_consume( transport );
  if (n == PN_EOS) {
    pni_close_tail(transport);
  }

  if (n < 0 && n != PN_EOS) return n;
  return 0;
}

// input stream has closed
int pn_transport_close_tail(pn_transport_t *transport)
{
  pni_close_tail(transport);
  transport_consume( transport );
  return 0;
  // XXX: what if not all input processed at this point?  do we care???
}

// output
ssize_t pn_transport_pending(pn_transport_t *transport)      /* <0 == done */
{
  assert(transport);
  return transport_produce( transport );
}

const char *pn_transport_head(pn_transport_t *transport)
{
  if (transport && transport->output_pending) {
    return transport->output_buf;
  }
  return NULL;
}

ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size)
{
  assert(transport);

  ssize_t pending = pn_transport_pending(transport);
  if (pending < 0) {
    return pending;
  } else if (size > (size_t) pending) {
    size = pending;
  }

  if (pending > 0) {
    const char *src = pn_transport_head(transport);
    assert(src);
    memmove(dst, src, size);
  }

  return size;
}

void pn_transport_pop(pn_transport_t *transport, size_t size)
{
  if (transport) {
    assert( transport->output_pending >= size );
    transport->output_pending -= size;
    transport->bytes_output += size;
    if (transport->output_pending) {
      memmove( transport->output_buf,  &transport->output_buf[size],
               transport->output_pending );
    }

    if (transport->output_pending==0 && pn_transport_pending(transport) < 0) {
      // TODO: It looks to me that this is a NOP as iff we ever get here
      // TODO: pni_close_head() will always have been already called before leaving pn_transport_pending()
      pni_close_head(transport);
    }
  }
}

int pn_transport_close_head(pn_transport_t *transport)
{
  ssize_t pending = pn_transport_pending(transport);
  pni_close_head(transport);
  if (pending > 0)
    pn_transport_pop(transport, pending);
  return 0;
}

// true if the transport will not generate further output
bool pn_transport_quiesced(pn_transport_t *transport)
{
  if (!transport) return true;
  ssize_t pending = pn_transport_pending(transport);
  if (pending < 0) return true; // output done
  else if (pending > 0) return false;
  // no pending at transport, but check if data is buffered in I/O layers
  for (int layer = 0; layer<PN_IO_LAYER_CT; ++layer) {
    if (transport->io_layers[layer] &&
        transport->io_layers[layer]->buffered_output &&
        transport->io_layers[layer]->buffered_output( transport ))
      return false;
  }
  return true;
}

bool pn_transport_head_closed(pn_transport_t *transport) { return transport->head_closed; }

bool pn_transport_tail_closed(pn_transport_t *transport) { return transport->tail_closed; }

bool pn_transport_closed(pn_transport_t *transport) {
  return transport->head_closed && transport->tail_closed;
}

pn_connection_t *pn_transport_connection(pn_transport_t *transport)
{
  assert(transport);
  return transport->connection;
}
