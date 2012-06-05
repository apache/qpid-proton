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

void pn_delivery_buffer_destroy(pn_delivery_buffer_t *db)
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

void pn_destroy(pn_endpoint_t *endpoint)
{
  switch (endpoint->type)
  {
  case CONNECTION:
    pn_connection_destroy((pn_connection_t *)endpoint);
    break;
  case TRANSPORT:
    pn_transport_destroy((pn_transport_t *)endpoint);
    break;
  case SESSION:
    pn_session_destroy((pn_session_t *)endpoint);
    break;
  case SENDER:
    pn_link_destroy((pn_link_t *)endpoint);
    break;
  case RECEIVER:
    pn_link_destroy((pn_link_t *)endpoint);
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

void pn_connection_destroy(pn_connection_t *connection)
{
  if (!connection) return;

  pn_transport_destroy(connection->transport);
  while (connection->session_count)
    pn_session_destroy(connection->sessions[connection->session_count - 1]);
  free(connection->sessions);
  free(connection->container);
  free(connection->hostname);
  free(connection);
}

void pn_transport_open(pn_transport_t *transport)
{
  pn_open((pn_endpoint_t *) transport);
}

void pn_transport_close(pn_transport_t *transport)
{
  pn_close((pn_endpoint_t *) transport);
}

void pn_transport_destroy(pn_transport_t *transport)
{
  if (!transport) return;

  pn_dispatcher_destroy(transport->disp);
  for (int i = 0; i < transport->session_capacity; i++) {
    pn_delivery_buffer_destroy(&transport->sessions[i].incoming);
    pn_delivery_buffer_destroy(&transport->sessions[i].outgoing);
    free(transport->sessions[i].links);
    free(transport->sessions[i].handles);
  }
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

pn_connection_t *pn_get_connection(pn_session_t *session)
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

void pn_session_destroy(pn_session_t *session)
{
  if (!session) return;

  while (session->link_count)
    pn_link_destroy(session->links[session->link_count - 1]);
  pn_remove_session(session->connection, session);
  free(session->links);
  free(session);
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

void pn_clear_tag(pn_delivery_t *delivery)
{
  if (delivery->tag.start) {
    free(delivery->tag.start);
    delivery->tag = (pn_bytes_t) {0, NULL};
  }
}

void pn_clear_bytes(pn_delivery_t *delivery)
{
  if (delivery->capacity) {
    free(delivery->bytes);
    delivery->bytes = NULL;
    delivery->capacity = 0;
  }
}

void pn_free_deliveries(pn_delivery_t *delivery)
{
  while (delivery)
  {
    pn_delivery_t *next = delivery->link_next;
    pn_clear_tag(delivery);
    pn_clear_bytes(delivery);
    free(delivery);
    delivery = next;
  }
}

void pn_dump_deliveries(pn_delivery_t *delivery)
{
  if (delivery) {
    while (delivery)
    {
      printf("%p(%.*s)", (void *) delivery, (int) delivery->tag.size,
             delivery->tag.start);
      if (delivery->link_next) printf(" -> ");
      delivery = delivery->link_next;
    }
  } else {
    printf("NULL");
  }
}

void pn_link_dump(pn_link_t *link)
{
  pn_dump_deliveries(link->settled_head);
  printf("\n");
  pn_dump_deliveries(link->head);
  printf("\n");
}

void pn_link_open(pn_link_t *link)
{
  if (link) pn_open((pn_endpoint_t *) link);
}

void pn_link_close(pn_link_t *link)
{
  if (link) pn_close((pn_endpoint_t *) link);
}

void pn_link_destroy(pn_link_t *link)
{
  if (!link) return;

  free(link->local_source);
  free(link->local_target);
  free(link->remote_source);
  free(link->remote_target);
  pn_remove_link(link->session, link);
  pn_free_deliveries(link->settled_head);
  pn_free_deliveries(link->head);
  free(link->name);
  free(link);
}

void pn_endpoint_init(pn_endpoint_t *endpoint, int type, pn_connection_t *conn)
{
  endpoint->type = type;
  endpoint->state = PN_LOCAL_UNINIT | PN_REMOTE_UNINIT;
  endpoint->error = (pn_error_t) {.condition = NULL};
  endpoint->endpoint_next = NULL;
  endpoint->endpoint_prev = NULL;
  endpoint->transport_next = NULL;
  endpoint->transport_prev = NULL;
  endpoint->modified = false;

  LL_ADD_PFX(conn->endpoint_head, conn->endpoint_tail, endpoint, endpoint_);
}

pn_connection_t *pn_connection()
{
  pn_connection_t *conn = malloc(sizeof(pn_connection_t));
  if (!conn) return NULL;

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

  return conn;
}

pn_state_t pn_connection_state(pn_connection_t *connection)
{
  return connection ? connection->endpoint.state : 0;
}

pn_error_t *pn_connection_error(pn_connection_t *connection)
{
  return connection ? &connection->endpoint.error : NULL;
}

char *pn_connection_container(pn_connection_t *connection)
{
  return connection ? connection->container : NULL;
}

void pn_connection_set_container(pn_connection_t *connection, const char *container)
{
  if (!connection) return;
  if (connection->container) free(connection->container);
  connection->container = pn_strdup(container);
}

char *pn_connection_hostname(pn_connection_t *connection)
{
  return connection ? connection->hostname : NULL;
}

void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname)
{
  if (!connection) return;
  if (connection->hostname) free(connection->hostname);
  connection->hostname = pn_strdup(hostname);
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
    LL_ADD_PFX(connection->work_head, connection->work_tail, delivery, work_);
    delivery->work = true;
  }
}

void pn_clear_work(pn_connection_t *connection, pn_delivery_t *delivery)
{
  if (delivery->work)
  {
    LL_REMOVE_PFX(connection->work_head, connection->work_tail, delivery, work_);
    delivery->work = false;
  }
}

void pn_work_update(pn_connection_t *connection, pn_delivery_t *delivery)
{
  pn_link_t *link = pn_link(delivery);
  pn_delivery_t *current = pn_current(link);
  if (delivery->updated && !delivery->local_settled) {
    pn_add_work(connection, delivery);
  } else if (delivery == current) {
    if (link->endpoint.type == SENDER) {
      if (pn_credit(link) > 0) {
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
    LL_ADD_PFX(connection->tpwork_head, connection->tpwork_tail, delivery, tpwork_);
    delivery->tpwork = true;
  }
  pn_modified(connection, &connection->endpoint);
}

void pn_clear_tpwork(pn_delivery_t *delivery)
{
  pn_connection_t *connection = delivery->link->session->connection;
  if (delivery->tpwork)
  {
    LL_REMOVE_PFX(connection->tpwork_head, connection->tpwork_tail, delivery, tpwork_);
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
    LL_ADD_PFX(connection->transport_head, connection->transport_tail, endpoint, transport_);
    endpoint->modified = true;
  }
}

void pn_clear_modified(pn_connection_t *connection, pn_endpoint_t *endpoint)
{
  if (endpoint->modified) {
    LL_REMOVE_PFX(connection->transport_head, connection->transport_tail, endpoint, transport_);
    endpoint->transport_next = NULL;
    endpoint->transport_prev = NULL;
    endpoint->modified = false;
  }
}

bool pn_matches(pn_endpoint_t *endpoint, pn_endpoint_type_t type, pn_state_t state)
{
  if (endpoint->type != type) return false;

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
  pn_session_t *ssn = malloc(sizeof(pn_session_t));
  if (!ssn) return NULL;

  pn_endpoint_init(&ssn->endpoint, SESSION, conn);
  pn_add_session(conn, ssn);
  ssn->links = NULL;
  ssn->link_capacity = 0;
  ssn->link_count = 0;

  return ssn;
}

pn_state_t pn_session_state(pn_session_t *session)
{
  return session->endpoint.state;
}

pn_error_t *pn_session_error(pn_session_t *session)
{
  return &session->endpoint.error;
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

void pn_transport_init(pn_transport_t *transport)
{
  pn_endpoint_init(&transport->endpoint, TRANSPORT, transport->connection);

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
  transport->error = 0;

  transport->sessions = NULL;
  transport->session_capacity = 0;

  transport->channels = NULL;
  transport->channel_capacity = 0;
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

pn_transport_t *pn_transport(pn_connection_t *conn)
{
  if (!conn) return NULL;

  if (conn->transport) {
    return NULL;
  } else {
    conn->transport = malloc(sizeof(pn_transport_t));
    if (!conn->transport) return NULL;

    conn->transport->connection = conn;
    pn_transport_init(conn->transport);
    return conn->transport;
  }
}

pn_error_t *pn_transport_error(pn_transport_t *transport)
{
  return &transport->endpoint.error;
}

void pn_link_init(pn_link_t *link, int type, pn_session_t *session, const char *name)
{
  pn_endpoint_init(&link->endpoint, type, session->connection);
  pn_add_link(session, link);
  link->name = pn_strdup(name);
  link->local_source = NULL;
  link->local_target = NULL;
  link->remote_source = NULL;
  link->remote_target = NULL;
  link->settled_head = link->settled_tail = NULL;
  link->head = link->tail = link->current = NULL;
  link->unsettled_count = 0;
  link->credit = 0;
  link->queued = 0;
  link->drain = false;
  link->drained = false;
}

const char *pn_source(pn_link_t *link)
{
  return link ? link->local_source : NULL;
}

const char *pn_target(pn_link_t *link)
{
  return link ? link->local_target : NULL;
}

void pn_set_source(pn_link_t *link, const char *source)
{
  if (!link) return;
  if (link->local_source) free(link->local_source);
  link->local_source = pn_strdup(source);
}

void pn_set_target(pn_link_t *link, const char *target)
{
  if (!link) return;
  if (link->local_target) free(link->local_target);
  link->local_target = pn_strdup(target);
}

char *pn_remote_source(pn_link_t *link)
{
  return link ? link->remote_source : NULL;
}

char *pn_remote_target(pn_link_t *link)
{
  return link ? link->remote_target : NULL;
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
  pn_link_t *snd = malloc(sizeof(pn_link_t));
  if (!snd) return NULL;
  pn_link_init(snd, SENDER, session, name);
  return snd;
}

pn_link_t *pn_receiver(pn_session_t *session, const char *name)
{
  if (!session) return NULL;
  pn_link_t *rcv = malloc(sizeof(pn_link_t));
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
  return &link->endpoint.error;
}

const char *pn_link_name(pn_link_t *link)
{
  return link ? link->name : NULL;
}

bool pn_is_sender(pn_link_t *link)
{
  return link->endpoint.type == SENDER;
}

bool pn_is_receiver(pn_link_t *link)
{
  return link->endpoint.type == RECEIVER;
}

pn_session_t *pn_get_session(pn_link_t *link)
{
  return link->session;
}

pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag)
{
  if (!link) return NULL;
  pn_delivery_t *delivery = link->settled_head;
  LL_POP_PFX(link->settled_head, link->settled_tail, link_);
  if (!delivery) delivery = malloc(sizeof(pn_delivery_t));
  if (!delivery) return NULL;
  delivery->link = link;
  delivery->tag.size = tag.size;
  delivery->tag.start = malloc(tag.size);
  memcpy(delivery->tag.start, tag.bytes, tag.size);
  delivery->local_state = 0;
  delivery->remote_state = 0;
  delivery->local_settled = false;
  delivery->remote_settled = false;
  delivery->updated = false;
  delivery->settled = false;
  LL_ADD_PFX(link->head, link->tail, delivery, link_);
  delivery->work_next = NULL;
  delivery->work_prev = NULL;
  delivery->work = false;
  delivery->tpwork_next = NULL;
  delivery->tpwork_prev = NULL;
  delivery->tpwork = false;
  delivery->bytes = NULL;
  delivery->size = 0;
  delivery->capacity = 0;
  delivery->done = false;
  delivery->context = NULL;

  if (!link->current)
    link->current = delivery;

  link->unsettled_count++;

  pn_work_update(link->session->connection, delivery);

  return delivery;
}

int pn_unsettled(pn_link_t *link)
{
  return link->unsettled_count;
}

pn_delivery_t *pn_unsettled_head(pn_link_t *link)
{
  return link->head;
}

pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery)
{
  return delivery->link_next;
}

bool pn_is_current(pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  return pn_current(link) == delivery;
}

void pn_delivery_dump(pn_delivery_t *d)
{
  char tag[1024];
  pn_quote_data(tag, 1024, d->tag.start, d->tag.size);
  printf("{tag=%s, local_state=%u, remote_state=%u, local_settled=%u, "
         "remote_settled=%u, updated=%u, current=%u, writable=%u, readable=%u, "
         "work=%u}",
         tag, d->local_state, d->remote_state, d->local_settled,
         d->remote_settled, d->updated, pn_is_current(d), pn_writable(d),
         pn_readable(d), d->work);
}

pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery)
{
  if (delivery) {
    return pn_dtag(delivery->tag.start, delivery->tag.size);
  } else {
    return (pn_delivery_tag_t) {0};
  }
}

pn_delivery_t *pn_current(pn_link_t *link)
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
  link->current = link->current->link_next;
}

void pn_advance_receiver(pn_link_t *link)
{
  link->credit--;
  link->queued--;
  link->current = link->current->link_next;
}

bool pn_advance(pn_link_t *link)
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

int pn_credit(pn_link_t *link)
{
  return link ? link->credit : 0;
}

int pn_queued(pn_link_t *link)
{
  return link ? link->queued : 0;
}

void pn_real_settle(pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  LL_REMOVE_PFX(link->head, link->tail, delivery, link_);
  // TODO: what if we settle the current delivery?
  LL_ADD_PFX(link->settled_head, link->settled_tail, delivery, link_);
  pn_clear_tag(delivery);
  pn_clear_bytes(delivery);
  delivery->settled = true;
}

void pn_full_settle(pn_delivery_buffer_t *db, pn_delivery_t *delivery)
{
  pn_delivery_state_t *state = delivery->context;
  delivery->context = NULL;
  if (state) state->delivery = NULL;
  pn_real_settle(delivery);
  if (state) pn_delivery_buffer_gc(db);
  pn_clear_tpwork(delivery);
}

void pn_settle(pn_delivery_t *delivery)
{
  if (!delivery) return;

  pn_link_t *link = delivery->link;
  if (pn_is_current(delivery)) {
    pn_advance(link);
  }

  link->unsettled_count--;
  delivery->local_settled = true;
  pn_add_tpwork(delivery);
  pn_work_update(delivery->link->session->connection, delivery);
}

int pn_post_close(pn_transport_t *transport)
{
  const char *condition = transport->endpoint.error.condition;
  return pn_post_frame(transport->disp, 0, "DL[?DL[s]]", CLOSE, (bool) condition, ERROR, condition);
}

int pn_do_error(pn_transport_t *transport, const char *condition, const char *fmt, ...)
{
  va_list ap;
  transport->endpoint.error.condition = condition;
  va_start(ap, fmt);
  // XXX: result
  vsnprintf(transport->endpoint.error.description, DESCRIPTION, fmt, ap);
  va_end(ap);
  if (!transport->close_sent) {
    pn_post_close(transport);
    transport->close_sent = true;
  }
  transport->disp->halt = true;
  fprintf(stderr, "ERROR %s %s\n", condition, transport->endpoint.error.description);
  return PN_ERR;
}

int pn_do_open(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
  pn_connection_t *conn = transport->connection;
  PN_SET_REMOTE(conn->endpoint.state, PN_REMOTE_ACTIVE);
  return 0;
}

int pn_do_begin(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
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

char *pn_bytes_strdup(pn_bytes_t str)
{
  return pn_strndup(str.start, str.size);
}

int pn_do_attach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
  pn_bytes_t name;
  uint32_t handle;
  bool is_sender;
  pn_bytes_t source;
  pn_bytes_t target;
  pn_sequence_t idc;
  int err = pn_scan_args(disp, "D.[SIo..D.[S]D.[S]..I]", &name, &handle,
                         &is_sender, &source, &target, &idc);
  if (err) return err;
  char strname[name.size + 1];
  strncpy(strname, name.start, name.size);
  strname[name.size] = '\0';

  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_link_state_t *link_state = pn_find_link(ssn_state, name, is_sender);
  if (!link_state) {
    pn_link_t *link;
    if (is_sender) {
      link = (pn_link_t *) pn_sender(ssn_state->session, strname);
    } else {
      link = (pn_link_t *) pn_receiver(ssn_state->session, strname);
    }
    link_state = pn_link_get_state(ssn_state, link);
  }

  pn_map_handle(ssn_state, handle, link_state);
  PN_SET_REMOTE(link_state->link->endpoint.state, PN_REMOTE_ACTIVE);
  if (source.start)
    link_state->link->remote_source = pn_bytes_strdup(source);
  if (target.start)
    link_state->link->remote_target = pn_bytes_strdup(target);

  if (!is_sender) {
    link_state->delivery_count = idc;
  }

  return 0;
}

int pn_post_flow(pn_transport_t *transport, pn_session_state_t *ssn_state, pn_link_state_t *state);

int pn_do_transfer(pn_dispatcher_t *disp)
{
  // XXX: multi transfer
  pn_transport_t *transport = disp->context;
  uint32_t handle;
  pn_bytes_t tag;
  pn_sequence_t id;
  bool more;
  int err = pn_scan_args(disp, "D.[IIz..o]", &handle, &id, &tag, &more);
  if (err) return err;
  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_link_state_t *link_state = pn_handle_state(ssn_state, handle);
  pn_link_t *link = link_state->link;
  pn_delivery_t *delivery;
  if (link->tail && !link->tail->done) {
    delivery = link->tail;
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
    delivery->context = state;
    if (id != state->id) {
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

  PN_ENSURE(delivery->bytes, delivery->capacity, delivery->size + disp->size);
  memmove(delivery->bytes + delivery->size, disp->payload, disp->size);
  delivery->size += disp->size;
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
  pn_transport_t *transport = disp->context;
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
      pn_delivery_t *delivery = pn_current(link);
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
    delivery->remote_state = dispo;
    delivery->remote_settled = settled;
    delivery->updated = true;
    pn_work_update(transport->connection, delivery);
  }

  return 0;
}

int pn_do_detach(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
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

  link_state->remote_handle = -1;

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
  pn_transport_t *transport = disp->context;
  pn_session_state_t *ssn_state = pn_channel_state(transport, disp->channel);
  pn_session_t *session = ssn_state->session;

  ssn_state->remote_channel = -1;
  PN_SET_REMOTE(session->endpoint.state, PN_REMOTE_CLOSED);
  return 0;
}

int pn_do_close(pn_dispatcher_t *disp)
{
  pn_transport_t *transport = disp->context;
  transport->close_rcvd = true;
  PN_SET_REMOTE(transport->connection->endpoint.state, PN_REMOTE_CLOSED);
  return 0;
}

ssize_t pn_input(pn_transport_t *transport, char *bytes, size_t available)
{
  if (!available) {
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
      fprintf(stderr, "    <- EOS\n");
    return PN_ERR;
  }

  if (transport->close_rcvd) {
    pn_do_error(transport, "amqp:connection:framing-error", "data after close");
    if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
      fprintf(stderr, "    <- EOS\n");
    return PN_ERR;
  }

  ssize_t n = pn_dispatcher_input(transport->disp, bytes, available);
  if (n >= 0 && transport->close_rcvd) {
    if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
      fprintf(stderr, "    <- EOS\n");
    return PN_EOS;
  } else if (n < 0) {
    transport->error = n;
  }
  return n;
}

bool pn_delivery_buffered(pn_delivery_t *delivery)
{
  if (delivery->settled) return false;
  if (pn_is_sender(delivery->link)) {
    pn_delivery_state_t *state = delivery->context;
    if (state) {
      return (delivery->done && !state->sent) || delivery->size > 0;
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
      int err = pn_post_frame(transport->disp, 0, "DL[SS]", OPEN, connection->container, connection->hostname);
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
      pn_post_frame(transport->disp, ssn_state->local_channel, "DL[SIonn?DL[S]?DL[S]nnI]", ATTACH,
                    link->name,
                    state->local_handle,
                    endpoint->type == RECEIVER,
                    (bool) link->local_source, SOURCE, link->local_source,
                    (bool) link->local_target, TARGET, link->local_target,
                    0);
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
        ((state->link_credit != rcv->credit - rcv->queued) || !ssn_state->incoming_window)) {
      state->link_credit = rcv->credit - rcv->queued;
      return pn_post_flow(transport, ssn_state, state);
    }
  }

  return 0;
}

int pn_post_disp(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
  // XXX: check for null state
  pn_delivery_state_t *state = delivery->context;
  uint64_t code;
  switch(delivery->local_state) {
  case PN_ACCEPTED:
    code = ACCEPTED;
    break;
  case PN_RELEASED:
    code = RELEASED;
    break;
    //TODO: rejected and modified (both take extra data which may need to be passed through somehow) e.g. change from enum to discriminated union?
  default:
    code = 0;
  }

  if (code || delivery->local_settled)
    return pn_post_frame(transport->disp, ssn_state->local_channel, "DL[oIIo?DL[]]", DISPOSITION,
                         link->endpoint.type == RECEIVER, state->id, state->id,
                         delivery->local_settled,
                         (bool)code, code);
  return 0;
}

int pn_process_tpwork_sender(pn_transport_t *transport, pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  pn_session_state_t *ssn_state = pn_session_get_state(transport, link->session);
  pn_link_state_t *link_state = pn_link_get_state(ssn_state, link);
  if ((int16_t) ssn_state->local_channel >= 0 && (int32_t) link_state->local_handle >= 0) {
    pn_delivery_state_t *state = delivery->context;
    if (!state && pn_delivery_buffer_available(&ssn_state->outgoing)) {
      state = pn_delivery_buffer_push(&ssn_state->outgoing, delivery);
      delivery->context = state;
    }

    if (state && !state->sent && (delivery->done || delivery->size > 0) &&
        ssn_state->outgoing_window > 0 && link_state->link_credit > 0) {
      if (delivery->bytes) {
        pn_set_payload(transport->disp, delivery->bytes, delivery->size);
        delivery->size = 0;
      }
      int err = pn_post_frame(transport->disp, ssn_state->local_channel, "DL[IIzIoo]", TRANSFER,
                              link_state->local_handle, state->id,
                              delivery->tag.size, delivery->tag.start,
                              0, delivery->local_settled, !delivery->done);
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

  pn_delivery_state_t *state = delivery->context;
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
  if ((int16_t) ssn_state->local_channel >= 0 && !delivery->remote_settled) {
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
      pn_link_t *link = delivery->link;
      if (pn_is_sender(link)) {
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
      pn_delivery_t *tail = state->link->tail;
      if (!tail || !pn_delivery_buffered(tail)) {
        state->delivery_count += state->link_credit;
        state->link_credit = 0;
        snd->credit = 0;
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
    if (endpoint->state & PN_LOCAL_CLOSED && (int32_t) state->local_handle >= 0) {
      if (pn_is_sender(link) && pn_queued(link)) return 0;
      int err = pn_post_frame(transport->disp, ssn_state->local_channel, "DL[Io]", DETACH,
                              state->local_handle, true);
      if (err) return err;
      state->local_handle = -2;
    }

    pn_clear_modified(transport->connection, endpoint);
  }

  return 0;
}

int pn_process_ssn_teardown(pn_transport_t *transport, pn_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    pn_session_t *session = (pn_session_t *) endpoint;
    pn_session_state_t *state = pn_session_get_state(transport, session);
    if (endpoint->state & PN_LOCAL_CLOSED && (int16_t) state->local_channel >= 0)
    {
      if (transport->connection->tpwork_head) return 0;
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
      if (transport->connection->tpwork_head) return 0;
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
    int err = phase(transport, endpoint);
    if (err) return err;
    endpoint = endpoint->transport_next;
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

  if ((err = pn_phase(transport, pn_process_flow_sender))) return err;
  if ((err = pn_phase(transport, pn_process_link_teardown))) return err;
  if ((err = pn_phase(transport, pn_process_ssn_teardown))) return err;
  if ((err = pn_phase(transport, pn_process_conn_teardown))) return err;

  if (transport->connection->tpwork_head) {
    pn_modified(transport->connection, &transport->connection->endpoint);
  }

  return 0;
}

ssize_t pn_output(pn_transport_t *transport, char *bytes, size_t size)
{
  if (!transport->error)
    transport->error = pn_process(transport);

  if (!transport->disp->available && (transport->close_sent || transport->error)) {
    if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
      fprintf(stderr, "    -> EOS\n");
    if (transport->error)
      return transport->error;
    else
      return PN_EOS;
  }

  return pn_dispatcher_output(transport->disp, bytes, size);
}

void pn_trace(pn_transport_t *transport, pn_trace_t trace)
{
  transport->disp->trace = trace;
}

ssize_t pn_send(pn_link_t *sender, const char *bytes, size_t n)
{
  pn_delivery_t *current = pn_current(sender);
  if (!current) return PN_EOS;
  PN_ENSURE(current->bytes, current->capacity, current->size + n);
  memmove(current->bytes + current->size, bytes, n);
  current->size += n;
  pn_add_tpwork(current);
  return n;
}

void pn_drained(pn_link_t *sender)
{
  if (sender) {
    sender->drained = true;
    pn_modified(sender->session->connection, &sender->endpoint);
  }
}

ssize_t pn_recv(pn_link_t *receiver, char *bytes, size_t n)
{
  if (!receiver) return PN_ARG_ERR;

  pn_delivery_t *delivery = receiver->current;
  if (delivery) {
    if (delivery->size) {
      size_t size = n > delivery->size ? delivery->size : n;
      memmove(bytes, delivery->bytes, size);
      memmove(bytes, bytes + size, delivery->size - size);
      delivery->size -= size;
      return size;
    } else {
      return delivery->done ? PN_EOS : 0;
    }
  } else {
    return PN_STATE_ERR;
  }
}

void pn_flow(pn_link_t *receiver, int credit)
{
  if (!receiver) return;
  receiver->credit += credit;
  pn_modified(receiver->session->connection, &receiver->endpoint);
}

void pn_drain(pn_link_t *receiver)
{
  if (receiver && pn_is_receiver(receiver))
    receiver->drain = true;
}

time_t pn_tick(pn_transport_t *engine, time_t now)
{
  return 0;
}

pn_link_t *pn_link(pn_delivery_t *delivery)
{
  if (!delivery) return NULL;
  return delivery->link;
}

int pn_local_disp(pn_delivery_t *delivery)
{
  return delivery->local_state;
}

int pn_remote_disp(pn_delivery_t *delivery)
{
  return delivery->remote_state;
}

bool pn_remote_settled(pn_delivery_t *delivery)
{
  return delivery ? delivery->remote_settled : false;
}

bool pn_updated(pn_delivery_t *delivery)
{
  return delivery ? delivery->updated : false;
}

void pn_clear(pn_delivery_t *delivery)
{
  delivery->updated = false;
  pn_work_update(delivery->link->session->connection, delivery);
}

void pn_disposition(pn_delivery_t *delivery, pn_disposition_t disposition)
{
  if (!delivery) return;
  delivery->local_state = disposition;
  pn_add_tpwork(delivery);
}

bool pn_writable(pn_delivery_t *delivery)
{
  if (!delivery) return false;

  pn_link_t *link = delivery->link;
  return pn_is_sender(link) && pn_is_current(delivery) && pn_credit(link) > 0;
}

bool pn_readable(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_link_t *link = delivery->link;
    return pn_is_receiver(link) && pn_is_current(delivery);
  } else {
    return false;
  }
}

size_t pn_pending(pn_delivery_t *delivery)
{
  return delivery->size;
}
