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

#include "../engine/engine-internal.h"
#include <stdlib.h>
#include <string.h>
#include <proton/framing.h>
#include "protocol.h"
#include "dispatch_actions.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "../engine/event.h"

#include "../sasl/sasl-internal.h"
#include "../ssl/ssl-internal.h"
#include "../platform.h"
#include "../platform_fmt.h"

static ssize_t transport_consume(pn_transport_t *transport);

// delivery buffers

void pn_delivery_map_init(pn_delivery_map_t *db, pn_sequence_t next)
{
  db->deliveries = pn_hash(0, 0.75, PN_REFCOUNT);
  db->next = next;
}

void pn_delivery_map_free(pn_delivery_map_t *db)
{
  pn_free(db->deliveries);
}

pn_delivery_t *pn_delivery_map_get(pn_delivery_map_t *db, pn_sequence_t id)
{
  return (pn_delivery_t *) pn_hash_get(db->deliveries, id);
}

static void pn_delivery_state_init(pn_delivery_state_t *ds, pn_delivery_t *delivery, pn_sequence_t id)
{
  ds->id = id;
  ds->sent = false;
  ds->init = true;
}

pn_delivery_state_t *pn_delivery_map_push(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  pn_delivery_state_t *ds = &delivery->state;
  pn_delivery_state_init(ds, delivery, db->next++);
  pn_hash_put(db->deliveries, ds->id, delivery);
  return ds;
}

void pn_delivery_map_del(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  if (delivery->state.init) {
    pn_hash_del(db->deliveries, delivery->state.id);
  }
  delivery->state.init = false;
  delivery->state.sent = false;
}

void pn_delivery_map_clear(pn_delivery_map_t *dm)
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

static ssize_t pn_input_read_amqp_header(pn_io_layer_t *io_layer, const char *bytes, size_t available);
static ssize_t pn_input_read_amqp(pn_io_layer_t *io_layer, const char *bytes, size_t available);
static ssize_t pn_output_write_amqp_header(pn_io_layer_t *io_layer, char *bytes, size_t available);
static ssize_t pn_output_write_amqp(pn_io_layer_t *io_layer, char *bytes, size_t available);
static pn_timestamp_t pn_tick_amqp(pn_io_layer_t *io_layer, pn_timestamp_t now);

static void pni_default_tracer(pn_transport_t *transport, const char *message)
{
  fprintf(stderr, "[%p]:%s\n", (void *) transport, message);
}

static void pn_transport_initialize(void *object)
{
  pn_transport_t *transport = (pn_transport_t *)object;
  transport->freed = false;
  transport->output_buf = NULL;
  transport->output_size = PN_DEFAULT_MAX_FRAME_SIZE ? PN_DEFAULT_MAX_FRAME_SIZE : 16 * 1024;
  transport->input_buf = NULL;
  transport->input_size =  PN_DEFAULT_MAX_FRAME_SIZE ? PN_DEFAULT_MAX_FRAME_SIZE : 16 * 1024;
  transport->tracer = pni_default_tracer;
  transport->header_count = 0;
  transport->sasl = NULL;
  transport->ssl = NULL;
  transport->scratch = pn_string(NULL);
  transport->disp = pn_dispatcher(0, transport);
  transport->connection = NULL;

  pn_io_layer_t *io_layer = transport->io_layers;
  while (io_layer != &transport->io_layers[PN_IO_AMQP]) {
    io_layer->context = NULL;
    io_layer->next = io_layer + 1;
    io_layer->process_input = pn_io_layer_input_passthru;
    io_layer->process_output = pn_io_layer_output_passthru;
    io_layer->process_tick = pn_io_layer_tick_passthru;
    io_layer->buffered_output = NULL;
    io_layer->buffered_input = NULL;
    ++io_layer;
  }

  pn_io_layer_t *amqp = &transport->io_layers[PN_IO_AMQP];
  amqp->context = transport;
  amqp->process_input = pn_input_read_amqp_header;
  amqp->process_output = pn_output_write_amqp_header;
  amqp->process_tick = pn_io_layer_tick_passthru;
  amqp->buffered_output = NULL;
  amqp->buffered_input = NULL;
  amqp->next = NULL;

  transport->open_sent = false;
  transport->open_rcvd = false;
  transport->close_sent = false;
  transport->close_rcvd = false;
  transport->tail_closed = false;
  transport->head_closed = false;
  transport->remote_container = NULL;
  transport->remote_hostname = NULL;
  transport->local_max_frame = PN_DEFAULT_MAX_FRAME_SIZE;
  transport->remote_max_frame = 0;
  transport->channel_max = 0;
  transport->remote_channel_max = 0;
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

  transport->local_channels = pn_hash(0, 0.75, PN_REFCOUNT);
  transport->remote_channels = pn_hash(0, 0.75, PN_REFCOUNT);

  transport->bytes_input = 0;
  transport->bytes_output = 0;

  transport->input_pending = 0;
  transport->output_pending = 0;

  transport->done_processing = false;
}

pn_session_t *pn_channel_state(pn_transport_t *transport, uint16_t channel)
{
  return (pn_session_t *) pn_hash_get(transport->remote_channels, channel);
}

static void pn_map_channel(pn_transport_t *transport, uint16_t channel, pn_session_t *session)
{
  pn_hash_put(transport->remote_channels, channel, session);
  session->state.remote_channel = channel;
}

void pn_unmap_channel(pn_transport_t *transport, pn_session_t *ssn)
{
  uint16_t channel = ssn->state.remote_channel;
  ssn->state.remote_channel = -2;
  // note: may free the session:
  pn_hash_del(transport->remote_channels, channel);
}


static void pn_transport_finalize(void *object);
#define pn_transport_hashcode NULL
#define pn_transport_compare NULL
#define pn_transport_inspect NULL

