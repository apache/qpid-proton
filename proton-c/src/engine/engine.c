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
#include <stdlib.h>
#include <string.h>
#include <proton/framing.h>
#include "protocol.h"
#include <inttypes.h>

#include <stdarg.h>
#include <stdio.h>

#include "../sasl/sasl-internal.h"
#include "../ssl/ssl-internal.h"

// delivery buffers

void pn_delivery_buffer_init(pn_delivery_buffer_t *db, pn_sequence_t next, size_t capacity)
{
  // XXX: error handling
  db->deliveries = malloc(sizeof(pn_delivery_state_t) * capacity);
  db->next = next;
  db->capacity = capacity;
  db->head = 0;
  db->size = 0;
}

void pn_delivery_buffer_free(pn_delivery_buffer_t *db)
{
  free(db->deliveries);
}

size_t pn_delivery_buffer_size(pn_delivery_buffer_t *db)
{
  return db->size;
}

size_t pn_delivery_buffer_available(pn_delivery_buffer_t *db)
{
  return db->capacity - db->size;
}

bool pn_delivery_buffer_empty(pn_delivery_buffer_t *db)
{
  return db->size == 0;
}

pn_delivery_state_t *pn_delivery_buffer_get(pn_delivery_buffer_t *db, size_t index)
{
  if (index < db->size) return db->deliveries + ((db->head + index) % db->capacity);
  else return NULL;
}

pn_delivery_state_t *pn_delivery_buffer_head(pn_delivery_buffer_t *db)
{
  if (db->size) return db->deliveries + db->head;
  else return NULL;
}

pn_delivery_state_t *pn_delivery_buffer_tail(pn_delivery_buffer_t *db)
{
  if (db->size) return pn_delivery_buffer_get(db, db->size - 1);
  else return NULL;
}

pn_sequence_t pn_delivery_buffer_lwm(pn_delivery_buffer_t *db)
{
  if (db->size) return pn_delivery_buffer_head(db)->id;
  else return db->next;
}

static void pn_delivery_state_init(pn_delivery_state_t *ds, pn_delivery_t *delivery, pn_sequence_t id)
{
  ds->delivery = delivery;
  ds->id = id;
  ds->sent = false;
}

pn_delivery_state_t *pn_delivery_buffer_push(pn_delivery_buffer_t *db, pn_delivery_t *delivery)
{
  if (!pn_delivery_buffer_available(db))
    return NULL;
  db->size++;
  pn_delivery_state_t *ds = pn_delivery_buffer_tail(db);
  pn_delivery_state_init(ds, delivery, db->next++);
  return ds;
}

bool pn_delivery_buffer_pop(pn_delivery_buffer_t *db)
{
  if (db->size) {
    db->head = (db->head + 1) % db->capacity;
    db->size--;
    return true;
  } else {
    return false;
  }
}

void pn_delivery_buffer_gc(pn_delivery_buffer_t *db)
{
  while (db->size && !pn_delivery_buffer_head(db)->delivery) {
    pn_delivery_buffer_pop(db);
  }
}

// endpoints

pn_connection_t *pn_ep_get_connection(pn_endpoint_t *endpoint)
{
  switch (endpoint->type) {
  case CONNECTION:
    return (pn_connection_t *) endpoint;
  case SESSION:
    return ((pn_session_t *) endpoint)->connection;
  case SENDER:
  case RECEIVER:
    return ((pn_link_t *) endpoint)->session->connection;
  case TRANSPORT:
    return ((pn_transport_t *) endpoint)->connection;
  }

  return NULL;
}

void pn_modified(pn_connection_t *connection, pn_endpoint_t *endpoint);

void pn_open(pn_endpoint_t *endpoint)
{
  // TODO: do we care about the current state?
  PN_SET_LOCAL(endpoint->state, PN_LOCAL_ACTIVE);
  pn_modified(pn_ep_get_connection(endpoint), endpoint);
}

void pn_close(pn_endpoint_t *endpoint)
{
  // TODO: do we care about the current state?
  PN_SET_LOCAL(endpoint->state, PN_LOCAL_CLOSED);
  pn_modified(pn_ep_get_connection(endpoint), endpoint);
}

void pn_free(pn_endpoint_t *endpoint)
{
  switch (endpoint->type)
  {
  case CONNECTION:
    pn_connection_free((pn_connection_t *)endpoint);
    break;
  case TRANSPORT:
    pn_transport_free((pn_transport_t *)endpoint);
    break;
  case SESSION:
    pn_session_free((pn_session_t *)endpoint);
    break;
  case SENDER:
    pn_link_free((pn_link_t *)endpoint);
    break;
  case RECEIVER:
    pn_link_free((pn_link_t *)endpoint);
    break;
  }
}

void pn_connection_open(pn_connection_t *connection)
{
  if (connection) pn_open((pn_endpoint_t *) connection);
}

void pn_connection_close(pn_connection_t *connection)
{
  if (connection) pn_close((pn_endpoint_t *) connection);
}

void pn_endpoint_tini(pn_endpoint_t *endpoint);

void pn_connection_free(pn_connection_t *connection)
{
  if (!connection) return;

  while (connection->session_count)
    pn_session_free(connection->sessions[connection->session_count - 1]);
  free(connection->sessions);
  free(connection->container);
  free(connection->hostname);
  pn_data_free(connection->offered_capabilities);
  pn_data_free(connection->desired_capabilities);
  pn_endpoint_tini(&connection->endpoint);
  free(connection);
}

void *pn_connection_get_context(pn_connection_t *conn)
{
    return conn ? conn->context : 0;
}

void pn_connection_set_context(pn_connection_t *conn, void *context)
{
    if (conn)
        conn->context = context;
}

void pn_transport_open(pn_transport_t *transport)
{
  pn_open((pn_endpoint_t *) transport);
}

void pn_transport_close(pn_transport_t *transport)
{
  pn_close((pn_endpoint_t *) transport);
}

void pn_transport_free(pn_transport_t *transport)
{
  if (!transport) return;

  pn_ssl_free(transport->ssl);
  pn_sasl_free(transport->sasl);
  pn_dispatcher_free(transport->disp);
  for (int i = 0; i < transport->session_capacity; i++) {
    pn_delivery_buffer_free(&transport->sessions[i].incoming);
    pn_delivery_buffer_free(&transport->sessions[i].outgoing);
    free(transport->sessions[i].links);
    free(transport->sessions[i].handles);
  }
  free(transport->remote_container);
  free(transport->remote_hostname);
  pn_data_free(transport->remote_offered_capabilities);
  pn_data_free(transport->remote_desired_capabilities);
  pn_error_free(transport->error);
  free(transport->sessions);
  free(transport->channels);
  free(transport);
}

void pn_add_session(pn_connection_t *conn, pn_session_t *ssn)
{
  PN_ENSURE(conn->sessions, conn->session_capacity, conn->session_count + 1);
  conn->sessions[conn->session_count++] = ssn;
  ssn->connection = conn;
  ssn->id = conn->session_count;
}

void pn_remove_session(pn_connection_t *conn, pn_session_t *ssn)
{
  for (int i = 0; i < conn->session_count; i++)
  {
    if (conn->sessions[i] == ssn)
    {
      memmove(&conn->sessions[i], &conn->sessions[i+1], conn->session_count - i - 1);
      conn->session_count--;
      break;
    }
  }
  ssn->connection = NULL;
}

pn_connection_t *pn_session_connection(pn_session_t *session)
{
  return session ? session->connection : NULL;
}

void pn_session_open(pn_session_t *session)
{
  if (session) pn_open((pn_endpoint_t *) session);
}

void pn_session_close(pn_session_t *session)
{
  if (session) pn_close((pn_endpoint_t *) session);
}

void pn_session_free(pn_session_t *session)
{
  if (!session) return;

  while (session->link_count)
    pn_link_free(session->links[session->link_count - 1]);
  if (session->connection)
    pn_remove_session(session->connection, session);
  free(session->links);
  pn_endpoint_tini(&session->endpoint);
  free(session);
}

void *pn_session_get_context(pn_session_t *session)
{
    return session ? session->context : 0;
}

void pn_session_set_context(pn_session_t *session, void *context)
{
    if (session)
        session->context = context;
}


void pn_add_link(pn_session_t *ssn, pn_link_t *link)
{
  PN_ENSURE(ssn->links, ssn->link_capacity, ssn->link_count + 1);
  ssn->links[ssn->link_count++] = link;
  link->session = ssn;
  link->id = ssn->link_count;
}

void pn_remove_link(pn_session_t *ssn, pn_link_t *link)
{
  for (int i = 0; i < ssn->link_count; i++)
  {
    if (ssn->links[i] == link)
    {
      memmove(&ssn->links[i], &ssn->links[i+1], ssn->link_count - i - 1);
      ssn->link_count--;
      break;
    }
  }
  link->session = NULL;
}

void pn_free_delivery(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_buffer_free(delivery->tag);
    pn_buffer_free(delivery->bytes);
    free(delivery);
  }
}

void pn_link_open(pn_link_t *link)
{
  if (link) pn_open((pn_endpoint_t *) link);
}

void pn_link_close(pn_link_t *link)
{
  if (link) pn_close((pn_endpoint_t *) link);
}

void pn_terminus_free(pn_terminus_t *terminus)
{
  free(terminus->address);
  pn_data_free(terminus->properties);
  pn_data_free(terminus->capabilities);
  pn_data_free(terminus->outcomes);
  pn_data_free(terminus->filter);
}