pn_transport_t *pn_transport()
{
  static const pn_class_t clazz = PN_CLASS(pn_transport);
  pn_transport_t *transport = (pn_transport_t *) pn_new(sizeof(pn_transport_t),
                                                        &clazz);
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
  return transport;
}

void pn_transport_free(pn_transport_t *transport)
{
  if (!transport) return;
  assert(!transport->freed);
  transport->freed = true;
  // once the application frees the transport, no further I/O
  // processing can be done to the connection:
  pn_transport_unbind(transport);
  pn_decref(transport);
}

static void pn_transport_finalize(void *object)
{
  pn_transport_t *transport = (pn_transport_t *) object;

  pn_ssl_free(transport->ssl);
  pn_sasl_free(transport->sasl);
  pn_dispatcher_free(transport->disp);
  free(transport->remote_container);
  free(transport->remote_hostname);
  pn_free(transport->remote_offered_capabilities);
  pn_free(transport->remote_desired_capabilities);
  pn_free(transport->remote_properties);
  pn_free(transport->disp_data);
  pn_condition_tini(&transport->remote_condition);
  pn_free(transport->local_channels);
  pn_free(transport->remote_channels);
  if (transport->input_buf) free(transport->input_buf);
  if (transport->output_buf) free(transport->output_buf);
  pn_free(transport->scratch);
}

int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection)
{
  if (!transport) return PN_ARG_ERR;
  if (transport->connection) return PN_STATE_ERR;
  if (connection->transport) return PN_STATE_ERR;
  transport->connection = connection;
  connection->transport = transport;
  pn_incref2(connection, transport);
  if (transport->open_rcvd) {
    PN_SET_REMOTE(connection->endpoint.state, PN_REMOTE_ACTIVE);
    pn_collector_put(connection->collector, PN_CONNECTION_REMOTE_OPEN, connection);
    transport->disp->halt = false;
    transport_consume(transport);        // blech - testBindAfterOpen
  }
  return 0;
}

void pni_transport_unbind_handles(pn_hash_t *handles)
{
  for (pn_handle_t h = pn_hash_head(handles); h; h = pn_hash_next(handles, h)) {
    uintptr_t key = pn_hash_key(handles, h);
    pn_link_t *link = (pn_link_t *) pn_hash_value(handles, h);
    pn_link_unbound(link);
    pn_hash_del(handles, key);
  }
}

void pni_transport_unbind_channels(pn_hash_t *channels)
{
  for (pn_handle_t h = pn_hash_head(channels); h; h = pn_hash_next(channels, h)) {
    uintptr_t key = pn_hash_key(channels, h);
    pn_session_t *ssn = (pn_session_t *) pn_hash_value(channels, h);
    pni_transport_unbind_handles(ssn->state.local_handles);
    pni_transport_unbind_handles(ssn->state.remote_handles);
    pn_session_unbound(ssn);
    pn_hash_del(channels, key);
  }
}