void pn_link_free(pn_link_t *link)
{
  if (!link) return;

  pn_terminus_free(&link->source);
  pn_terminus_free(&link->target);
  pn_terminus_free(&link->remote_source);
  pn_terminus_free(&link->remote_target);
  pn_remove_link(link->session, link);
  while (link->settled_head) {
    pn_delivery_t *d = link->settled_head;
    LL_POP(link, settled);
    pn_free_delivery(d);
  }
  while (link->unsettled_head) {
    pn_delivery_t *d = link->unsettled_head;
    LL_POP(link, unsettled);
    pn_free_delivery(d);
  }
  free(link->name);
  pn_endpoint_tini(&link->endpoint);
  free(link);
}

void *pn_link_get_context(pn_link_t *link)
{
    return link ? link->context : 0;
}

void pn_link_set_context(pn_link_t *link, void *context)
{
    if (link)
        link->context = context;
}

void pn_endpoint_init(pn_endpoint_t *endpoint, int type, pn_connection_t *conn)
{
  endpoint->type = type;
  endpoint->state = PN_LOCAL_UNINIT | PN_REMOTE_UNINIT;
  endpoint->error = pn_error();
  endpoint->endpoint_next = NULL;
  endpoint->endpoint_prev = NULL;
  endpoint->transport_next = NULL;
  endpoint->transport_prev = NULL;
  endpoint->modified = false;

  LL_ADD(conn, endpoint, endpoint);
}

void pn_endpoint_tini(pn_endpoint_t *endpoint)
{
  pn_error_free(endpoint->error);
}

pn_connection_t *pn_connection()
{
  pn_connection_t *conn = (pn_connection_t *) malloc(sizeof(pn_connection_t));
  if (!conn) return NULL;

  conn->context = NULL;
  conn->endpoint_head = NULL;
  conn->endpoint_tail = NULL;
  pn_endpoint_init(&conn->endpoint, CONNECTION, conn);
  conn->transport_head = NULL;
  conn->transport_tail = NULL;
  conn->sessions = NULL;
  conn->session_capacity = 0;
  conn->session_count = 0;
  conn->transport = NULL;
  conn->work_head = NULL;
  conn->work_tail = NULL;
  conn->tpwork_head = NULL;
  conn->tpwork_tail = NULL;
  conn->container = NULL;
  conn->hostname = NULL;
  conn->offered_capabilities = pn_data(16);
  conn->desired_capabilities = pn_data(16);

  return conn;
}

pn_state_t pn_connection_state(pn_connection_t *connection)
{
  return connection ? connection->endpoint.state : 0;
}

pn_error_t *pn_connection_error(pn_connection_t *connection)
{
  return connection ? connection->endpoint.error : NULL;
}

const char *pn_connection_get_container(pn_connection_t *connection)
{
  return connection ? connection->container : NULL;
}

void pn_connection_set_container(pn_connection_t *connection, const char *container)
{
  if (!connection) return;
  if (connection->container) free(connection->container);
  connection->container = pn_strdup(container);
}

const char *pn_connection_get_hostname(pn_connection_t *connection)
{
  return connection ? connection->hostname : NULL;
}

void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname)
{
  if (!connection) return;
  if (connection->hostname) free(connection->hostname);
  connection->hostname = pn_strdup(hostname);
}

pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection)
{
  return connection->offered_capabilities;
}

pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection)
{
  return connection->desired_capabilities;
}

pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection)
{
  if (!connection) return NULL;
  return connection->transport ? connection->transport->remote_offered_capabilities : NULL;
}

pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection)
{
  if (!connection) return NULL;
  return connection->transport ? connection->transport->remote_desired_capabilities : NULL;
}

const char *pn_connection_remote_container(pn_connection_t *connection)
{
  if (!connection) return NULL;
  return connection->transport ? connection->transport->remote_container : NULL;
}

const char *pn_connection_remote_hostname(pn_connection_t *connection)
{
  if (!connection) return NULL;
  return connection->transport ? connection->transport->remote_hostname : NULL;
}

pn_delivery_t *pn_work_head(pn_connection_t *connection)
{
  if (!connection) return NULL;
  return connection->work_head;
}

pn_delivery_t *pn_work_next(pn_delivery_t *delivery)
{
  if (!delivery) return NULL;

  if (delivery->work)
    return delivery->work_next;
  else
    return pn_work_head(delivery->link->session->connection);
}

void pn_add_work(pn_connection_t *connection, pn_delivery_t *delivery)
{
  if (!delivery->work)
  {
    LL_ADD(connection, work, delivery);
    delivery->work = true;
  }
}

void pn_clear_work(pn_connection_t *connection, pn_delivery_t *delivery)
{
  if (delivery->work)
  {
    LL_REMOVE(connection, work, delivery);
    delivery->work = false;
  }
}

void pn_work_update(pn_connection_t *connection, pn_delivery_t *delivery)
{
  pn_link_t *link = pn_delivery_link(delivery);
  pn_delivery_t *current = pn_link_current(link);
  if (delivery->updated && !delivery->local_settled) {
    pn_add_work(connection, delivery);
  } else if (delivery == current) {
    if (link->endpoint.type == SENDER) {
      if (pn_link_credit(link) > 0) {
        pn_add_work(connection, delivery);
      } else {
        pn_clear_work(connection, delivery);
      }
    } else {
      pn_add_work(connection, delivery);
    }
  } else {
    pn_clear_work(connection, delivery);
  }
}

void pn_add_tpwork(pn_delivery_t *delivery)
{
  pn_connection_t *connection = delivery->link->session->connection;
  if (!delivery->tpwork)
  {
    LL_ADD(connection, tpwork, delivery);
    delivery->tpwork = true;
  }
  pn_modified(connection, &connection->endpoint);
}

void pn_clear_tpwork(pn_delivery_t *delivery)
{
  pn_connection_t *connection = delivery->link->session->connection;
  if (delivery->tpwork)
  {
    LL_REMOVE(connection, tpwork, delivery);
    delivery->tpwork = false;
  }
}

void pn_dump(pn_connection_t *conn)
{
  pn_endpoint_t *endpoint = conn->transport_head;
  while (endpoint)
  {
    printf("%p", (void *) endpoint);
    endpoint = endpoint->transport_next;
    if (endpoint)
      printf(" -> ");
  }
  printf("\n");
}

void pn_modified(pn_connection_t *connection, pn_endpoint_t *endpoint)
{
  if (!endpoint->modified) {
    LL_ADD(connection, transport, endpoint);
    endpoint->modified = true;
  }
}

void pn_clear_modified(pn_connection_t *connection, pn_endpoint_t *endpoint)
{
  if (endpoint->modified) {
    LL_REMOVE(connection, transport, endpoint);
    endpoint->transport_next = NULL;
    endpoint->transport_prev = NULL;
    endpoint->modified = false;
  }
}

bool pn_matches(pn_endpoint_t *endpoint, pn_endpoint_type_t type, pn_state_t state)
{
  if (endpoint->type != type) return false;

  if (!state) return true;

  int st = endpoint->state;
  if ((state & PN_REMOTE_MASK) == 0 || (state & PN_LOCAL_MASK) == 0)
    return st & state;
  else
    return st == state;
}

pn_endpoint_t *pn_find(pn_endpoint_t *endpoint, pn_endpoint_type_t type, pn_state_t state)
{
  while (endpoint)
  {
    if (pn_matches(endpoint, type, state))
      return endpoint;
    endpoint = endpoint->endpoint_next;
  }
  return NULL;
}

pn_session_t *pn_session_head(pn_connection_t *conn, pn_state_t state)
{
  if (conn)
    return (pn_session_t *) pn_find(conn->endpoint_head, SESSION, state);
  else
    return NULL;
}

pn_session_t *pn_session_next(pn_session_t *ssn, pn_state_t state)
{
  if (ssn)
    return (pn_session_t *) pn_find(ssn->endpoint.endpoint_next, SESSION, state);
  else
    return NULL;
}

pn_link_t *pn_link_head(pn_connection_t *conn, pn_state_t state)
{
  if (!conn) return NULL;

  pn_endpoint_t *endpoint = conn->endpoint_head;

  while (endpoint)
  {
    if (pn_matches(endpoint, SENDER, state) || pn_matches(endpoint, RECEIVER, state))
      return (pn_link_t *) endpoint;
    endpoint = endpoint->endpoint_next;
  }

  return NULL;
}

pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state)
{
  if (!link) return NULL;

  pn_endpoint_t *endpoint = link->endpoint.endpoint_next;

  while (endpoint)
  {
    if (pn_matches(endpoint, SENDER, state) || pn_matches(endpoint, RECEIVER, state))
      return (pn_link_t *) endpoint;
    endpoint = endpoint->endpoint_next;
  }

  return NULL;
}

pn_session_t *pn_session(pn_connection_t *conn)
{
  if (!conn) return NULL;
  pn_session_t *ssn = (pn_session_t *) malloc(sizeof(pn_session_t));
  if (!ssn) return NULL;

  pn_endpoint_init(&ssn->endpoint, SESSION, conn);
  pn_add_session(conn, ssn);
  ssn->links = NULL;
  ssn->link_capacity = 0;
  ssn->link_count = 0;
  ssn->context = 0;

  return ssn;
}

pn_state_t pn_session_state(pn_session_t *session)
{
  return session->endpoint.state;
}