int pn_transport_unbind(pn_transport_t *transport)
{
  assert(transport);
  if (!transport->connection) return 0;

  pn_connection_t *conn = transport->connection;
  transport->connection = NULL;

  // XXX: what happens if the endpoints are freed before we get here?
  pn_session_t *ssn = pn_session_head(conn, 0);
  while (ssn) {
    pn_delivery_map_clear(&ssn->state.incoming);
    pn_delivery_map_clear(&ssn->state.outgoing);
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
  pn_decref2(conn, transport);
  return 0;
}

pn_error_t *pn_transport_error(pn_transport_t *transport)
{
  return NULL;
}

static void pn_map_handle(pn_session_t *ssn, uint32_t handle, pn_link_t *link)
{
  link->state.remote_handle = handle;
  pn_hash_put(ssn->state.remote_handles, handle, link);
}

void pn_unmap_handle(pn_session_t *ssn, pn_link_t *link)
{
  uint32_t handle = link->state.remote_handle;
  link->state.remote_handle = -2;
  // may delete link:
  pn_hash_del(ssn->state.remote_handles, handle);
}

pn_link_t *pn_handle_state(pn_session_t *ssn, uint32_t handle)
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

void pni_disposition_encode(pn_disposition_t *disposition, pn_data_t *data)
{
  pn_condition_t *cond = &disposition->condition;
  switch (disposition->type) {
  case PN_RECEIVED:
    pn_data_put_list(data);
    pn_data_enter(data);
    pn_data_put_uint(data, disposition->section_number);
    pn_data_put_ulong(data, disposition->section_offset);
    pn_data_exit(data);
    break;
  case PN_ACCEPTED:
  case PN_RELEASED:
    return;
  case PN_REJECTED:
    pn_data_fill(data, "[?DL[sSC]]", pn_condition_is_set(cond), ERROR,
                 pn_condition_get_name(cond),
                 pn_condition_get_description(cond),
                 pn_condition_info(cond));
    break;
  case PN_MODIFIED:
    pn_data_fill(data, "[ooC]",
                 disposition->failed,
                 disposition->undeliverable,
                 disposition->annotations);
    break;
  default:
    pn_data_copy(data, disposition->data);
    break;
  }
}

int pn_post_close(pn_transport_t *transport, const char *condition, const char *description)
{
  pn_condition_t *cond = NULL;
  if (transport->connection) {
    cond = pn_connection_condition(transport->connection);
  }
  pn_data_t *info = NULL;
  if (!condition && pn_condition_is_set(cond)) {
    condition = pn_condition_get_name(cond);
    description = pn_condition_get_description(cond);
    info = pn_condition_info(cond);
  }

  return pn_post_frame(transport->disp, 0, "DL[?DL[sSC]]", CLOSE,
                       (bool) condition, ERROR, condition, description, info);
}

int pn_do_error(pn_transport_t *transport, const char *condition, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[1024];
  // XXX: result
  vsnprintf(buf, 1024, fmt, ap);
  va_end(ap);
  if (!transport->close_sent) {
    if (!transport->open_sent) {
      pn_post_frame(transport->disp, 0, "DL[S]", OPEN, "");
    }

    pn_post_close(transport, condition, buf);
    transport->close_sent = true;
  }
  transport->disp->halt = true;
  pn_transport_logf(transport, "ERROR %s %s", condition, buf);
  return PN_ERR;
}

static char *pn_bytes_strdup(pn_bytes_t str)
{
  return pn_strndup(str.start, str.size);
}

int pn_do_open(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  pn_connection_t *conn = transport->connection;
  bool container_q, hostname_q;
  pn_bytes_t remote_container, remote_hostname;
  pn_data_clear(transport->remote_offered_capabilities);
  pn_data_clear(transport->remote_desired_capabilities);
  pn_data_clear(transport->remote_properties);
  int err = pn_scan_args(disp, "D.[?S?SIHI..CCC]", &container_q,
                         &remote_container, &hostname_q, &remote_hostname,
                         &transport->remote_max_frame,
                         &transport->remote_channel_max,
                         &transport->remote_idle_timeout,
                         transport->remote_offered_capabilities,
                         transport->remote_desired_capabilities,
                         transport->remote_properties);
  if (err) return err;
  if (transport->remote_max_frame > 0) {
    if (transport->remote_max_frame < AMQP_MIN_MAX_FRAME_SIZE) {
      pn_transport_logf(transport, "Peer advertised bad max-frame (%u), forcing to %u",
                        transport->remote_max_frame, AMQP_MIN_MAX_FRAME_SIZE);
      transport->remote_max_frame = AMQP_MIN_MAX_FRAME_SIZE;
    }
    disp->remote_max_frame = transport->remote_max_frame;
    pn_buffer_clear( disp->frame );
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
    pn_collector_put(conn->collector, PN_CONNECTION_REMOTE_OPEN, conn);
  } else {
    transport->disp->halt = true;
  }
  if (transport->remote_idle_timeout)
    transport->io_layers[PN_IO_AMQP].process_tick = pn_tick_amqp;  // enable timeouts
  transport->open_rcvd = true;
  return 0;
}

int pn_do_begin(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  bool reply;
  uint16_t remote_channel;
  pn_sequence_t next;
  int err = pn_scan_args(disp, "D.[?HI]", &reply, &remote_channel, &next);
  if (err) return err;

  pn_session_t *ssn;
  if (reply) {
    // XXX: what if session is NULL?
    ssn = (pn_session_t *) pn_hash_get(transport->local_channels, remote_channel);
  } else {
    ssn = pn_session(transport->connection);
  }
  ssn->state.incoming_transfer_count = next;
  pn_map_channel(transport, disp->channel, ssn);
  PN_SET_REMOTE(ssn->endpoint.state, PN_REMOTE_ACTIVE);
  pn_collector_put(transport->connection->collector, PN_SESSION_REMOTE_OPEN, ssn);
  return 0;
}

pn_link_t *pn_find_link(pn_session_t *ssn, pn_bytes_t name, bool is_sender)
{
  pn_endpoint_type_t type = is_sender ? SENDER : RECEIVER;

  for (size_t i = 0; i < pn_list_size(ssn->links); i++)
  {
    pn_link_t *link = (pn_link_t *) pn_list_get(ssn->links, i);
    if (link->endpoint.type == type &&
        !strncmp(name.start, pn_string_get(link->name), name.size))
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

  if (!strncmp(symbol.start, "link-detach", symbol.size))
    return PN_EXPIRE_WITH_LINK;
  if (!strncmp(symbol.start, "session-end", symbol.size))
    return PN_EXPIRE_WITH_SESSION;
  if (!strncmp(symbol.start, "connection-close", symbol.size))
    return PN_EXPIRE_WITH_CONNECTION;
  if (!strncmp(symbol.start, "never", symbol.size))
    return PN_EXPIRE_NEVER;

  return PN_EXPIRE_WITH_SESSION;
}

static pn_distribution_mode_t symbol2dist_mode(const pn_bytes_t symbol)
{
  if (!symbol.start)
    return PN_DIST_MODE_UNSPECIFIED;

  if (!strncmp(symbol.start, "move", symbol.size))
    return PN_DIST_MODE_MOVE;
  if (!strncmp(symbol.start, "copy", symbol.size))
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

int pn_do_attach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
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
  int err = pn_scan_args(disp, "D.[SIo?B?BD.[SIsIo.s]D.[SIsIo]..I]", &name, &handle,
                         &is_sender,
                         &snd_settle, &snd_settle_mode,
                         &rcv_settle, &rcv_settle_mode,
                         &source, &src_dr, &src_exp, &src_timeout, &src_dynamic, &dist_mode,
                         &target, &tgt_dr, &tgt_exp, &tgt_timeout, &tgt_dynamic,
                         &idc);
  if (err) return err;
  char strbuf[128];      // avoid malloc for most link names
  char *strheap = (name.size >= sizeof(strbuf)) ? (char *) malloc(name.size + 1) : NULL;
  char *strname = strheap ? strheap : strbuf;
  strncpy(strname, name.start, name.size);
  strname[name.size] = '\0';

  pn_session_t *ssn = pn_channel_state(transport, disp->channel);
  if (!ssn) {
      pn_do_error(transport, "amqp:connection:no-session", "attach without a session");
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

  pn_map_handle(ssn, handle, link);
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
    pn_terminus_set_type(rtgt, PN_UNSPECIFIED);
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

  err = pn_scan_args(disp, "D.[.....D.[.....C.C.CC]D.[.....CC]",
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

  pn_collector_put(transport->connection->collector, PN_LINK_REMOTE_OPEN, link);
  return 0;
}

int pn_post_flow(pn_transport_t *transport, pn_session_t *ssn, pn_link_t *link);

// free the delivery
static void pn_full_settle(pn_delivery_map_t *db, pn_delivery_t *delivery)
{
  assert(!delivery->work);
  pn_clear_tpwork(delivery);
  pn_real_settle(delivery);
}

int pn_do_transfer(pn_dispatcher_t *disp)
{
  // XXX: multi transfer
  pn_transport_t *transport = disp->transport;
  uint32_t handle;
  pn_bytes_t tag;
  bool id_present;
  pn_sequence_t id;
  bool settled;
  bool more;
  int err = pn_scan_args(disp, "D.[I?Iz.oo]", &handle, &id_present, &id, &tag,
                         &settled, &more);
  if (err) return err;
  pn_session_t *ssn = pn_channel_state(transport, disp->channel);

  if (!ssn->state.incoming_window) {
    return pn_do_error(transport, "amqp:session:window-violation", "incoming session window exceeded");
  }

  pn_link_t *link = pn_handle_state(ssn, handle);
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
    pn_delivery_state_t *state = pn_delivery_map_push(incoming, delivery);
    if (id_present && id != state->id) {
      int err = pn_do_error(transport, "amqp:session:invalid-field",
                            "sequencing error, expected delivery-id %u, got %u",
                            state->id, id);
      // XXX: this will probably leave delivery buffer state messed up
      pn_full_settle(incoming, delivery);
      return err;
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

  pn_buffer_append(delivery->bytes, disp->payload, disp->size);
  ssn->incoming_bytes += disp->size;
  delivery->done = !more;

  ssn->state.incoming_transfer_count++;
  ssn->state.incoming_window--;

  // XXX: need better policy for when to refresh window
  if (!ssn->state.incoming_window && (int32_t) link->state.local_handle >= 0) {
    pn_post_flow(transport, ssn, link);
  }

  pn_collector_put(transport->connection->collector, PN_DELIVERY, delivery);
  return 0;
}

int pn_do_flow(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  pn_sequence_t onext, inext, delivery_count;
  uint32_t iwin, owin, link_credit;
  uint32_t handle;
  bool inext_init, handle_init, dcount_init, drain;
  int err = pn_scan_args(disp, "D.[?IIII?I?II.o]", &inext_init, &inext, &iwin,
                         &onext, &owin, &handle_init, &handle, &dcount_init,
                         &delivery_count, &link_credit, &drain);
  if (err) return err;

  pn_session_t *ssn = pn_channel_state(transport, disp->channel);

  if (inext_init) {
    ssn->state.remote_incoming_window = inext + iwin - ssn->state.outgoing_transfer_count;
  } else {
    ssn->state.remote_incoming_window = iwin;
  }

  if (handle_init) {
    pn_link_t *link = pn_handle_state(ssn, handle);
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

    pn_collector_put(transport->connection->collector, PN_LINK_FLOW, link);
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

int pn_do_disposition(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  bool role;
  pn_sequence_t first, last;
  uint64_t type = 0;
  bool last_init, settled, type_init;
  pn_data_clear(transport->disp_data);
  int err = pn_scan_args(disp, "D.[oI?IoD?LC]", &role, &first, &last_init,
                         &last, &settled, &type_init, &type,
                         transport->disp_data);
  if (err) return err;
  if (!last_init) last = first;

  pn_session_t *ssn = pn_channel_state(transport, disp->channel);
  pn_delivery_map_t *deliveries;
  if (role) {
    deliveries = &ssn->state.outgoing;
  } else {
    deliveries = &ssn->state.incoming;
  }

  pn_data_rewind(transport->disp_data);
  bool remote_data = (pn_data_next(transport->disp_data) &&
                      pn_data_get_list(transport->disp_data) > 0);

  for (pn_sequence_t id = first; id <= last; id++) {
    pn_delivery_t *delivery = pn_delivery_map_get(deliveries, id);
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

      pn_collector_put(transport->connection->collector, PN_DELIVERY, delivery);
    }
  }

  return 0;
}

int pn_do_detach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  uint32_t handle;
  bool closed;
  int err = pn_scan_args(disp, "D.[Io]", &handle, &closed);
  if (err) return err;

  pn_session_t *ssn = pn_channel_state(transport, disp->channel);
  if (!ssn) {
    return pn_do_error(transport, "amqp:invalid-field", "no such channel: %u", disp->channel);
  }
  pn_link_t *link = pn_handle_state(ssn, handle);
  if (!link) {
    return pn_do_error(transport, "amqp:invalid-field", "no such handle: %u", handle);
  }

  err = pn_scan_error(disp->args, &link->endpoint.remote_condition, SCAN_ERROR_DETACH);
  if (err) return err;

  if (closed)
  {
    PN_SET_REMOTE(link->endpoint.state, PN_REMOTE_CLOSED);
    pn_collector_put(transport->connection->collector, PN_LINK_REMOTE_CLOSE, link);
  } else {
    // TODO: implement
  }

  pn_unmap_handle(ssn, link);
  return 0;
}

int pn_do_end(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  pn_session_t *ssn = pn_channel_state(transport, disp->channel);
  int err = pn_scan_error(disp->args, &ssn->endpoint.remote_condition, SCAN_ERROR_DEFAULT);
  if (err) return err;
  PN_SET_REMOTE(ssn->endpoint.state, PN_REMOTE_CLOSED);
  pn_collector_put(transport->connection->collector, PN_SESSION_REMOTE_CLOSE, ssn);
  pn_unmap_channel(transport, ssn);
  return 0;
}

int pn_do_close(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->transport;
  pn_connection_t *conn = transport->connection;
  int err = pn_scan_error(disp->args, &transport->remote_condition, SCAN_ERROR_DEFAULT);
  if (err) return err;
  transport->close_rcvd = true;
  PN_SET_REMOTE(conn->endpoint.state, PN_REMOTE_CLOSED);
  pn_collector_put(transport->connection->collector, PN_CONNECTION_REMOTE_CLOSE, conn);
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
  pn_io_layer_t *io_layer = transport->io_layers;
  size_t consumed = 0;

  while (transport->input_pending || transport->tail_closed) {
    ssize_t n;
    n = io_layer->process_input( io_layer,
                                 transport->input_buf + consumed,
                                 transport->input_pending );
    if (n > 0) {
      consumed += n;
      transport->input_pending -= n;
    } else if (n == 0) {
      break;
    } else {
      assert(n == PN_EOS);
      if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
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

static ssize_t pn_input_read_header(pn_transport_t *transport, const char *bytes, size_t available,
                                    const char *header, size_t size, const char *protocol,
                                    ssize_t (*next)(pn_io_layer_t *, const char *, size_t))
{
  const char *point = header + transport->header_count;
  int delta = pn_min(available, size - transport->header_count);
  if (!available || memcmp(bytes, point, delta)) {
    char quoted[1024];
    pn_quote_data(quoted, 1024, bytes, available);
    pn_do_error(transport, "amqp:connection:framing-error",
                "%s header mismatch: '%s'%s", protocol, quoted,
                available ? "" : " (connection aborted)");
    return PN_EOS;
  } else {
    transport->header_count += delta;
    if (transport->header_count == size) {
      transport->header_count = 0;
      transport->io_layers[PN_IO_AMQP].process_input = next;

      if (transport->disp->trace & PN_TRACE_FRM)
        pn_transport_logf(transport, "  <- %s", protocol);
    }
    return delta;
  }
}

#define AMQP_HEADER ("AMQP\x00\x01\x00\x00")

static ssize_t pn_input_read_amqp_header(pn_io_layer_t *io_layer, const char *bytes, size_t available)
{
  pn_transport_t *transport = (pn_transport_t *)io_layer->context;
  return pn_input_read_header(transport, bytes, available, AMQP_HEADER, 8,
                              "AMQP", pn_input_read_amqp);
}

static ssize_t pn_input_read_amqp(pn_io_layer_t *io_layer, const char *bytes, size_t available)
{
  pn_transport_t *transport = (pn_transport_t *)io_layer->context;
  if (transport->close_rcvd) {
    if (available > 0) {
      pn_do_error(transport, "amqp:connection:framing-error", "data after close");
      return PN_EOS;
    }
  }

  if (!available) {
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    return PN_EOS;
  }


  ssize_t n = pn_dispatcher_input(transport->disp, bytes, available);
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
static pn_timestamp_t pn_tick_amqp(pn_io_layer_t *io_layer, pn_timestamp_t now)
{
  pn_timestamp_t timeout = 0;
  pn_transport_t *transport = (pn_transport_t *)io_layer->context;

  if (transport->local_idle_timeout) {
    if (transport->dead_remote_deadline == 0 ||
        transport->last_bytes_input != transport->bytes_input) {
      transport->dead_remote_deadline = now + transport->local_idle_timeout;
      transport->last_bytes_input = transport->bytes_input;
    } else if (transport->dead_remote_deadline <= now) {
      transport->dead_remote_deadline = now + transport->local_idle_timeout;
      // Note: AMQP-1.0 really should define a generic "timeout" error, but does not.
      pn_do_error(transport, "amqp:resource-limit-exceeded", "local-idle-timeout expired");
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
      if (transport->disp->available == 0) {    // no outbound data pending
        // so send empty frame (and account for it!)
        pn_post_frame(transport->disp, 0, "");
        transport->last_bytes_output += transport->disp->available;
      }
    }
    timeout = pn_timestamp_min( timeout, transport->keepalive_deadline );
  }

  return timeout;
}

int pn_process_conn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
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
      int err = pn_post_frame(transport->disp, 0, "DL[SS?I?H?InnCCC]", OPEN,
                              cid ? cid : "",
                              pn_string_get(connection->hostname),
                              // if not zero, advertise our max frame size and idle timeout
                              (bool)transport->local_max_frame, transport->local_max_frame,
                              (bool)transport->channel_max, transport->channel_max,
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

static uint16_t allocate_alias(pn_hash_t *aliases)
{
  for (uint32_t i = 0; i < 65536; i++) {
    if (!pn_hash_get(aliases, i)) {
      return i;
    }
  }

  assert(false);
  return 0;
}

size_t pn_session_outgoing_window(pn_session_t *ssn)
{
  uint32_t size = ssn->connection->transport->remote_max_frame;
  if (!size) {
    return ssn->outgoing_deliveries;
  } else {
    pn_sequence_t frames = ssn->outgoing_bytes/size;
    if (ssn->outgoing_bytes % size) {
      frames++;
    }
    return pn_max(frames, ssn->outgoing_deliveries);
  }
}

size_t pn_session_incoming_window(pn_session_t *ssn)
{
  uint32_t size = ssn->connection->transport->local_max_frame;
  if (!size) {
    return 2147483647; // biggest legal value
  } else {
    return (ssn->incoming_capacity - ssn->incoming_bytes)/size;
  }
}

int pn_process_ssn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION && transport->open_sent)
  {
    pn_session_t *ssn = (pn_session_t *) endpoint;
    pn_session_state_t *state = &ssn->state;
    if (!(endpoint->state & PN_LOCAL_UNINIT) && state->local_channel == (uint16_t) -1)
    {
      uint16_t channel = allocate_alias(transport->local_channels);
      state->incoming_window = pn_session_incoming_window(ssn);
      state->outgoing_window = pn_session_outgoing_window(ssn);
      pn_post_frame(transport->disp, channel, "DL[?HIII]", BEGIN,
                    ((int16_t) state->remote_channel >= 0), state->remote_channel,
                    state->outgoing_transfer_count,
                    state->incoming_window,
                    state->outgoing_window);
      state->local_channel = channel;
      pn_hash_put(transport->local_channels, channel, ssn);
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

int pn_process_link_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
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
      state->local_handle = allocate_alias(ssn_state->local_handles);
      pn_hash_put(ssn_state->local_handles, state->local_handle, link);
      const pn_distribution_mode_t dist_mode = link->source.distribution_mode;
      int err = pn_post_frame(transport->disp, ssn_state->local_channel,
                              "DL[SIoBB?DL[SIsIoC?sCnCC]?DL[SIsIoCC]nnI]", ATTACH,
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
                              0);
      if (err) return err;
    }
  }

  return 0;
}

int pn_post_flow(pn_transport_t *transport, pn_session_t *ssn, pn_link_t *link)
{
  ssn->state.incoming_window = pn_session_incoming_window(ssn);
  ssn->state.outgoing_window = pn_session_outgoing_window(ssn);
  bool linkq = (bool) link;
  pn_link_state_t *state = &link->state;
  return pn_post_frame(transport->disp, ssn->state.local_channel, "DL[?IIII?I?I?In?o]", FLOW,
                       (int16_t) ssn->state.remote_channel >= 0, ssn->state.incoming_transfer_count,
                       ssn->state.incoming_window,
                       ssn->state.outgoing_transfer_count,
                       ssn->state.outgoing_window,
                       linkq, linkq ? state->local_handle : 0,
                       linkq, linkq ? state->delivery_count : 0,
                       linkq, linkq ? state->link_credit : 0,
                       linkq, linkq ? link->drain : false);
}

int pn_process_flow_receiver(pn_transport_t *transport, pn_endpoint_t *endpoint)
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
      return pn_post_flow(transport, ssn, rcv);
    }
  }

  return 0;
}

int pn_flush_disp(pn_transport_t *transport, pn_session_t *ssn)
{
  uint64_t code = ssn->state.disp_code;
  bool settled = ssn->state.disp_settled;
  if (ssn->state.disp) {
    int err = pn_post_frame(transport->disp, ssn->state.local_channel, "DL[oIIo?DL[]]", DISPOSITION,
                            ssn->state.disp_type, ssn->state.disp_first, ssn->state.disp_last,
                            settled, (bool)code, code);
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

int pn_post_disp(pn_transport_t *transport, pn_delivery_t *delivery)
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
    pni_disposition_encode(&delivery->local, transport->disp_data);
    return pn_post_frame(transport->disp, ssn->state.local_channel,
                         "DL[oIIo?DLC]", DISPOSITION,
                         role, state->id, state->id, delivery->local.settled,
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
    int err = pn_flush_disp(transport, ssn);
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

int pn_process_tpwork_sender(pn_transport_t *transport, pn_delivery_t *delivery, bool *settle)
{
  *settle = false;
  pn_link_t *link = delivery->link;
  pn_session_state_t *ssn_state = &link->session->state;
  pn_link_state_t *link_state = &link->state;
  bool xfr_posted = false;
  if ((int16_t) ssn_state->local_channel >= 0 && (int32_t) link_state->local_handle >= 0) {
    pn_delivery_state_t *state = &delivery->state;
    if (!state->sent && (delivery->done || pn_buffer_size(delivery->bytes) > 0) &&
        ssn_state->remote_incoming_window > 0 && link_state->link_credit > 0) {
      if (!state->init) {
        state = pn_delivery_map_push(&ssn_state->outgoing, delivery);
      }

      pn_bytes_t bytes = pn_buffer_bytes(delivery->bytes);
      pn_set_payload(transport->disp, bytes.start, bytes.size);
      pn_bytes_t tag = pn_buffer_bytes(delivery->tag);
      int count = pn_post_transfer_frame(transport->disp,
                                         ssn_state->local_channel,
                                         link_state->local_handle,
                                         state->id, &tag,
                                         0, // message-format
                                         delivery->local.settled,
                                         !delivery->done,
                                         ssn_state->remote_incoming_window);
      if (count < 0) return count;
      xfr_posted = true;
      ssn_state->outgoing_transfer_count += count;
      ssn_state->remote_incoming_window -= count;

      int sent = bytes.size - transport->disp->output_size;
      pn_buffer_trim(delivery->bytes, sent, 0);
      link->session->outgoing_bytes -= sent;
      if (!pn_buffer_size(delivery->bytes) && delivery->done) {
        state->sent = true;
        link_state->delivery_count++;
        link_state->link_credit--;
        link->queued--;
        link->session->outgoing_deliveries--;
      }

      pn_collector_put(transport->connection->collector, PN_LINK_FLOW, link);
    }
  }

  pn_delivery_state_t *state = delivery->state.init ? &delivery->state : NULL;
  if ((int16_t) ssn_state->local_channel >= 0 && !delivery->remote.settled
      && state && state->sent && !xfr_posted) {
    int err = pn_post_disp(transport, delivery);
    if (err) return err;
  }

  *settle = delivery->local.settled && state && state->sent;
  return 0;
}

int pn_process_tpwork_receiver(pn_transport_t *transport, pn_delivery_t *delivery, bool *settle)
{
  *settle = false;
  pn_link_t *link = delivery->link;
  // XXX: need to prevent duplicate disposition sending
  pn_session_t *ssn = link->session;
  if ((int16_t) ssn->state.local_channel >= 0 && !delivery->remote.settled && delivery->state.init) {
    int err = pn_post_disp(transport, delivery);
    if (err) return err;
  }

  // XXX: need to centralize this policy and improve it
  if (!ssn->state.incoming_window) {
    int err = pn_post_flow(transport, ssn, link);
    if (err) return err;
  }

  *settle = delivery->local.settled;
  return 0;
}

int pn_process_tpwork(pn_transport_t *transport, pn_endpoint_t *endpoint)
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
        int err = pn_process_tpwork_sender(transport, delivery, &settle);
        if (err) return err;
      } else {
        dm = &link->session->state.incoming;
        int err = pn_process_tpwork_receiver(transport, delivery, &settle);
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

int pn_process_flush_disp(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION) {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = &session->state;
    if ((int16_t) state->local_channel >= 0 && !transport->close_sent)
    {
      int err = pn_flush_disp(transport, session);
      if (err) return err;
    }
  }

  return 0;
}

int pn_process_flow_sender(pn_transport_t *transport, pn_endpoint_t *endpoint)
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
        return pn_post_flow(transport, ssn, snd);
      }
    }
  }

  return 0;
}

int pn_process_link_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SENDER || endpoint->type == RECEIVER)
  {
    pn_link_t *link = (pn_link_t *) endpoint;
    pn_session_t *session = link->session;
    pn_session_state_t *ssn_state = &session->state;
    pn_link_state_t *state = &link->state;
    if (endpoint->state & PN_LOCAL_CLOSED && (int32_t) state->local_handle >= 0 &&
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

      int err = pn_post_frame(transport->disp, ssn_state->local_channel, "DL[Io?DL[sSC]]", DETACH,
                              state->local_handle, true, (bool) name, ERROR, name, description, info);
      if (err) return err;
      pn_hash_del(ssn_state->local_handles, state->local_handle);
      state->local_handle = -2;
    }

    pn_clear_modified(transport->connection, endpoint);
  }

  return 0;
}

bool pn_pointful_buffering(pn_transport_t *transport, pn_session_t *session)
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

int pn_process_ssn_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = &session->state;
    if (endpoint->state & PN_LOCAL_CLOSED && (int16_t) state->local_channel >= 0
        && !transport->close_sent)
    {
      if (pn_pointful_buffering(transport, session)) return 0;

      const char *name = NULL;
      const char *description = NULL;
      pn_data_t *info = NULL;

      if (pn_condition_is_set(&endpoint->condition)) {
        name = pn_condition_get_name(&endpoint->condition);
        description = pn_condition_get_description(&endpoint->condition);
        info = pn_condition_info(&endpoint->condition);
      }

      int err = pn_post_frame(transport->disp, state->local_channel, "DL[?DL[sSC]]", END,
                              (bool) name, ERROR, name, description, info);
      if (err) return err;
      pn_hash_del(transport->local_channels, state->local_channel);
      state->local_channel = -2;
    }

    pn_clear_modified(transport->connection, endpoint);
  }
  return 0;
}

int pn_process_conn_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (endpoint->state & PN_LOCAL_CLOSED && !transport->close_sent) {
      if (pn_pointful_buffering(transport, NULL)) return 0;
      int err = pn_post_close(transport, NULL, NULL);
      if (err) return err;
      transport->close_sent = true;
    }

    pn_clear_modified(transport->connection, endpoint);
  }
  return 0;
}

int pn_phase(pn_transport_t *transport, int (*phase)(pn_transport_t *, pn_endpoint_t *))
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

int pn_process(pn_transport_t *transport)
{
  int err;
  if ((err = pn_phase(transport, pn_process_conn_setup))) return err;
  if ((err = pn_phase(transport, pn_process_ssn_setup))) return err;
  if ((err = pn_phase(transport, pn_process_link_setup))) return err;
  if ((err = pn_phase(transport, pn_process_flow_receiver))) return err;

  // XXX: this has to happen two times because we might settle stuff
  // on the first pass and create space for more work to be done on the
  // second pass
  if ((err = pn_phase(transport, pn_process_tpwork))) return err;
  if ((err = pn_phase(transport, pn_process_tpwork))) return err;

  if ((err = pn_phase(transport, pn_process_flush_disp))) return err;

  if ((err = pn_phase(transport, pn_process_flow_sender))) return err;
  if ((err = pn_phase(transport, pn_process_link_teardown))) return err;
  if ((err = pn_phase(transport, pn_process_ssn_teardown))) return err;
  if ((err = pn_phase(transport, pn_process_conn_teardown))) return err;

  if (transport->connection->tpwork_head) {
    pn_modified(transport->connection, &transport->connection->endpoint, false);
  }

  return 0;
}