pn_error_t *pn_session_error(pn_session_t *session)
{
  return session->endpoint.error;
}

int pn_do_open(pn_dispatcher_t *disp);
int pn_do_begin(pn_dispatcher_t *disp);
int pn_do_attach(pn_dispatcher_t *disp);
int pn_do_transfer(pn_dispatcher_t *disp);
int pn_do_flow(pn_dispatcher_t *disp);
int pn_do_disposition(pn_dispatcher_t *disp);
int pn_do_detach(pn_dispatcher_t *disp);
int pn_do_end(pn_dispatcher_t *disp);
int pn_do_close(pn_dispatcher_t *disp);

static ssize_t pn_input_read_sasl_header(pn_transport_t *transport, const char *bytes, size_t available);
static ssize_t pn_input_read_sasl(pn_transport_t *transport, const char *bytes, size_t available);
static ssize_t pn_input_read_amqp_header(pn_transport_t *transport, const char *bytes, size_t available);
static ssize_t pn_input_read_amqp(pn_transport_t *transport, const char *bytes, size_t available);
static ssize_t pn_output_write_sasl_header(pn_transport_t *transport, char *bytes, size_t available);
static ssize_t pn_output_write_sasl(pn_transport_t *transport, char *bytes, size_t available);
static ssize_t pn_output_write_amqp_header(pn_transport_t *transport, char *bytes, size_t available);
static ssize_t pn_output_write_amqp(pn_transport_t *transport, char *bytes, size_t available);
static pn_timestamp_t pn_process_tick(pn_transport_t *transport, pn_timestamp_t now);

void pn_transport_init(pn_transport_t *transport)
{
  transport->process_input = pn_input_read_amqp_header;
  transport->process_output = pn_output_write_amqp_header;
  transport->process_tick = NULL;
  transport->header_count = 0;
  transport->sasl = NULL;
  transport->ssl = NULL;
  transport->disp = pn_dispatcher(0, transport);

  pn_dispatcher_action(transport->disp, OPEN, "OPEN", pn_do_open);
  pn_dispatcher_action(transport->disp, BEGIN, "BEGIN", pn_do_begin);
  pn_dispatcher_action(transport->disp, ATTACH, "ATTACH", pn_do_attach);
  pn_dispatcher_action(transport->disp, TRANSFER, "TRANSFER", pn_do_transfer);
  pn_dispatcher_action(transport->disp, FLOW, "FLOW", pn_do_flow);
  pn_dispatcher_action(transport->disp, DISPOSITION, "DISPOSITION", pn_do_disposition);
  pn_dispatcher_action(transport->disp, DETACH, "DETACH", pn_do_detach);
  pn_dispatcher_action(transport->disp, END, "END", pn_do_end);
  pn_dispatcher_action(transport->disp, CLOSE, "CLOSE", pn_do_close);

  transport->open_sent = false;
  transport->open_rcvd = false;
  transport->close_sent = false;
  transport->close_rcvd = false;
  transport->remote_container = NULL;
  transport->remote_hostname = NULL;
  transport->local_max_frame = 0;
  transport->remote_max_frame = 0;
  transport->local_idle_timeout = 0;
  transport->dead_remote_deadline = 0;
  transport->last_bytes_input = 0;
  transport->remote_idle_timeout = 0;
  transport->keepalive_deadline = 0;
  transport->last_bytes_output = 0;
  transport->remote_offered_capabilities = pn_data(16);
  transport->remote_desired_capabilities = pn_data(16);
  transport->error = pn_error();

  transport->sessions = NULL;
  transport->session_capacity = 0;

  transport->channels = NULL;
  transport->channel_capacity = 0;

  transport->condition = NULL;

  transport->bytes_input = 0;
  transport->bytes_output = 0;
}

pn_session_state_t *pn_session_get_state(pn_transport_t *transport, pn_session_t *ssn)
{
  int old_capacity = transport->session_capacity;
  PN_ENSURE(transport->sessions, transport->session_capacity, ssn->id + 1);
  for (int i = old_capacity; i < transport->session_capacity; i++)
  {
    transport->sessions[i] = (pn_session_state_t) {.session=NULL,
                                                   .local_channel=-1,
                                                   .remote_channel=-1};
    pn_delivery_buffer_init(&transport->sessions[i].incoming, 0, PN_SESSION_WINDOW);
    pn_delivery_buffer_init(&transport->sessions[i].outgoing, 0, PN_SESSION_WINDOW);
  }
  pn_session_state_t *state = &transport->sessions[ssn->id];
  state->session = ssn;
  return state;
}

pn_session_state_t *pn_channel_state(pn_transport_t *transport, uint16_t channel)
{
  PN_ENSUREZ(transport->channels, transport->channel_capacity, channel + 1);
  return transport->channels[channel];
}

void pn_map_channel(pn_transport_t *transport, uint16_t channel, pn_session_state_t *state)
{
  PN_ENSUREZ(transport->channels, transport->channel_capacity, channel + 1);
  state->remote_channel = channel;
  transport->channels[channel] = state;
}

pn_transport_t *pn_transport()
{
  pn_transport_t *transport = (pn_transport_t *) malloc(sizeof(pn_transport_t));
  if (!transport) return NULL;

  transport->connection = NULL;
  pn_transport_init(transport);
  return transport;
}

void pn_transport_sasl_init(pn_transport_t *transport)
{
  transport->process_input = pn_input_read_sasl_header;
  transport->process_output = pn_output_write_sasl_header;
}

int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection)
{
  if (!transport) return PN_ARG_ERR;
  if (transport->connection) return PN_STATE_ERR;
  if (connection->transport) return PN_STATE_ERR;
  transport->connection = connection;
  connection->transport = transport;
  if (transport->open_rcvd) {
    PN_SET_REMOTE(connection->endpoint.state, PN_REMOTE_ACTIVE);
    if (!pn_error_code(transport->error)) {
      transport->disp->halt = false;
    }
  }
  return 0;
}

pn_error_t *pn_transport_error(pn_transport_t *transport)
{
  return transport->error;
}

void pn_terminus_init(pn_terminus_t *terminus, pn_terminus_type_t type)
{
  terminus->type = type;
  terminus->address = NULL;
  terminus->durability = PN_NONDURABLE;
  terminus->expiry_policy = PN_SESSION_CLOSE;
  terminus->timeout = 0;
  terminus->dynamic = false;
  terminus->properties = pn_data(16);
  terminus->capabilities = pn_data(16);
  terminus->outcomes = pn_data(16);
  terminus->filter = pn_data(16);
}

void pn_link_init(pn_link_t *link, int type, pn_session_t *session, const char *name)
{
  pn_endpoint_init(&link->endpoint, type, session->connection);
  pn_add_link(session, link);
  link->name = pn_strdup(name);
  pn_terminus_init(&link->source, PN_SOURCE);
  pn_terminus_init(&link->target, PN_TARGET);
  pn_terminus_init(&link->remote_source, PN_UNSPECIFIED);
  pn_terminus_init(&link->remote_target, PN_UNSPECIFIED);
  link->settled_head = link->settled_tail = NULL;
  link->unsettled_head = link->unsettled_tail = link->current = NULL;
  link->unsettled_count = 0;
  link->available = 0;
  link->credit = 0;
  link->queued = 0;
  link->drain = false;
  link->drained = false;
  link->context = 0;
}

pn_terminus_t *pn_link_source(pn_link_t *link)
{
  return link ? &link->source : NULL;
}

pn_terminus_t *pn_link_target(pn_link_t *link)
{
  return link ? &link->target : NULL;
}

pn_terminus_t *pn_link_remote_source(pn_link_t *link)
{
  return link ? &link->remote_source : NULL;
}

pn_terminus_t *pn_link_remote_target(pn_link_t *link)
{
  return link ? &link->remote_target : NULL;
}

int pn_terminus_set_type(pn_terminus_t *terminus, pn_terminus_type_t type)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->type = type;
  return 0;
}

pn_terminus_type_t pn_terminus_get_type(pn_terminus_t *terminus)
{
  return terminus ? terminus->type : (pn_terminus_type_t) 0;
}

const char *pn_terminus_get_address(pn_terminus_t *terminus)
{
  return terminus ? terminus->address : NULL;
}

int pn_terminus_set_address(pn_terminus_t *terminus, const char *address)
{
  if (!terminus) return PN_ARG_ERR;
  if (terminus->address) free(terminus->address);
  terminus->address = pn_strdup(address);
  return 0;
}

char *pn_bytes_strdup(pn_bytes_t str)
{
  return pn_strndup(str.start, str.size);
}

int pn_terminus_set_address_bytes(pn_terminus_t *terminus, pn_bytes_t address)
{
  if (!terminus) return PN_ARG_ERR;
  if (terminus->address) free(terminus->address);
  terminus->address = pn_bytes_strdup(address);
  return 0;
}

pn_durability_t pn_terminus_get_durability(pn_terminus_t *terminus)
{
  return terminus ? terminus->durability : (pn_durability_t) 0;
}

int pn_terminus_set_durability(pn_terminus_t *terminus, pn_durability_t durability)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->durability = durability;
  return 0;
}

pn_expiry_policy_t pn_terminus_get_expiry_policy(pn_terminus_t *terminus)
{
  return terminus ? terminus->expiry_policy : (pn_expiry_policy_t) 0;
}