static ssize_t pn_output_write_header(pn_transport_t *transport,
                                      char *bytes, size_t size,
                                      const char *header, size_t hdrsize,
                                      const char *protocol,
                                      ssize_t (*next)(pn_io_layer_t *, char *, size_t))
{
  if (transport->disp->trace & PN_TRACE_FRM)
    pn_transport_logf(transport, "  -> %s", protocol);
  assert(size >= hdrsize);
  memmove(bytes, header, hdrsize);
  transport->io_layers[PN_IO_AMQP].process_output = next;
  return hdrsize;
}

static ssize_t pn_output_write_amqp_header(pn_io_layer_t *io_layer, char *bytes, size_t size)
{
  pn_transport_t *transport = (pn_transport_t *)io_layer->context;
  return pn_output_write_header(transport, bytes, size, AMQP_HEADER, 8, "AMQP",
                                pn_output_write_amqp);
}

static ssize_t pn_output_write_amqp(pn_io_layer_t *io_layer, char *bytes, size_t size)
{
  pn_transport_t *transport = (pn_transport_t *)io_layer->context;
  if (transport->connection && !transport->done_processing) {
    int err = pn_process(transport);
    if (err) {
      pn_transport_logf(transport, "process error %i", err);
      transport->done_processing = true;
    }
  }

  // write out any buffered data _before_ returning PN_EOS, else we
  // could truncate an outgoing Close frame containing a useful error
  // status
  if (!transport->disp->available && transport->close_sent) {
    return PN_EOS;
  }

  return pn_dispatcher_output(transport->disp, bytes, size);
}