int pn_terminus_set_expiry_policy(pn_terminus_t *terminus, pn_expiry_policy_t expiry_policy)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->expiry_policy = expiry_policy;
  return 0;
}

pn_seconds_t pn_terminus_get_timeout(pn_terminus_t *terminus)
{
  return terminus ? terminus->timeout : 0;
}

int pn_terminus_set_timeout(pn_terminus_t *terminus, pn_seconds_t timeout)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->timeout = timeout;
  return 0;
}

bool pn_terminus_is_dynamic(pn_terminus_t *terminus)
{
  return terminus ? terminus->dynamic : false;
}

int pn_terminus_set_dynamic(pn_terminus_t *terminus, bool dynamic)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->dynamic = dynamic;
  return 0;
}

pn_data_t *pn_terminus_properties(pn_terminus_t *terminus)
{
  return terminus ? terminus->properties : NULL;
}

pn_data_t *pn_terminus_capabilities(pn_terminus_t *terminus)
{
  return terminus ? terminus->capabilities : NULL;
}

pn_data_t *pn_terminus_outcomes(pn_terminus_t *terminus)
{
  return terminus ? terminus->outcomes : NULL;
}

pn_data_t *pn_terminus_filter(pn_terminus_t *terminus)
{
  return terminus ? terminus->filter : NULL;
}

int pn_terminus_copy(pn_terminus_t *terminus, pn_terminus_t *src)
{
  if (!terminus || !src) {
    return PN_ARG_ERR;
  }

  terminus->type = src->type;
  int err = pn_terminus_set_address(terminus, pn_terminus_get_address(src));
  if (err) return err;
  terminus->durability = src->durability;
  terminus->expiry_policy = src->expiry_policy;
  terminus->timeout = src->timeout;
  terminus->dynamic = src->dynamic;
  err = pn_data_copy(terminus->properties, src->properties);
  if (err) return err;
  err = pn_data_copy(terminus->capabilities, src->capabilities);
  if (err) return err;
  err = pn_data_copy(terminus->outcomes, src->outcomes);
  if (err) return err;
  err = pn_data_copy(terminus->filter, src->filter);
  if (err) return err;
  return 0;
}

pn_link_state_t *pn_link_get_state(pn_session_state_t *ssn_state, pn_link_t *link)
{
  int old_capacity = ssn_state->link_capacity;
  PN_ENSURE(ssn_state->links, ssn_state->link_capacity, link->id + 1);
  for (int i = old_capacity; i < ssn_state->link_capacity; i++)
  {
    ssn_state->links[i] = (pn_link_state_t) {.link=NULL, .local_handle = -1,
                                             .remote_handle=-1};
  }
  pn_link_state_t *state = &ssn_state->links[link->id];
  state->link = link;
  return state;
}

void pn_map_handle(pn_session_state_t *ssn_state, uint32_t handle, pn_link_state_t *state)
{
  PN_ENSUREZ(ssn_state->handles, ssn_state->handle_capacity, handle + 1);
  state->remote_handle = handle;
  ssn_state->handles[handle] = state;
}

pn_link_state_t *pn_handle_state(pn_session_state_t *ssn_state, uint32_t handle)
{
  PN_ENSUREZ(ssn_state->handles, ssn_state->handle_capacity, handle + 1);
  return ssn_state->handles[handle];
}

pn_link_t *pn_sender(pn_session_t *session, const char *name)
{
  if (!session) return NULL;
  pn_link_t *snd = (pn_link_t *) malloc(sizeof(pn_link_t));
  if (!snd) return NULL;
  pn_link_init(snd, SENDER, session, name);
  return snd;
}

pn_link_t *pn_receiver(pn_session_t *session, const char *name)
{
  if (!session) return NULL;
  pn_link_t *rcv = (pn_link_t *) malloc(sizeof(pn_link_t));
  if (!rcv) return NULL;
  pn_link_init(rcv, RECEIVER, session, name);
  return rcv;
}

pn_state_t pn_link_state(pn_link_t *link)
{
  return link->endpoint.state;
}

pn_error_t *pn_link_error(pn_link_t *link)
{
  return link->endpoint.error;
}

const char *pn_link_name(pn_link_t *link)
{
  return link ? link->name : NULL;
}

bool pn_link_is_sender(pn_link_t *link)
{
  return link->endpoint.type == SENDER;
}

bool pn_link_is_receiver(pn_link_t *link)
{
  return link->endpoint.type == RECEIVER;
}

pn_session_t *pn_link_session(pn_link_t *link)
{
  return link->session;
}

pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag)
{
  if (!link) return NULL;
  pn_delivery_t *delivery = link->settled_head;
  LL_POP(link, settled);
  if (!delivery) {
    delivery = (pn_delivery_t *) malloc(sizeof(pn_delivery_t));
    if (!delivery) return NULL;
    delivery->tag = pn_buffer(16);
    delivery->bytes = pn_buffer(64);
  }
  delivery->link = link;
  pn_buffer_clear(delivery->tag);
  pn_buffer_append(delivery->tag, tag.bytes, tag.size);
  delivery->local_state = 0;
  delivery->remote_state = 0;
  delivery->local_settled = false;
  delivery->remote_settled = false;
  delivery->updated = false;
  delivery->settled = false;
  LL_ADD(link, unsettled, delivery);
  delivery->work_next = NULL;
  delivery->work_prev = NULL;
  delivery->work = false;
  delivery->tpwork_next = NULL;
  delivery->tpwork_prev = NULL;
  delivery->tpwork = false;
  pn_buffer_clear(delivery->bytes);
  delivery->done = false;
  delivery->transport_context = NULL;
  delivery->context = NULL;

  if (!link->current)
    link->current = delivery;

  link->unsettled_count++;

  pn_work_update(link->session->connection, delivery);

  return delivery;
}

int pn_link_unsettled(pn_link_t *link)
{
  return link->unsettled_count;
}

pn_delivery_t *pn_unsettled_head(pn_link_t *link)
{
  pn_delivery_t *d = link->unsettled_head;
  while (d && d->local_settled) {
    d = d->unsettled_next;
  }
  return d;
}

pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery)
{
  pn_delivery_t *d = delivery->unsettled_next;
  while (d && d->local_settled) {
    d = d->unsettled_next;
  }
  return d;
}

bool pn_is_current(pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  return pn_link_current(link) == delivery;
}

void pn_delivery_dump(pn_delivery_t *d)
{
  char tag[1024];
  pn_bytes_t bytes = pn_buffer_bytes(d->tag);
  pn_quote_data(tag, 1024, bytes.start, bytes.size);
  printf("{tag=%s, local_state=%u, remote_state=%u, local_settled=%u, "
         "remote_settled=%u, updated=%u, current=%u, writable=%u, readable=%u, "
         "work=%u}",
         tag, d->local_state, d->remote_state, d->local_settled,
         d->remote_settled, d->updated, pn_is_current(d),
         pn_delivery_writable(d), pn_delivery_readable(d), d->work);
}

void *pn_delivery_get_context(pn_delivery_t *delivery)
{
    return delivery ? delivery->context : NULL;
}

void pn_delivery_set_context(pn_delivery_t *delivery, void *context)
{
    if (delivery)
        delivery->context = context;
}

pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_bytes_t tag = pn_buffer_bytes(delivery->tag);
    return pn_dtag(tag.start, tag.size);
  } else {
    return (pn_delivery_tag_t) {0};
  }
}

pn_delivery_t *pn_link_current(pn_link_t *link)
{
  if (!link) return NULL;
  return link->current;
}

void pn_advance_sender(pn_link_t *link)
{
  link->current->done = true;
  link->queued++;
  link->credit--;
  pn_add_tpwork(link->current);
  link->current = link->current->unsettled_next;
}

void pn_advance_receiver(pn_link_t *link)
{
  link->credit--;
  link->queued--;
  link->current = link->current->unsettled_next;
}

bool pn_link_advance(pn_link_t *link)
{
  if (link && link->current) {
    pn_delivery_t *prev = link->current;
    if (link->endpoint.type == SENDER) {
      pn_advance_sender(link);
    } else {
      pn_advance_receiver(link);
    }
    pn_delivery_t *next = link->current;
    pn_work_update(link->session->connection, prev);
    if (next) pn_work_update(link->session->connection, next);
    return prev != next;
  } else {
    return false;
  }
}

int pn_link_credit(pn_link_t *link)
{
  return link ? link->credit : 0;
}

int pn_link_available(pn_link_t *link)
{
  return link ? link->available : 0;
}

int pn_link_queued(pn_link_t *link)
{
  return link ? link->queued : 0;
}

void pn_real_settle(pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  LL_REMOVE(link, unsettled, delivery);
  // TODO: what if we settle the current delivery?
  LL_ADD(link, settled, delivery);
  pn_buffer_clear(delivery->tag);
  pn_buffer_clear(delivery->bytes);
  delivery->settled = true;
}

void pn_full_settle(pn_delivery_buffer_t *db, pn_delivery_t *delivery)
{
  pn_delivery_state_t *state = (pn_delivery_state_t *) delivery->transport_context;
  delivery->transport_context = NULL;
  if (state) state->delivery = NULL;
  pn_real_settle(delivery);
  if (state) pn_delivery_buffer_gc(db);
  pn_clear_tpwork(delivery);
}

void pn_delivery_settle(pn_delivery_t *delivery)
{
  if (!delivery) return;

  pn_link_t *link = delivery->link;
  if (pn_is_current(delivery)) {
    pn_link_advance(link);
  }

  link->unsettled_count--;
  delivery->local_settled = true;
  pn_add_tpwork(delivery);
  pn_work_update(delivery->link->session->connection, delivery);
}

int pn_post_close(pn_transport_t *transport)
{
  const char *condition = transport->condition;
  return pn_post_frame(transport->disp, 0, "DL[?DL[s]]", CLOSE, (bool) condition, ERROR, condition);
}

int pn_do_error(pn_transport_t *transport, const char *condition, const char *fmt, ...)
{
  va_list ap;
  transport->condition = condition;
  va_start(ap, fmt);
  char buf[1024];
  // XXX: result
  vsnprintf(buf, 1024, fmt, ap);
  va_end(ap);
  pn_error_set(transport->error, PN_ERR, buf);
  if (!transport->close_sent) {
    pn_post_close(transport);
    transport->close_sent = true;
  }
  transport->disp->halt = true;
  fprintf(stderr, "ERROR %s %s\n", condition, pn_error_text(transport->error));
  return PN_ERR;
}

int pn_do_open(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  pn_connection_t *conn = transport->connection;
  bool container_q, hostname_q;
  pn_bytes_t remote_container, remote_hostname;
  pn_data_clear(transport->remote_offered_capabilities);
  pn_data_clear(transport->remote_desired_capabilities);
  int err = pn_scan_args(disp, "D.[?S?SI.I..CC]", &container_q,
                         &remote_container, &hostname_q, &remote_hostname,
                         &transport->remote_max_frame,
                         &transport->remote_idle_timeout,
                         transport->remote_offered_capabilities,
                         transport->remote_desired_capabilities);
  if (err) return err;
  if (transport->remote_max_frame > 0) {
    if (transport->remote_max_frame < AMQP_MIN_MAX_FRAME_SIZE) {
      fprintf(stderr, "Peer advertised bad max-frame (%u), forcing to %u\n",
              transport->remote_max_frame, AMQP_MIN_MAX_FRAME_SIZE);
      transport->remote_max_frame = AMQP_MIN_MAX_FRAME_SIZE;
    }
    disp->remote_max_frame = transport->remote_max_frame;
    pn_buffer_clear( disp->frame );
    pn_buffer_ensure( disp->frame, disp->remote_max_frame );
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
  } else {
    transport->disp->halt = true;
  }
  if (transport->remote_idle_timeout)
    transport->process_tick = pn_process_tick;  // enable timeouts
  transport->open_rcvd = true;
  return 0;
}

int pn_do_begin(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  bool reply;
  uint16_t remote_channel;
  pn_sequence_t next;
  int err = pn_scan_args(disp, "D.[?HI]", &reply, &remote_channel, &next);
  if (err) return err;

  pn_session_state_t *state;
  if (reply) {
    // XXX: what if session is NULL?
    state = &transport->sessions[remote_channel];
  } else {
    pn_session_t *ssn = pn_session(transport->connection);
    state = pn_session_get_state(transport, ssn);
  }
  state->incoming_transfer_count = next;
  pn_map_channel(transport, disp->channel, state);
  PN_SET_REMOTE(state->session->endpoint.state, PN_REMOTE_ACTIVE);

  return 0;
}

pn_link_state_t *pn_find_link(pn_session_state_t *ssn_state, pn_bytes_t name, bool is_sender)
{
  pn_endpoint_type_t type = is_sender ? SENDER : RECEIVER;

  for (int i = 0; i < ssn_state->session->link_count; i++)
  {
    pn_link_t *link = ssn_state->session->links[i];
    if (link->endpoint.type == type &&
        !strncmp(name.start, link->name, name.size))
    {
      return pn_link_get_state(ssn_state, link);
    }
  }
  return NULL;
}

static pn_expiry_policy_t symbol2policy(pn_bytes_t symbol)
{
  if (!symbol.start)
    return PN_SESSION_CLOSE;

  if (!strncmp(symbol.start, "link-detach", symbol.size))
    return PN_LINK_CLOSE;
  if (!strncmp(symbol.start, "session-end", symbol.size))
    return PN_SESSION_CLOSE;
  if (!strncmp(symbol.start, "connection-close", symbol.size))
    return PN_CONNECTION_CLOSE;
  if (!strncmp(symbol.start, "never", symbol.size))
    return PN_NEVER;

  return PN_SESSION_CLOSE;
}

int pn_do_attach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  pn_bytes_t name;
  uint32_t handle;
  bool is_sender;
  pn_bytes_t source, target;
  pn_durability_t src_dr, tgt_dr;
  pn_bytes_t src_exp, tgt_exp;
  pn_seconds_t src_timeout, tgt_timeout;
  bool src_dynamic, tgt_dynamic;
  pn_sequence_t idc;
  int err = pn_scan_args(disp, "D.[SIo..D.[SIsIo]D.[SIsIo]..I]", &name, &handle,
                         &is_sender,
                         &source, &src_dr, &src_exp, &src_timeout, &src_dynamic,
                         &target, &tgt_dr, &tgt_exp, &tgt_timeout, &tgt_dynamic,
                         &idc);
  if (err) return err;
  char strname[name.size + 1];
  strncpy(strname, name.start, name.size);
  strname[name.size] = '\0';

  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_link_state_t *link_state = pn_find_link(ssn_state, name, is_sender);
  pn_link_t *link;
  if (!link_state) {
    if (is_sender) {
      link = (pn_link_t *) pn_sender(ssn_state->session, strname);
    } else {
      link = (pn_link_t *) pn_receiver(ssn_state->session, strname);
    }
    link_state = pn_link_get_state(ssn_state, link);
  } else {
    link = link_state->link;
  }

  pn_map_handle(ssn_state, handle, link_state);
  PN_SET_REMOTE(link->endpoint.state, PN_REMOTE_ACTIVE);
  pn_terminus_t *rsrc = &link_state->link->remote_source;
  if (source.start) {
    pn_terminus_set_type(rsrc, PN_SOURCE);
    pn_terminus_set_address_bytes(rsrc, source);
    pn_terminus_set_durability(rsrc, src_dr);
    pn_terminus_set_expiry_policy(rsrc, symbol2policy(src_exp));
    pn_terminus_set_timeout(rsrc, src_timeout);
    pn_terminus_set_dynamic(rsrc, src_dynamic);
  } else {
    pn_terminus_set_type(rsrc, PN_UNSPECIFIED);
  }
  pn_terminus_t *rtgt = &link_state->link->remote_target;
  if (target.start) {
    pn_terminus_set_type(rtgt, PN_TARGET);
    pn_terminus_set_address_bytes(rtgt, target);
    pn_terminus_set_durability(rtgt, tgt_dr);
    pn_terminus_set_expiry_policy(rtgt, symbol2policy(tgt_exp));
    pn_terminus_set_timeout(rtgt, tgt_timeout);
    pn_terminus_set_dynamic(rtgt, tgt_dynamic);
  } else {
    pn_terminus_set_type(rtgt, PN_UNSPECIFIED);
  }

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
    link_state->delivery_count = idc;
  }

  return 0;
}

int pn_post_flow(pn_transport_t *transport, pn_session_state_t *ssn_state, pn_link_state_t *state);

int pn_do_transfer(pn_dispatcher_t *disp)
{
  // XXX: multi transfer
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  uint32_t handle;
  pn_bytes_t tag;
  bool id_present;
  pn_sequence_t id;
  bool more;
  int err = pn_scan_args(disp, "D.[I?Iz..o]", &handle, &id_present, &id, &tag,
                         &more);
  if (err) return err;
  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_link_state_t *link_state = pn_handle_state(ssn_state, handle);
  pn_link_t *link = link_state->link;
  pn_delivery_t *delivery;
  if (link->unsettled_tail && !link->unsettled_tail->done) {
    delivery = link->unsettled_tail;
  } else {
    pn_delivery_buffer_t *incoming = &ssn_state->incoming;

    if (!pn_delivery_buffer_available(incoming)) {
      return pn_do_error(transport, "amqp:session:window-violation", "incoming session window exceeded");
    }

    if (!ssn_state->incoming_init) {
      incoming->next = id;
      ssn_state->incoming_init = true;
    }

    delivery = pn_delivery(link, pn_dtag(tag.start, tag.size));
    pn_delivery_state_t *state = pn_delivery_buffer_push(incoming, delivery);
    delivery->transport_context = state;
    if (id_present && id != state->id) {
      int err = pn_do_error(transport, "amqp:session:invalid-field",
                            "sequencing error, expected delivery-id %u, got %u",
                            state->id, id);
      // XXX: this will probably leave delivery buffer state messed up
      pn_full_settle(incoming, delivery);
      return err;
    }

    link_state->delivery_count++;
    link_state->link_credit--;
    link->queued++;
  }

  pn_buffer_append(delivery->bytes, disp->payload, disp->size);
  delivery->done = !more;

  ssn_state->incoming_transfer_count++;
  ssn_state->incoming_window--;

  if (!ssn_state->incoming_window && (int32_t) link_state->local_handle >= 0) {
    pn_post_flow(transport, ssn_state, link_state);
  }

  return 0;
}