// generate outbound data, return amount of pending output else error
static ssize_t transport_produce(pn_transport_t *transport)
{
  pn_io_layer_t *io_layer = transport->io_layers;
  ssize_t space = transport->output_size - transport->output_pending;

  if (space == 0) {     // can we expand the buffer?
    int more = 0;
    if (!transport->remote_max_frame)   // no limit, so double it
      more = transport->output_size;
    else if (transport->remote_max_frame > transport->output_size)
      more = transport->remote_max_frame - transport->output_size;
    if (more) {
      char *newbuf = (char *)malloc( transport->output_size + more );
      if (newbuf) {
        memmove( newbuf, transport->output_buf, transport->output_pending );
        free( transport->output_buf );
        transport->output_buf = newbuf;
        transport->output_size += more;
        space = more;
      }
    }
  }

  while (space > 0) {
    ssize_t n;
    n = io_layer->process_output( io_layer,
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
      if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM)) {
        if (n < 0) {
          pn_transport_log(transport, "  -> EOS");
        }
        /*else
          pn_transport_logf(transport, "  -> EOS (%" PN_ZI ") %s", n,
          pn_error_text(transport->error));*/
      }
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
  if (transport->sasl) pn_sasl_trace(transport->sasl, trace);
  if (transport->ssl) pn_ssl_trace(transport->ssl, trace);
  transport->disp->trace = trace;
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
  transport->context = context;
}

void *pn_transport_get_context(pn_transport_t *transport)
{
  assert(transport);
  return transport->context;
}

void pn_transport_log(pn_transport_t *transport, const char *message)
{
  assert(transport);
  transport->tracer(transport, message);
}

void pn_transport_logf(pn_transport_t *transport, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  pn_string_vformat(transport->scratch, fmt, ap);
  va_end(ap);

  pn_transport_log(transport, pn_string_get(transport->scratch));
}

uint16_t pn_transport_get_channel_max(pn_transport_t *transport)
{
  return transport->channel_max;
}

void pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max)
{
  transport->channel_max = channel_max;
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
  transport->io_layers[PN_IO_AMQP].process_tick = pn_tick_amqp;
}

pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport)
{
  return transport->remote_idle_timeout;
}

pn_timestamp_t pn_transport_tick(pn_transport_t *transport, pn_timestamp_t now)
{
  pn_io_layer_t *io_layer = transport->io_layers;
  return io_layer->process_tick( io_layer, now );
}

uint64_t pn_transport_get_frames_output(const pn_transport_t *transport)
{
  if (transport && transport->disp)
    return transport->disp->output_frames_ct;
  return 0;
}

uint64_t pn_transport_get_frames_input(const pn_transport_t *transport)
{
  if (transport && transport->disp)
    return transport->disp->input_frames_ct;
  return 0;
}