int pn_do_flow(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  pn_sequence_t onext, inext, delivery_count;
  uint32_t iwin, owin, link_credit;
  uint32_t handle;
  bool inext_init, handle_init, dcount_init, drain;
  int err = pn_scan_args(disp, "D.[?IIII?I?II.o]", &inext_init, &inext, &iwin,
                         &onext, &owin, &handle_init, &handle, &dcount_init,
                         &delivery_count, &link_credit, &drain);
  if (err) return err;

  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);

  if (inext_init) {
    ssn_state->outgoing_window = inext + iwin - ssn_state->outgoing_transfer_count;
  } else {
    ssn_state->outgoing_window = iwin;
  }

  if (handle_init) {
    pn_link_state_t *link_state = pn_handle_state(ssn_state, handle);
    pn_link_t *link = link_state->link;
    if (link->endpoint.type == SENDER) {
      pn_sequence_t receiver_count;
      if (dcount_init) {
        receiver_count = delivery_count;
      } else {
        // our initial delivery count
        receiver_count = 0;
      }
      pn_sequence_t old = link_state->link_credit;
      link_state->link_credit = receiver_count + link_credit - link_state->delivery_count;
      link->credit += link_state->link_credit - old;
      link->drain = drain;
      pn_delivery_t *delivery = pn_link_current(link);
      if (delivery) pn_work_update(transport->connection, delivery);
    } else {
      pn_sequence_t delta = delivery_count - link_state->delivery_count;
      if (delta > 0) {
        link_state->delivery_count += delta;
        link_state->link_credit -= delta;
        link->credit -= delta;
      }
    }
  }

  return 0;
}

int pn_do_disposition(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
  bool role;
  pn_sequence_t first, last;
  uint64_t code;
  bool last_init, settled, code_init;
  int err = pn_scan_args(disp, "D.[oI?IoD?L[]]", &role, &first, &last_init,
                         &last, &settled, &code_init, &code);
  if (err) return err;
  if (!last_init) last = first;

  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_disposition_t dispo = 0;
  if (code_init) {
    switch (code)
    {
    case ACCEPTED:
      dispo = PN_ACCEPTED;
      break;
    case REJECTED:
      dispo = PN_REJECTED;
      break;
    default:
      // XXX
      fprintf(stderr, "default %" PRIu64 "\n", code);
      break;
    }
  }

  pn_delivery_buffer_t *deliveries;
  if (role) {
    deliveries = &ssn_state->outgoing;
  } else {
    deliveries = &ssn_state->incoming;
  }

  pn_sequence_t lwm = pn_delivery_buffer_lwm(deliveries);

  for (pn_sequence_t id = first; id <= last; id++) {
    if (id < lwm) continue;
    pn_delivery_state_t *state = pn_delivery_buffer_get(deliveries, id - lwm);
    pn_delivery_t *delivery = state->delivery;
    if (delivery) {
      delivery->remote_state = dispo;
      delivery->remote_settled = settled;
      delivery->updated = true;
      pn_work_update(transport->connection, delivery);
    }
  }

  return 0;
}

int pn_do_detach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  uint32_t handle;
  bool closed;
  int err = pn_scan_args(disp, "D.[Io]", &handle, &closed);
  if (err) return err;

  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  if (!ssn_state) {
    return pn_do_error(transport, "amqp:invalid-field", "no such channel: %u", disp->channel);
  }
  pn_link_state_t *link_state = pn_handle_state(ssn_state, handle);
  pn_link_t *link = link_state->link;

  link_state->remote_handle = -2;

  if (closed)
  {
    PN_SET_REMOTE(link->endpoint.state, PN_REMOTE_CLOSED);
  } else {
    // TODO: implement
  }

  return 0;
}

int pn_do_end(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_session_t *session = ssn_state->session;

  ssn_state->remote_channel = -2;
  PN_SET_REMOTE(session->endpoint.state, PN_REMOTE_CLOSED);
  return 0;
}

int pn_do_close(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = (pn_transport_t *) disp->context;
  transport->close_rcvd = true;
  PN_SET_REMOTE(transport->connection->endpoint.state, PN_REMOTE_CLOSED);
  return 0;
}

ssize_t pn_transport_input(pn_transport_t *transport, const char *bytes, size_t available)
{
  if (!transport) return PN_ARG_ERR;

  size_t consumed = 0;

  const bool use_ssl = transport->ssl != NULL;
  while (true) {
    ssize_t n;
    if (use_ssl)
      n = pn_ssl_input( transport->ssl, bytes + consumed, available - consumed);
    else
      n = transport->process_input(transport, bytes + consumed, available - consumed);
    if (n > 0) {
      consumed += n;
      if (consumed >= available) {
        break;
      }
    } else if (n == 0) {
      break;
    } else {
      if (n != PN_EOS) {
        pn_dispatcher_trace(transport->disp, 0, "ERROR[%i] %s\n",
                            pn_error_code(transport->error),
                            pn_error_text(transport->error));
      }
      if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
        pn_dispatcher_trace(transport->disp, 0, "<- EOS\n");
      return n;
    }
  }

  transport->bytes_input += consumed;
  return consumed;
}

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")

static ssize_t pn_input_read_header(pn_transport_t *transport, const char *bytes, size_t available,
                                    const char *header, size_t size, const char *protocol,
                                    ssize_t (*next)(pn_transport_t *, const char *, size_t))
{
  const char *point = header + transport->header_count;
  int delta = pn_min(available, size - transport->header_count);
  if (!available || memcmp(bytes, point, delta)) {
    char quoted[1024];
    pn_quote_data(quoted, 1024, bytes, available);
    return pn_error_format(transport->error, PN_ERR,
                           "%s header mismatch: '%s'", protocol, quoted);
  } else {
    transport->header_count += delta;
    if (transport->header_count == size) {
      transport->header_count = 0;
      transport->process_input = next;

      if (transport->disp->trace & PN_TRACE_FRM)
        fprintf(stderr, "    <- %s\n", protocol);
    }
    return delta;
  }
}

static ssize_t pn_input_read_sasl_header(pn_transport_t *transport, const char *bytes, size_t available)
{
  return pn_input_read_header(transport, bytes, available, SASL_HEADER, 8, "SASL", pn_input_read_sasl);
}

static ssize_t pn_input_read_sasl(pn_transport_t *transport, const char *bytes, size_t available)
{
  pn_sasl_t *sasl = transport->sasl;
  ssize_t n = pn_sasl_input(sasl, bytes, available);
  if (n == PN_EOS) {
    transport->process_input = pn_input_read_amqp_header;
    return transport->process_input(transport, bytes, available);
  } else {
    return n;
  }
}

#define AMQP_HEADER ("AMQP\x00\x01\x00\x00")

static ssize_t pn_input_read_amqp_header(pn_transport_t *transport, const char *bytes, size_t available)
{
  return pn_input_read_header(transport, bytes, available, AMQP_HEADER, 8,
                              "AMQP", pn_input_read_amqp);
}

static ssize_t pn_input_read_amqp(pn_transport_t *transport, const char *bytes, size_t available)
{
  if (transport->close_rcvd) {
    if (available > 0) {
      pn_do_error(transport, "amqp:connection:framing-error", "data after close");
      return PN_ERR;
    } else {
      return PN_EOS;
    }
  }

  if (!available) {
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    return PN_ERR;
  }


  ssize_t n = pn_dispatcher_input(transport->disp, bytes, available);
  if (n < 0) {
    return pn_error_set(transport->error, n, "dispatch error");
  } else if (transport->close_rcvd) {
    return PN_EOS;
  } else {
    return n;
  }
}

/* process AMQP related timer events */
static pn_timestamp_t pn_process_tick(pn_transport_t *transport, pn_timestamp_t now)
{
  pn_timestamp_t timeout = 0;

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
        transport->keepalive_deadline = now + (transport->remote_idle_timeout/2.0);
        transport->last_bytes_output = transport->bytes_output;
      } else if (transport->keepalive_deadline <= now) {
        transport->keepalive_deadline = now + (transport->remote_idle_timeout/2.0);
        if (transport->disp->available == 0) {    // no outbound data ready
          pn_post_frame(transport->disp, 0, "");  // so send empty frame
          transport->last_bytes_output += 8;      // and account for it!
      }
    }
    timeout = pn_timestamp_next_expire( timeout, transport->keepalive_deadline );
  }

  return timeout;
}

bool pn_delivery_buffered(pn_delivery_t *delivery)
{
  if (delivery->settled) return false;
  if (pn_link_is_sender(delivery->link)) {
    pn_delivery_state_t *state = (pn_delivery_state_t *) delivery->transport_context;
    if (state) {
      return (delivery->done && !state->sent) || pn_buffer_size(delivery->bytes) > 0;
    } else {
      return delivery->done;
    }
  } else {
    return false;
  }
}

int pn_process_conn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (!(endpoint->state & PN_LOCAL_UNINIT) && !transport->open_sent)
    {
      pn_connection_t *connection = (pn_connection_t *) endpoint;
      int err = pn_post_frame(transport->disp, 0, "DL[SS?In?InnCC]", OPEN,
                              connection->container,
                              connection->hostname,
                              // if not zero, advertise our max frame size and idle timeout
                              (bool)transport->local_max_frame, transport->local_max_frame,
                              (bool)transport->local_idle_timeout, transport->local_idle_timeout,
                              connection->offered_capabilities,
                              connection->desired_capabilities);
      if (err) return err;
      transport->open_sent = true;
    }
  }

  return 0;
}