/** Pass through input handler */
ssize_t pn_io_layer_input_passthru(pn_io_layer_t *io_layer, const char *data, size_t available)
{
  pn_io_layer_t *next = io_layer->next;
  if (next)
    return next->process_input( next, data, available );
  return PN_EOS;
}

/** Pass through output handler */
ssize_t pn_io_layer_output_passthru(pn_io_layer_t *io_layer, char *bytes, size_t size)
{
  pn_io_layer_t *next = io_layer->next;
  if (next)
    return next->process_output( next, bytes, size );
  return PN_EOS;
}

/** Pass through tick handler */
pn_timestamp_t pn_io_layer_tick_passthru(pn_io_layer_t *io_layer, pn_timestamp_t now)
{
  pn_io_layer_t *next = io_layer->next;
  if (next)
    return next->process_tick( next, now );
  return 0;
}


///

// input
ssize_t pn_transport_capacity(pn_transport_t *transport)  /* <0 == done */
{
  if (transport->tail_closed) return PN_EOS;
  //if (pn_error_code(transport->error)) return pn_error_code(transport->error);

  ssize_t capacity = transport->input_size - transport->input_pending;
  if (!capacity) {
    // can we expand the size of the input buffer?
    int more = 0;
    if (!transport->local_max_frame) {  // no limit (ha!)
      more = transport->input_size;
    } else if (transport->local_max_frame > transport->input_size) {
      more = transport->local_max_frame - transport->input_size;
    }
    if (more) {
      char *newbuf = (char *) malloc( transport->input_size + more );
      if (newbuf) {
        memmove( newbuf, transport->input_buf, transport->input_pending );
        free( transport->input_buf );
        transport->input_buf = newbuf;
        transport->input_size += more;
        capacity = more;
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
    transport->tail_closed = true;
  }

  if (n < 0 && n != PN_EOS) return n;
  return 0;
}

// input stream has closed
int pn_transport_close_tail(pn_transport_t *transport)
{
  transport->tail_closed = true;
  transport_consume( transport );
  return 0;
  // XXX: what if not all input processed at this point?  do we care???
}

// output
ssize_t pn_transport_pending(pn_transport_t *transport)      /* <0 == done */
{
  assert(transport);
  if (transport->head_closed) return PN_EOS;
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
  if (transport && size) {
    assert( transport->output_pending >= size );
    transport->output_pending -= size;
    transport->bytes_output += size;
    if (transport->output_pending) {
      memmove( transport->output_buf,  &transport->output_buf[size],
               transport->output_pending );
    }
  }
}

int pn_transport_close_head(pn_transport_t *transport)
{
  transport->head_closed = true;
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
  pn_io_layer_t *io_layer = transport->io_layers;
  while (io_layer != &transport->io_layers[PN_IO_LAYER_CT]) {
    if (io_layer->buffered_output && io_layer->buffered_output( io_layer ))
      return false;
    ++io_layer;
  }
  return true;
}

bool pn_transport_closed(pn_transport_t *transport)
{
  assert(transport);
  ssize_t capacity = pn_transport_capacity(transport);
  ssize_t pending = pn_transport_pending(transport);
  return capacity < 0 && pending < 0;
}