int pn_process_ssn_setup(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION && transport->open_sent)
  {
    pn_session_t *ssn = (pn_session_t *) endpoint;
    pn_session_state_t *state = pn_session_get_state(transport, ssn);
    if (!(endpoint->state & PN_LOCAL_UNINIT) && state->local_channel == (uint16_t) -1)
    {
      // XXX: we use the session id as the outgoing channel, we depend
      // on this for looking up via remote channel
      uint16_t channel = ssn->id;
      pn_post_frame(transport->disp, channel, "DL[?HIII]", BEGIN,
                    ((int16_t) state->remote_channel >= 0), state->remote_channel,
                    state->outgoing_transfer_count,
                    pn_delivery_buffer_available(&state->incoming),
                    pn_delivery_buffer_available(&state->outgoing));
      state->local_channel = channel;
    }
  }

  return 0;
}

static const char *expiry_symbol(pn_expiry_policy_t policy)
{
  switch (policy)
  {
  case PN_LINK_CLOSE:
    return "link-detach";
  case PN_SESSION_CLOSE:
    return NULL;
  case PN_CONNECTION_CLOSE:
    return "connection-close";
  case PN_NEVER:
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
    pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
    pn_link_state_t *state = pn_link_get_state(ssn_state, link);
    if (((int16_t) ssn_state->local_channel >= 0) &&
        !(endpoint->state & PN_LOCAL_UNINIT) && state->local_handle == (uint32_t) -1)
    {
      // XXX
      state->local_handle = link->id;
      int err = pn_post_frame(transport->disp, ssn_state->local_channel,
                              "DL[SIonn?DL[SIsIoCnCnCC]?DL[SIsIoCC]nnI]", ATTACH,
                              link->name,
                              state->local_handle,
                              endpoint->type == RECEIVER,
                              (bool) link->source.type, SOURCE,
                              link->source.address,
                              link->source.durability,
                              expiry_symbol(link->source.expiry_policy),
                              link->source.timeout,
                              link->source.dynamic,
                              link->source.properties,
                              link->source.filter,
                              link->source.outcomes,
                              link->source.capabilities,
                              (bool) link->target.type, TARGET,
                              link->target.address,
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

int pn_post_flow(pn_transport_t *transport, pn_session_state_t *ssn_state, pn_link_state_t *state)
{
  ssn_state->incoming_window = pn_delivery_buffer_available(&ssn_state->incoming);
  bool link = (bool) state;
  return pn_post_frame(transport->disp, ssn_state->local_channel, "DL[?IIII?I?I?In?o]", FLOW,
                       (int16_t) ssn_state->remote_channel >= 0, ssn_state->incoming_transfer_count,
                       ssn_state->incoming_window,
                       ssn_state->outgoing.next,
                       pn_delivery_buffer_available(&ssn_state->outgoing),
                       link, state ? state->local_handle : 0,
                       link, state ? state->delivery_count : 0,
                       link, state ? state->link_credit : 0,
                       link, state ? state->link->drain : false);
}

int pn_process_flow_receiver(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == RECEIVER && endpoint->state & PN_LOCAL_ACTIVE)
  {
    pn_link_t *rcv = (pn_link_t *) endpoint;
    pn_session_state_t *ssn_state = pn_session_get_state(transport, rcv->session);
    pn_link_state_t *state = pn_link_get_state(ssn_state, rcv);
    if ((int16_t) ssn_state->local_channel >= 0 &&
        (int32_t) state->local_handle >= 0 &&
        ((rcv->drain || state->link_credit != rcv->credit - rcv->queued) || !ssn_state->incoming_window)) {
      state->link_credit = rcv->credit - rcv->queued;
      return pn_post_flow(transport, ssn_state, state);
    }
  }

  return 0;
}

int pn_flush_disp(pn_transport_t *transport, pn_session_state_t *ssn_state)
{
  uint64_t code = ssn_state->disp_code;
  bool settled = ssn_state->disp_settled;
  if (ssn_state->disp) {
    int err = pn_post_frame(transport->disp, ssn_state->local_channel, "DL[oIIo?DL[]]", DISPOSITION,
                            ssn_state->disp_type, ssn_state->disp_first, ssn_state->disp_last,
                            settled, (bool)code, code);
    if (err) return err;
    ssn_state->disp_type = 0;
    ssn_state->disp_code = 0;
    ssn_state->disp_settled = 0;
    ssn_state->disp_first = 0;
    ssn_state->disp_last = 0;
    ssn_state->disp = false;
  }
  return 0;
}

int pn_post_disp(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
  pn_modified(transport->connection, &link->session->endpoint);
  // XXX: check for null state
  pn_delivery_state_t *state = (pn_delivery_state_t *) delivery->transport_context;
  uint64_t code;
  switch(delivery->local_state) {
  case PN_ACCEPTED:
    code = ACCEPTED;
    break;
  case PN_RELEASED:
    code = RELEASED;
    break;
  case PN_REJECTED:
    code = REJECTED;
    break;
    //TODO: rejected and modified (both take extra data which may need to be passed through somehow) e.g. change from enum to discriminated union?
  default:
    code = 0;
  }

  if (!code && !delivery->local_settled) {
    return 0;
  }

  if (ssn_state->disp && code == ssn_state->disp_code &&
      delivery->local_settled == ssn_state->disp_settled &&
      ssn_state->disp_type == (link->endpoint.type == RECEIVER)) {
    if (state->id == ssn_state->disp_first - 1) {
      ssn_state->disp_first = state->id;
      return 0;
    } else if (state->id == ssn_state->disp_last + 1) {
      ssn_state->disp_last = state->id;
      return 0;
    }
  }

  if (ssn_state->disp) {
    int err = pn_flush_disp(transport, ssn_state);
    if (err) return err;
  }

  ssn_state->disp_type = (link->endpoint.type == RECEIVER);
  ssn_state->disp_code = code;
  ssn_state->disp_settled = delivery->local_settled;
  ssn_state->disp_first = state->id;
  ssn_state->disp_last = state->id;
  ssn_state->disp = true;

  return 0;
}

int pn_process_tpwork_sender(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
  pn_link_state_t *link_state = pn_link_get_state(ssn_state, link);
  if ((int16_t) ssn_state->local_channel >= 0 && (int32_t) link_state->local_handle >= 0) {
    pn_delivery_state_t *state = (pn_delivery_state_t *) delivery->transport_context;
    if (!state && pn_delivery_buffer_available(&ssn_state->outgoing)) {
      state = pn_delivery_buffer_push(&ssn_state->outgoing, delivery);
      delivery->transport_context = state;
    }

    if (state && !state->sent && (delivery->done || pn_buffer_size(delivery->bytes) > 0) &&
        ssn_state->outgoing_window > 0 && link_state->link_credit > 0) {
      pn_bytes_t bytes = pn_buffer_bytes(delivery->bytes);
      pn_set_payload(transport->disp, bytes.start, bytes.size);
      pn_buffer_clear(delivery->bytes);
      pn_bytes_t tag = pn_buffer_bytes(delivery->tag);
      int err = pn_post_transfer_frame(transport->disp,
                                       ssn_state->local_channel,
                                       link_state->local_handle,
                                       state->id, &tag,
                                       0, // message-format
                                       delivery->local_settled,
                                       !delivery->done);
      if (err) return err;
      ssn_state->outgoing_transfer_count++;
      ssn_state->outgoing_window--;
      if (delivery->done) {
        state->sent = true;
        link_state->delivery_count++;
        link_state->link_credit--;
        link->queued--;
      }
    }
  }

  pn_delivery_state_t *state = (pn_delivery_state_t *) delivery->transport_context;
  // XXX: need to prevent duplicate disposition sending
  if ((int16_t) ssn_state->local_channel >= 0 && !delivery->remote_settled
      && state && state->sent) {
    int err = pn_post_disp(transport, delivery);
    if (err) return err;
  }

  if (delivery->local_settled && state && state->sent) {
    pn_full_settle(&ssn_state->outgoing, delivery);
  }

  return 0;
}

int pn_process_tpwork_receiver(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  // XXX: need to prevent duplicate disposition sending
  pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
  if ((int16_t) ssn_state->local_channel >= 0 && !delivery->remote_settled && delivery->transport_context) {
    int err = pn_post_disp(transport, delivery);
    if (err) return err;
  }

  if (delivery->local_settled) {
    size_t available = pn_delivery_buffer_available(&ssn_state->incoming);
    pn_full_settle(&ssn_state->incoming, delivery);
    if (!ssn_state->incoming_window &&
        pn_delivery_buffer_available(&ssn_state->incoming) > available) {
      int err = pn_post_flow(transport, ssn_state, NULL);
      if (err) return err;
    }
  }

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
      if (!delivery->transport_context && transport->disp->available > 0) {
        break;
      }

      pn_link_t *link = delivery->link;
      if (pn_link_is_sender(link)) {
        int err = pn_process_tpwork_sender(transport, delivery);
        if (err) return err;
      } else {
        int err = pn_process_tpwork_receiver(transport, delivery);
        if (err) return err;
      }

      if (!pn_delivery_buffered(delivery)) {
        pn_clear_tpwork(delivery);
      }

      delivery = delivery->tpwork_next;
    }
  }

  return 0;
}

int pn_process_flush_disp(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION) {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = pn_session_get_state(transport, session);
    if ((int16_t) state->local_channel >= 0 && !transport->close_sent)
    {
      int err = pn_flush_disp(transport, state);
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
    pn_session_state_t *ssn_state = pn_session_get_state(transport, snd->session);
    pn_link_state_t *state = pn_link_get_state(ssn_state, snd);
    if ((int16_t) ssn_state->local_channel >= 0 &&
        (int32_t) state->local_handle >= 0 &&
        snd->drain && snd->drained) {
      pn_delivery_t *tail = state->link->unsettled_tail;
      if (!tail || !pn_delivery_buffered(tail)) {
        state->delivery_count += state->link_credit;
        state->link_credit = 0;
        snd->drained = false;
        return pn_post_flow(transport, ssn_state, state);
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
    pn_session_state_t *ssn_state = pn_session_get_state(transport, session);
    pn_link_state_t *state = pn_link_get_state(ssn_state, link);
    if (endpoint->state & PN_LOCAL_CLOSED && (int32_t) state->local_handle >= 0 &&
        (int16_t) ssn_state->local_channel >= 0 && !transport->close_sent) {
      if (pn_link_is_sender(link) && pn_link_queued(link) &&
          (int32_t) state->remote_handle != -2 &&
          (int16_t) ssn_state->remote_channel != -2 &&
          !transport->close_rcvd) return 0;
      int err = pn_post_frame(transport->disp, ssn_state->local_channel, "DL[Io]", DETACH,
                              state->local_handle, true);
      if (err) return err;
      state->local_handle = -2;
    }

    pn_clear_modified(transport->connection, endpoint);
  }

  return 0;
}

bool pn_pointful_buffering(pn_transport_t *transport, pn_session_t *session)
{
  if (!transport->open_rcvd) return true;
  if (transport->close_rcvd) return false;

  pn_connection_t *conn = transport->connection;
  pn_link_t *link = pn_link_head(conn, 0);
  while (link) {
    if (pn_link_is_sender(link) && pn_link_queued(link) > 0) {
      pn_session_t *ssn = link->session;
      if (session && session == ssn) {
        pn_session_state_t *ssn_state = pn_session_get_state(transport, session);
        pn_link_state_t *state = pn_link_get_state(ssn_state, link);

        if ((int32_t) state->remote_handle != -2 &&
            (int16_t) ssn_state->remote_channel != -2) {
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
    pn_session_state_t *state = pn_session_get_state(transport, session);
    if (endpoint->state & PN_LOCAL_CLOSED && (int16_t) state->local_channel >= 0
        && !transport->close_sent)
    {
      if (pn_pointful_buffering(transport, session)) return 0;
      int err = pn_post_frame(transport->disp, state->local_channel, "DL[]", END);
      if (err) return err;
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
      int err = pn_post_close(transport);
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
    pn_modified(transport->connection, &transport->connection->endpoint);
  }

  return 0;
}

static ssize_t pn_output_write_header(pn_transport_t *transport,
                                      char *bytes, size_t size,
                                      const char *header, size_t hdrsize,
                                      const char *protocol,
                                      ssize_t (*next)(pn_transport_t *, char *, size_t))
{
  if (transport->disp->trace & PN_TRACE_FRM)
    fprintf(stderr, "    -> %s\n", protocol);
  if (size >= hdrsize) {
    memmove(bytes, header, hdrsize);
    transport->process_output = next;
    return hdrsize;
  } else {
    return pn_error_format(transport->error, PN_UNDERFLOW, "underflow writing %s header", protocol);
  }
}

static ssize_t pn_output_write_sasl_header(pn_transport_t *transport, char *bytes, size_t size)
{
  return pn_output_write_header(transport, bytes, size, SASL_HEADER, 8, "SASL",
                                pn_output_write_sasl);
}

static ssize_t pn_output_write_sasl(pn_transport_t *transport, char *bytes, size_t size)
{
  pn_sasl_t *sasl = transport->sasl;
  ssize_t n = pn_sasl_output(sasl, bytes, size);
  if (n == PN_EOS) {
    transport->process_output = pn_output_write_amqp_header;
    return transport->process_output(transport, bytes, size);
  } else {
    return n;
  }
}

static ssize_t pn_output_write_amqp_header(pn_transport_t *transport, char *bytes, size_t size)
{
  return pn_output_write_header(transport, bytes, size, AMQP_HEADER, 8, "AMQP",
                                pn_output_write_amqp);
}

static ssize_t pn_output_write_amqp(pn_transport_t *transport, char *bytes, size_t size)
{
  if (!transport->connection) {
    return 0;
  }

  if (!pn_error_code(transport->error)) {
    pn_error_set(transport->error, pn_process(transport), "process error");
  }

  if (!transport->disp->available && (transport->close_sent || pn_error_code(transport->error))) {
    if (pn_error_code(transport->error))
      return pn_error_code(transport->error);
    else
      return PN_EOS;
  }

  return pn_dispatcher_output(transport->disp, bytes, size);
}

ssize_t pn_transport_output(pn_transport_t *transport, char *bytes, size_t size)
{
  if (!transport) return PN_ARG_ERR;

  size_t total = 0;

  const bool use_ssl = transport->ssl != NULL;

  while (size - total > 0) {
    ssize_t n;
    if (use_ssl)
      n = pn_ssl_output( transport->ssl, bytes + total, size - total);
    else
      n = transport->process_output(transport, bytes + total, size - total);
    if (n > 0) {
      total += n;
    } else if (n == 0) {
      break;
    } else if (n == PN_EOS) {
      if (total > 0) {
        break;
      } else {
        if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
          pn_dispatcher_trace(transport->disp, 0, "-> EOS\n");
        return PN_EOS;
      }
    } else {
      if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
        pn_dispatcher_trace(transport->disp, 0, "-> EOS (%zi) %s\n", n,
                            pn_error_text(transport->error));
      return n;
    }
  }

  transport->bytes_output += total;
  return total;
}

void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace)
{
  if (transport->sasl) pn_sasl_trace(transport->sasl, trace);
  if (transport->ssl) pn_ssl_trace(transport->ssl, trace);
  transport->disp->trace = trace;
}

uint32_t pn_transport_get_max_frame(pn_transport_t *transport)
{
  return transport->local_max_frame;
}

void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size)
{
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
  transport->process_tick = pn_process_tick;
}

pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport)
{
  return transport->remote_idle_timeout;
}

void pn_link_offered(pn_link_t *sender, int credit)
{
  sender->available = credit;
}

ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n)
{
  pn_delivery_t *current = pn_link_current(sender);
  if (!current) return PN_EOS;
  pn_buffer_append(current->bytes, bytes, n);
  pn_add_tpwork(current);
  return n;
}

void pn_link_drained(pn_link_t *sender)
{
  if (sender && sender->drain && sender->credit > 0) {
    sender->credit = 0;
    sender->drained = true;
    pn_modified(sender->session->connection, &sender->endpoint);
  }
}

ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n)
{
  if (!receiver) return PN_ARG_ERR;

  pn_delivery_t *delivery = receiver->current;
  if (delivery) {
    size_t size = pn_buffer_get(delivery->bytes, 0, n, bytes);
    pn_buffer_trim(delivery->bytes, size, 0);
    if (size) {
      return size;
    } else {
      return delivery->done ? PN_EOS : 0;
    }
  } else {
    return PN_STATE_ERR;
  }
}

void pn_link_flow(pn_link_t *receiver, int credit)
{
  if (receiver && pn_link_is_receiver(receiver)) {
    receiver->credit += credit;
    receiver->drain = false;
    pn_modified(receiver->session->connection, &receiver->endpoint);
  }
}

void pn_link_drain(pn_link_t *receiver, int credit)
{
  if (receiver && pn_link_is_receiver(receiver)) {
    pn_link_flow(receiver, credit);
    receiver->drain = true;
  }
}

pn_timestamp_t pn_transport_tick(pn_transport_t *transport, pn_timestamp_t now)
{
  if (transport && transport->process_tick)
    return transport->process_tick( transport, now );
  return 0;
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

pn_link_t *pn_delivery_link(pn_delivery_t *delivery)
{
  if (!delivery) return NULL;
  return delivery->link;
}

pn_disposition_t pn_delivery_local_state(pn_delivery_t *delivery)
{
  return (pn_disposition_t) delivery->local_state;
}

pn_disposition_t pn_delivery_remote_state(pn_delivery_t *delivery)
{
  return (pn_disposition_t) delivery->remote_state;
}

bool pn_delivery_settled(pn_delivery_t *delivery)
{
  return delivery ? delivery->remote_settled : false;
}

bool pn_delivery_updated(pn_delivery_t *delivery)
{
  return delivery ? delivery->updated : false;
}

void pn_delivery_clear(pn_delivery_t *delivery)
{
  delivery->updated = false;
  pn_work_update(delivery->link->session->connection, delivery);
}

void pn_delivery_update(pn_delivery_t *delivery, pn_disposition_t disposition)
{
  if (!delivery) return;
  delivery->local_state = disposition;
  pn_add_tpwork(delivery);
}

bool pn_delivery_writable(pn_delivery_t *delivery)
{
  if (!delivery) return false;

  pn_link_t *link = delivery->link;
  return pn_link_is_sender(link) && pn_is_current(delivery) && pn_link_credit(link) > 0;
}

bool pn_delivery_readable(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_link_t *link = delivery->link;
    return pn_link_is_receiver(link) && pn_is_current(delivery);
  } else {
    return false;
  }
}

size_t pn_delivery_pending(pn_delivery_t *delivery)
{
  return pn_buffer_size(delivery->bytes);
}

bool pn_delivery_partial(pn_delivery_t *delivery)
{
  return !delivery->done;
}
