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
#include "protocol.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "platform.h"
#include "platform_fmt.h"
#include "transport/transport.h"


static void pni_session_bound(pn_session_t *ssn);
static void pni_link_bound(pn_link_t *link);


// endpoints

static pn_connection_t *pni_ep_get_connection(pn_endpoint_t *endpoint)
{
  switch (endpoint->type) {
  case CONNECTION:
    return (pn_connection_t *) endpoint;
  case SESSION:
    return ((pn_session_t *) endpoint)->connection;
  case SENDER:
  case RECEIVER:
    return ((pn_link_t *) endpoint)->session->connection;
  }

  return NULL;
}

static pn_event_type_t endpoint_event(pn_endpoint_type_t type, bool open) {
  switch (type) {
  case CONNECTION:
    return open ? PN_CONNECTION_LOCAL_OPEN : PN_CONNECTION_LOCAL_CLOSE;
  case SESSION:
    return open ? PN_SESSION_LOCAL_OPEN : PN_SESSION_LOCAL_CLOSE;
  case SENDER:
  case RECEIVER:
    return open ? PN_LINK_LOCAL_OPEN : PN_LINK_LOCAL_CLOSE;
  default:
    assert(false);
    return PN_EVENT_NONE;
  }
}

static void pn_endpoint_open(pn_endpoint_t *endpoint)
{
  if (!(endpoint->state & PN_LOCAL_ACTIVE)) {
    PN_SET_LOCAL(endpoint->state, PN_LOCAL_ACTIVE);
    pn_connection_t *conn = pni_ep_get_connection(endpoint);
    pn_collector_put(conn->collector, PN_OBJECT, endpoint,
                     endpoint_event(endpoint->type, true));
    pn_modified(conn, endpoint, true);
  }
}

static void pn_endpoint_close(pn_endpoint_t *endpoint)
{
  if (!(endpoint->state & PN_LOCAL_CLOSED)) {
    PN_SET_LOCAL(endpoint->state, PN_LOCAL_CLOSED);
    pn_connection_t *conn = pni_ep_get_connection(endpoint);
    pn_collector_put(conn->collector, PN_OBJECT, endpoint,
                     endpoint_event(endpoint->type, false));
    pn_modified(conn, endpoint, true);
  }
}

void pn_connection_reset(pn_connection_t *connection)
{
  assert(connection);
  pn_endpoint_t *endpoint = &connection->endpoint;
  endpoint->state = PN_LOCAL_UNINIT | PN_REMOTE_UNINIT;
}

void pn_connection_open(pn_connection_t *connection)
{
  assert(connection);
  pn_endpoint_open(&connection->endpoint);
}

void pn_connection_close(pn_connection_t *connection)
{
  assert(connection);
  pn_endpoint_close(&connection->endpoint);
}

static void pni_endpoint_tini(pn_endpoint_t *endpoint);

void pn_connection_release(pn_connection_t *connection)
{
  assert(!connection->endpoint.freed);
  // free those endpoints that haven't been freed by the application
  LL_REMOVE(connection, endpoint, &connection->endpoint);
  while (connection->endpoint_head) {
    pn_endpoint_t *ep = connection->endpoint_head;
    switch (ep->type) {
    case SESSION:
      // note: this will free all child links:
      pn_session_free((pn_session_t *)ep);
      break;
    case SENDER:
    case RECEIVER:
      pn_link_free((pn_link_t *)ep);
      break;
    default:
      assert(false);
    }
  }
  connection->endpoint.freed = true;
  if (!connection->transport) {
    // no transport available to consume transport work items,
    // so manually clear them:
    pn_ep_incref(&connection->endpoint);
    pn_connection_unbound(connection);
  }
  pn_ep_decref(&connection->endpoint);
}

void pn_connection_free(pn_connection_t *connection) {
  pn_connection_release(connection);
  pn_decref(connection);
}

void pn_connection_bound(pn_connection_t *connection)
{
  pn_collector_put(connection->collector, PN_OBJECT, connection, PN_CONNECTION_BOUND);
  pn_ep_incref(&connection->endpoint);

  size_t nsessions = pn_list_size(connection->sessions);
  for (size_t i = 0; i < nsessions; i++) {
    pni_session_bound((pn_session_t *) pn_list_get(connection->sessions, i));
  }
}

// invoked when transport has been removed:
void pn_connection_unbound(pn_connection_t *connection)
{
  connection->transport = NULL;
  if (connection->endpoint.freed) {
    // connection has been freed prior to unbinding, thus it
    // cannot be re-assigned to a new transport.  Clear the
    // transport work lists to allow the connection to be freed.
    while (connection->transport_head) {
        pn_clear_modified(connection, connection->transport_head);
    }
    while (connection->tpwork_head) {
      pn_clear_tpwork(connection->tpwork_head);
    }
  }
  pn_ep_decref(&connection->endpoint);
}

pn_record_t *pn_connection_attachments(pn_connection_t *connection)
{
  assert(connection);
  return connection->context;
}

void *pn_connection_get_context(pn_connection_t *conn)
{
  // XXX: we should really assert on conn here, but this causes
  // messenger tests to fail
  return conn ? pn_record_get(conn->context, PN_LEGCTX) : NULL;
}

void pn_connection_set_context(pn_connection_t *conn, void *context)
{
  assert(conn);
  pn_record_set(conn->context, PN_LEGCTX, context);
}

pn_transport_t *pn_connection_transport(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport;
}

void pn_condition_init(pn_condition_t *condition)
{
  condition->name = pn_string(NULL);
  condition->description = pn_string(NULL);
  condition->info = pn_data(0);
}

void pn_condition_tini(pn_condition_t *condition)
{
  pn_data_free(condition->info);
  pn_free(condition->description);
  pn_free(condition->name);
}

static void pni_add_session(pn_connection_t *conn, pn_session_t *ssn)
{
  pn_list_add(conn->sessions, ssn);
  ssn->connection = conn;
  pn_incref(conn);  // keep around until finalized
  pn_ep_incref(&conn->endpoint);
}

static void pni_remove_session(pn_connection_t *conn, pn_session_t *ssn)
{
  if (pn_list_remove(conn->sessions, ssn)) {
    pn_ep_decref(&conn->endpoint);
    LL_REMOVE(conn, endpoint, &ssn->endpoint);
  }
}

pn_connection_t *pn_session_connection(pn_session_t *session)
{
  if (!session) return NULL;
  return session->connection;
}

void pn_session_open(pn_session_t *session)
{
  assert(session);
  pn_endpoint_open(&session->endpoint);
}

void pn_session_close(pn_session_t *session)
{
  assert(session);
  pn_endpoint_close(&session->endpoint);
}

void pn_session_free(pn_session_t *session)
{
  assert(!session->endpoint.freed);
  while(pn_list_size(session->links)) {
    pn_link_t *link = (pn_link_t *)pn_list_get(session->links, 0);
    pn_link_free(link);
  }
  pni_remove_session(session->connection, session);
  pn_list_add(session->connection->freed, session);
  session->endpoint.freed = true;
  pn_ep_decref(&session->endpoint);

  // the finalize logic depends on endpoint.freed, so we incref/decref
  // to give it a chance to rerun
  pn_incref(session);
  pn_decref(session);
}

pn_record_t *pn_session_attachments(pn_session_t *session)
{
  assert(session);
  return session->context;
}

void *pn_session_get_context(pn_session_t *session)
{
  return session ? pn_record_get(session->context, PN_LEGCTX) : 0;
}

void pn_session_set_context(pn_session_t *session, void *context)
{
  assert(context);
  pn_record_set(session->context, PN_LEGCTX, context);
}


static void pni_add_link(pn_session_t *ssn, pn_link_t *link)
{
  pn_list_add(ssn->links, link);
  link->session = ssn;
  pn_ep_incref(&ssn->endpoint);
}

static void pni_remove_link(pn_session_t *ssn, pn_link_t *link)
{
  if (pn_list_remove(ssn->links, link)) {
    pn_ep_decref(&ssn->endpoint);
    LL_REMOVE(ssn->connection, endpoint, &link->endpoint);
  }
}

void pn_link_open(pn_link_t *link)
{
  assert(link);
  pn_endpoint_open(&link->endpoint);
}

void pn_link_close(pn_link_t *link)
{
  assert(link);
  pn_endpoint_close(&link->endpoint);
}

void pn_link_detach(pn_link_t *link)
{
  assert(link);
  link->detached = true;
  pn_collector_put(link->session->connection->collector, PN_OBJECT, link, PN_LINK_LOCAL_DETACH);
  pn_modified(link->session->connection, &link->endpoint, true);

}

static void pni_terminus_free(pn_terminus_t *terminus)
{
  pn_free(terminus->address);
  pn_free(terminus->properties);
  pn_free(terminus->capabilities);
  pn_free(terminus->outcomes);
  pn_free(terminus->filter);
}

void pn_link_free(pn_link_t *link)
{
  assert(!link->endpoint.freed);
  pni_remove_link(link->session, link);
  pn_list_add(link->session->freed, link);
  pn_delivery_t *delivery = link->unsettled_head;
  while (delivery) {
    pn_delivery_t *next = delivery->unsettled_next;
    pn_delivery_settle(delivery);
    delivery = next;
  }
  link->endpoint.freed = true;
  pn_ep_decref(&link->endpoint);

  // the finalize logic depends on endpoint.freed (modified above), so
  // we incref/decref to give it a chance to rerun
  pn_incref(link);
  pn_decref(link);
}

void *pn_link_get_context(pn_link_t *link)
{
  assert(link);
  return pn_record_get(link->context, PN_LEGCTX);
}

void pn_link_set_context(pn_link_t *link, void *context)
{
  assert(link);
  pn_record_set(link->context, PN_LEGCTX, context);
}

pn_record_t *pn_link_attachments(pn_link_t *link)
{
  assert(link);
  return link->context;
}

void pn_endpoint_init(pn_endpoint_t *endpoint, int type, pn_connection_t *conn)
{
  endpoint->type = (pn_endpoint_type_t) type;
  endpoint->referenced = true;
  endpoint->state = PN_LOCAL_UNINIT | PN_REMOTE_UNINIT;
  endpoint->error = pn_error();
  pn_condition_init(&endpoint->condition);
  pn_condition_init(&endpoint->remote_condition);
  endpoint->endpoint_next = NULL;
  endpoint->endpoint_prev = NULL;
  endpoint->transport_next = NULL;
  endpoint->transport_prev = NULL;
  endpoint->modified = false;
  endpoint->freed = false;
  endpoint->refcount = 1;
  //fprintf(stderr, "initting 0x%lx\n", (uintptr_t) endpoint);

  LL_ADD(conn, endpoint, endpoint);
}

void pn_ep_incref(pn_endpoint_t *endpoint)
{
  endpoint->refcount++;
}

static pn_event_type_t pn_final_type(pn_endpoint_type_t type) {
  switch (type) {
  case CONNECTION:
    return PN_CONNECTION_FINAL;
  case SESSION:
    return PN_SESSION_FINAL;
  case SENDER:
  case RECEIVER:
    return PN_LINK_FINAL;
  default:
    assert(false);
    return PN_EVENT_NONE;
  }
}

static pn_endpoint_t *pn_ep_parent(pn_endpoint_t *endpoint) {
  switch (endpoint->type) {
  case CONNECTION:
    return NULL;
  case SESSION:
    return &((pn_session_t *) endpoint)->connection->endpoint;
  case SENDER:
  case RECEIVER:
    return &((pn_link_t *) endpoint)->session->endpoint;
  default:
    assert(false);
    return NULL;
  }
}

void pn_ep_decref(pn_endpoint_t *endpoint)
{
  assert(endpoint->refcount > 0);
  endpoint->refcount--;
  if (endpoint->refcount == 0) {
    pn_connection_t *conn = pni_ep_get_connection(endpoint);
    pn_collector_put(conn->collector, PN_OBJECT, endpoint, pn_final_type(endpoint->type));
  }
}

static void pni_endpoint_tini(pn_endpoint_t *endpoint)
{
  pn_error_free(endpoint->error);
  pn_condition_tini(&endpoint->remote_condition);
  pn_condition_tini(&endpoint->condition);
}

static void pni_free_children(pn_list_t *children, pn_list_t *freed)
{
  while (pn_list_size(children) > 0) {
    pn_endpoint_t *endpoint = (pn_endpoint_t *) pn_list_get(children, 0);
    assert(!endpoint->referenced);
    pn_free(endpoint);
  }

  while (pn_list_size(freed) > 0) {
    pn_endpoint_t *endpoint = (pn_endpoint_t *) pn_list_get(freed, 0);
    assert(!endpoint->referenced);
    pn_free(endpoint);
  }

  pn_free(children);
  pn_free(freed);
}

static void pn_connection_finalize(void *object)
{
  pn_connection_t *conn = (pn_connection_t *) object;
  pn_endpoint_t *endpoint = &conn->endpoint;

  if (conn->transport) {
    assert(!conn->transport->referenced);
    pn_free(conn->transport);
  }

  // freeing the transport could post events
  if (pn_refcount(conn) > 0) {
    return;
  }

  pni_free_children(conn->sessions, conn->freed);
  pn_free(conn->context);
  pn_decref(conn->collector);

  pn_free(conn->container);
  pn_free(conn->hostname);
  pn_free(conn->auth_user);
  pn_free(conn->auth_password);
  pn_free(conn->offered_capabilities);
  pn_free(conn->desired_capabilities);
  pn_free(conn->properties);
  pni_endpoint_tini(endpoint);
  pn_free(conn->delivery_pool);
}

#define pn_connection_initialize NULL
#define pn_connection_hashcode NULL
#define pn_connection_compare NULL
#define pn_connection_inspect NULL

pn_connection_t *pn_connection(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_connection);
  pn_connection_t *conn = (pn_connection_t *) pn_class_new(&clazz, sizeof(pn_connection_t));
  if (!conn) return NULL;

  conn->endpoint_head = NULL;
  conn->endpoint_tail = NULL;
  pn_endpoint_init(&conn->endpoint, CONNECTION, conn);
  conn->transport_head = NULL;
  conn->transport_tail = NULL;
  conn->sessions = pn_list(PN_WEAKREF, 0);
  conn->freed = pn_list(PN_WEAKREF, 0);
  conn->transport = NULL;
  conn->work_head = NULL;
  conn->work_tail = NULL;
  conn->tpwork_head = NULL;
  conn->tpwork_tail = NULL;
  conn->container = pn_string(NULL);
  conn->hostname = pn_string(NULL);
  conn->auth_user = pn_string(NULL);
  conn->auth_password = pn_string(NULL);
  conn->offered_capabilities = pn_data(0);
  conn->desired_capabilities = pn_data(0);
  conn->properties = pn_data(0);
  conn->collector = NULL;
  conn->context = pn_record();
  conn->delivery_pool = pn_list(PN_OBJECT, 0);

  return conn;
}

static const pn_event_type_t endpoint_init_event_map[] = {
  PN_CONNECTION_INIT,  /* CONNECTION */
  PN_SESSION_INIT,     /* SESSION */
  PN_LINK_INIT,        /* SENDER */
  PN_LINK_INIT};       /* RECEIVER */

void pn_connection_collect(pn_connection_t *connection, pn_collector_t *collector)
{
  pn_decref(connection->collector);
  connection->collector = collector;
  pn_incref(connection->collector);
  pn_endpoint_t *endpoint = connection->endpoint_head;
  while (endpoint) {
    pn_collector_put(connection->collector, PN_OBJECT, endpoint, endpoint_init_event_map[endpoint->type]);
    endpoint = endpoint->endpoint_next;
  }
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
  assert(connection);
  return pn_string_get(connection->container);
}

void pn_connection_set_container(pn_connection_t *connection, const char *container)
{
  assert(connection);
  pn_string_set(connection->container, container);
}

const char *pn_connection_get_hostname(pn_connection_t *connection)
{
  assert(connection);
  return pn_string_get(connection->hostname);
}

void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname)
{
  assert(connection);
  pn_string_set(connection->hostname, hostname);
}

const char *pn_connection_get_user(pn_connection_t *connection)
{
    assert(connection);
    return pn_string_get(connection->auth_user);
}

void pn_connection_set_user(pn_connection_t *connection, const char *user)
{
    assert(connection);
    pn_string_set(connection->auth_user, user);
}

void pn_connection_set_password(pn_connection_t *connection, const char *password)
{
    assert(connection);
    pn_string_set(connection->auth_password, password);
}

pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection)
{
  assert(connection);
  return connection->offered_capabilities;
}

pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection)
{
  assert(connection);
  return connection->desired_capabilities;
}

pn_data_t *pn_connection_properties(pn_connection_t *connection)
{
  assert(connection);
  return connection->properties;
}

pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport ? connection->transport->remote_offered_capabilities : NULL;
}

pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport ? connection->transport->remote_desired_capabilities : NULL;
}

pn_data_t *pn_connection_remote_properties(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport ? connection->transport->remote_properties : NULL;
}

const char *pn_connection_remote_container(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport ? connection->transport->remote_container : NULL;
}

const char *pn_connection_remote_hostname(pn_connection_t *connection)
{
  assert(connection);
  return connection->transport ? connection->transport->remote_hostname : NULL;
}

pn_delivery_t *pn_work_head(pn_connection_t *connection)
{
  assert(connection);
  return connection->work_head;
}

pn_delivery_t *pn_work_next(pn_delivery_t *delivery)
{
  assert(delivery);

  if (delivery->work)
    return delivery->work_next;
  else
    return pn_work_head(delivery->link->session->connection);
}

static void pni_add_work(pn_connection_t *connection, pn_delivery_t *delivery)
{
  if (!delivery->work)
  {
    assert(!delivery->local.settled);   // never allow settled deliveries
    LL_ADD(connection, work, delivery);
    delivery->work = true;
  }
}

static void pni_clear_work(pn_connection_t *connection, pn_delivery_t *delivery)
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
  if (delivery->updated && !delivery->local.settled) {
    pni_add_work(connection, delivery);
  } else if (delivery == current) {
    if (link->endpoint.type == SENDER) {
      if (pn_link_credit(link) > 0) {
        pni_add_work(connection, delivery);
      } else {
        pni_clear_work(connection, delivery);
      }
    } else {
      pni_add_work(connection, delivery);
    }
  } else {
    pni_clear_work(connection, delivery);
  }
}

static void pni_add_tpwork(pn_delivery_t *delivery)
{
  pn_connection_t *connection = delivery->link->session->connection;
  if (!delivery->tpwork)
  {
    LL_ADD(connection, tpwork, delivery);
    delivery->tpwork = true;
  }
  pn_modified(connection, &connection->endpoint, true);
}

void pn_clear_tpwork(pn_delivery_t *delivery)
{
  pn_connection_t *connection = delivery->link->session->connection;
  if (delivery->tpwork)
  {
    LL_REMOVE(connection, tpwork, delivery);
    delivery->tpwork = false;
    if (pn_refcount(delivery) > 0) {
      pn_incref(delivery);
      pn_decref(delivery);
    }
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

void pn_modified(pn_connection_t *connection, pn_endpoint_t *endpoint, bool emit)
{
  if (!endpoint->modified) {
    LL_ADD(connection, transport, endpoint);
    endpoint->modified = true;
  }

  if (emit && connection->transport) {
    pn_collector_put(connection->collector, PN_OBJECT, connection->transport,
                     PN_TRANSPORT);
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

static bool pni_matches(pn_endpoint_t *endpoint, pn_endpoint_type_t type, pn_state_t state)
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
    if (pni_matches(endpoint, type, state))
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
    if (pni_matches(endpoint, SENDER, state) || pni_matches(endpoint, RECEIVER, state))
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
    if (pni_matches(endpoint, SENDER, state) || pni_matches(endpoint, RECEIVER, state))
      return (pn_link_t *) endpoint;
    endpoint = endpoint->endpoint_next;
  }

  return NULL;
}

static void pn_session_incref(void *object)
{
  pn_session_t *session = (pn_session_t *) object;
  if (!session->endpoint.referenced) {
    session->endpoint.referenced = true;
    pn_incref(session->connection);
  } else {
    pn_object_incref(object);
  }
}

static bool pn_ep_bound(pn_endpoint_t *endpoint)
{
  pn_connection_t *conn = pni_ep_get_connection(endpoint);
  pn_session_t *ssn;
  pn_link_t *lnk;

  if (!conn->transport) return false;
  if (endpoint->modified) return true;

  switch (endpoint->type) {
  case CONNECTION:
    return ((pn_connection_t *)endpoint)->transport;
  case SESSION:
    ssn = (pn_session_t *) endpoint;
    return (((int16_t) ssn->state.local_channel) >= 0 || ((int16_t) ssn->state.remote_channel) >= 0);
  case SENDER:
  case RECEIVER:
    lnk = (pn_link_t *) endpoint;
    return ((int32_t) lnk->state.local_handle) >= 0 || ((int32_t) lnk->state.remote_handle) >= 0;
  default:
    assert(false);
    return false;
  }
}

static bool pni_connection_live(pn_connection_t *conn) {
  return pn_refcount(conn) > 1;
}

static bool pni_session_live(pn_session_t *ssn) {
  return pni_connection_live(ssn->connection) || pn_refcount(ssn) > 1;
}

static bool pni_link_live(pn_link_t *link) {
  return pni_session_live(link->session) || pn_refcount(link) > 1;
}

static bool pni_endpoint_live(pn_endpoint_t *endpoint) {
  switch (endpoint->type) {
  case CONNECTION:
    return pni_connection_live((pn_connection_t *)endpoint);
  case SESSION:
    return pni_session_live((pn_session_t *) endpoint);
  case SENDER:
  case RECEIVER:
    return pni_link_live((pn_link_t *) endpoint);
  default:
    assert(false);
    return false;
  }
}

static bool pni_preserve_child(pn_endpoint_t *endpoint)
{
  pn_connection_t *conn = pni_ep_get_connection(endpoint);
  pn_endpoint_t *parent = pn_ep_parent(endpoint);
  if (pni_endpoint_live(parent) && (!endpoint->freed || (pn_ep_bound(endpoint)))
      && endpoint->referenced) {
    pn_object_incref(endpoint);
    endpoint->referenced = false;
    pn_decref(parent);
    return true;
  } else {
    LL_REMOVE(conn, transport, endpoint);
    return false;
  }
}

static void pn_session_finalize(void *object)
{
  pn_session_t *session = (pn_session_t *) object;
  pn_endpoint_t *endpoint = &session->endpoint;

  if (pni_preserve_child(endpoint)) {
    return;
  }

  pn_free(session->context);
  pni_free_children(session->links, session->freed);
  pni_endpoint_tini(endpoint);
  pn_delivery_map_free(&session->state.incoming);
  pn_delivery_map_free(&session->state.outgoing);
  pn_free(session->state.local_handles);
  pn_free(session->state.remote_handles);
  pni_remove_session(session->connection, session);
  pn_list_remove(session->connection->freed, session);

  if (session->connection->transport) {
    pn_transport_t *transport = session->connection->transport;
    pn_hash_del(transport->local_channels, session->state.local_channel);
    pn_hash_del(transport->remote_channels, session->state.remote_channel);
  }

  if (endpoint->referenced) {
    pn_decref(session->connection);
  }
}

#define pn_session_new pn_object_new
#define pn_session_refcount pn_object_refcount
#define pn_session_decref pn_object_decref
#define pn_session_reify pn_object_reify
#define pn_session_initialize NULL
#define pn_session_hashcode NULL
#define pn_session_compare NULL
#define pn_session_inspect NULL

pn_session_t *pn_session(pn_connection_t *conn)
{
  assert(conn);


  pn_transport_t * transport = pn_connection_transport(conn);

  if(transport) {
    // channel_max is an index, not a count.  
    if(pn_hash_size(transport->local_channels) > (size_t)transport->channel_max) {
      pn_transport_logf(transport, 
                        "pn_session: too many sessions: %d  channel_max is %d",
                        pn_hash_size(transport->local_channels),
                        transport->channel_max);
      return (pn_session_t *) 0;
    }
  }

#define pn_session_free pn_object_free
  static const pn_class_t clazz = PN_METACLASS(pn_session);
#undef pn_session_free
  pn_session_t *ssn = (pn_session_t *) pn_class_new(&clazz, sizeof(pn_session_t));
  if (!ssn) return NULL;
  pn_endpoint_init(&ssn->endpoint, SESSION, conn);
  pni_add_session(conn, ssn);
  ssn->links = pn_list(PN_WEAKREF, 0);
  ssn->freed = pn_list(PN_WEAKREF, 0);
  ssn->context = pn_record();
  ssn->incoming_capacity = 1024*1024;
  ssn->incoming_bytes = 0;
  ssn->outgoing_bytes = 0;
  ssn->incoming_deliveries = 0;
  ssn->outgoing_deliveries = 0;
  ssn->outgoing_window = 2147483647;

  // begin transport state
  memset(&ssn->state, 0, sizeof(ssn->state));
  ssn->state.local_channel = (uint16_t)-1;
  ssn->state.remote_channel = (uint16_t)-1;
  pn_delivery_map_init(&ssn->state.incoming, 0);
  pn_delivery_map_init(&ssn->state.outgoing, 0);
  ssn->state.local_handles = pn_hash(PN_WEAKREF, 0, 0.75);
  ssn->state.remote_handles = pn_hash(PN_WEAKREF, 0, 0.75);
  // end transport state

  pn_collector_put(conn->collector, PN_OBJECT, ssn, PN_SESSION_INIT);
  if (conn->transport) {
    pni_session_bound(ssn);
  }
  pn_decref(ssn);
  return ssn;
}

static void pni_session_bound(pn_session_t *ssn)
{
  assert(ssn);
  size_t nlinks = pn_list_size(ssn->links);
  for (size_t i = 0; i < nlinks; i++) {
    pni_link_bound((pn_link_t *) pn_list_get(ssn->links, i));
  }
}

void pn_session_unbound(pn_session_t* ssn)
{
  assert(ssn);
  ssn->state.local_channel = (uint16_t)-1;
  ssn->state.remote_channel = (uint16_t)-1;
  ssn->incoming_bytes = 0;
  ssn->outgoing_bytes = 0;
  ssn->incoming_deliveries = 0;
  ssn->outgoing_deliveries = 0;
}

size_t pn_session_get_incoming_capacity(pn_session_t *ssn)
{
  assert(ssn);
  return ssn->incoming_capacity;
}

void pn_session_set_incoming_capacity(pn_session_t *ssn, size_t capacity)
{
  assert(ssn);
  // XXX: should this trigger a flow?
  ssn->incoming_capacity = capacity;
}

size_t pn_session_get_outgoing_window(pn_session_t *ssn)
{
  assert(ssn);
  return ssn->outgoing_window;
}

void pn_session_set_outgoing_window(pn_session_t *ssn, size_t window)
{
  assert(ssn);
  ssn->outgoing_window = window;
}

size_t pn_session_outgoing_bytes(pn_session_t *ssn)
{
  assert(ssn);
  return ssn->outgoing_bytes;
}

size_t pn_session_incoming_bytes(pn_session_t *ssn)
{
  assert(ssn);
  return ssn->incoming_bytes;
}

pn_state_t pn_session_state(pn_session_t *session)
{
  return session->endpoint.state;
}

pn_error_t *pn_session_error(pn_session_t *session)
{
  return session->endpoint.error;
}

static void pni_terminus_init(pn_terminus_t *terminus, pn_terminus_type_t type)
{
  terminus->type = type;
  terminus->address = pn_string(NULL);
  terminus->durability = PN_NONDURABLE;
  terminus->expiry_policy = PN_EXPIRE_WITH_SESSION;
  terminus->timeout = 0;
  terminus->dynamic = false;
  terminus->distribution_mode = PN_DIST_MODE_UNSPECIFIED;
  terminus->properties = pn_data(0);
  terminus->capabilities = pn_data(0);
  terminus->outcomes = pn_data(0);
  terminus->filter = pn_data(0);
}

static void pn_link_incref(void *object)
{
  pn_link_t *link = (pn_link_t *) object;
  if (!link->endpoint.referenced) {
    link->endpoint.referenced = true;
    pn_incref(link->session);
  } else {
    pn_object_incref(object);
  }
}

static void pn_link_finalize(void *object)
{
  pn_link_t *link = (pn_link_t *) object;
  pn_endpoint_t *endpoint = &link->endpoint;

  if (pni_preserve_child(endpoint)) {
    return;
  }

  while (link->unsettled_head) {
    assert(!link->unsettled_head->referenced);
    pn_free(link->unsettled_head);
  }

  pn_free(link->context);
  pni_terminus_free(&link->source);
  pni_terminus_free(&link->target);
  pni_terminus_free(&link->remote_source);
  pni_terminus_free(&link->remote_target);
  pn_free(link->name);
  pni_endpoint_tini(endpoint);
  pni_remove_link(link->session, link);
  pn_hash_del(link->session->state.local_handles, link->state.local_handle);
  pn_hash_del(link->session->state.remote_handles, link->state.remote_handle);
  pn_list_remove(link->session->freed, link);
  if (endpoint->referenced) {
    pn_decref(link->session);
  }
}

#define pn_link_refcount pn_object_refcount
#define pn_link_decref pn_object_decref
#define pn_link_reify pn_object_reify
#define pn_link_initialize NULL
#define pn_link_hashcode NULL
#define pn_link_compare NULL
#define pn_link_inspect NULL

pn_link_t *pn_link_new(int type, pn_session_t *session, const char *name)
{
#define pn_link_new pn_object_new
#define pn_link_free pn_object_free
  static const pn_class_t clazz = PN_METACLASS(pn_link);
#undef pn_link_new
#undef pn_link_free
  pn_link_t *link = (pn_link_t *) pn_class_new(&clazz, sizeof(pn_link_t));

  pn_endpoint_init(&link->endpoint, type, session->connection);
  pni_add_link(session, link);
  pn_incref(session);  // keep session until link finalized
  link->name = pn_string(name);
  pni_terminus_init(&link->source, PN_SOURCE);
  pni_terminus_init(&link->target, PN_TARGET);
  pni_terminus_init(&link->remote_source, PN_UNSPECIFIED);
  pni_terminus_init(&link->remote_target, PN_UNSPECIFIED);
  link->unsettled_head = link->unsettled_tail = link->current = NULL;
  link->unsettled_count = 0;
  link->available = 0;
  link->credit = 0;
  link->queued = 0;
  link->drain = false;
  link->drain_flag_mode = true;
  link->drained = 0;
  link->context = pn_record();
  link->snd_settle_mode = PN_SND_MIXED;
  link->rcv_settle_mode = PN_RCV_FIRST;
  link->remote_snd_settle_mode = PN_SND_MIXED;
  link->remote_rcv_settle_mode = PN_RCV_FIRST;
  link->detached = false;

  // begin transport state
  link->state.local_handle = -1;
  link->state.remote_handle = -1;
  link->state.delivery_count = 0;
  link->state.link_credit = 0;
  // end transport state

  pn_collector_put(session->connection->collector, PN_OBJECT, link, PN_LINK_INIT);
  if (session->connection->transport) {
    pni_link_bound(link);
  }
  pn_decref(link);
  return link;
}

static void pni_link_bound(pn_link_t *link)
{
}

void pn_link_unbound(pn_link_t* link)
{
  assert(link);
  link->state.local_handle = -1;
  link->state.remote_handle = -1;
  link->state.delivery_count = 0;
  link->state.link_credit = 0;
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
  assert(terminus);
  return pn_string_get(terminus->address);
}

int pn_terminus_set_address(pn_terminus_t *terminus, const char *address)
{
  assert(terminus);
  return pn_string_set(terminus->address, address);
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

pn_distribution_mode_t pn_terminus_get_distribution_mode(const pn_terminus_t *terminus)
{
  return terminus ? terminus->distribution_mode : PN_DIST_MODE_UNSPECIFIED;
}

int pn_terminus_set_distribution_mode(pn_terminus_t *terminus, pn_distribution_mode_t m)
{
  if (!terminus) return PN_ARG_ERR;
  terminus->distribution_mode = m;
  return 0;
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
  terminus->distribution_mode = src->distribution_mode;
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

pn_link_t *pn_sender(pn_session_t *session, const char *name)
{
  return pn_link_new(SENDER, session, name);
}

pn_link_t *pn_receiver(pn_session_t *session, const char *name)
{
  return pn_link_new(RECEIVER, session, name);
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
  assert(link);
  return pn_string_get(link->name);
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
  assert(link);
  return link->session;
}

static void pn_disposition_finalize(pn_disposition_t *ds)
{
  pn_free(ds->data);
  pn_free(ds->annotations);
  pn_condition_tini(&ds->condition);
}

static void pn_delivery_incref(void *object)
{
  pn_delivery_t *delivery = (pn_delivery_t *) object;
  if (delivery->link && !delivery->referenced) {
    delivery->referenced = true;
    pn_incref(delivery->link);
  } else {
    pn_object_incref(object);
  }
}

static bool pni_preserve_delivery(pn_delivery_t *delivery)
{
  pn_connection_t *conn = delivery->link->session->connection;
  return !delivery->local.settled || (conn->transport && (delivery->state.init || delivery->tpwork));
}

static void pn_delivery_finalize(void *object)
{
  pn_delivery_t *delivery = (pn_delivery_t *) object;
  pn_link_t *link = delivery->link;
  //  assert(!delivery->state.init);

  bool pooled = false;
  bool referenced = true;
  if (link) {
    if (pni_link_live(link) && pni_preserve_delivery(delivery) && delivery->referenced) {
      delivery->referenced = false;
      pn_object_incref(delivery);
      pn_decref(link);
      return;
    }
    referenced = delivery->referenced;

    pn_clear_tpwork(delivery);
    LL_REMOVE(link, unsettled, delivery);
    pn_delivery_map_del(pn_link_is_sender(link)
                        ? &link->session->state.outgoing
                        : &link->session->state.incoming,
                        delivery);
    pn_buffer_clear(delivery->tag);
    pn_buffer_clear(delivery->bytes);
    pn_record_clear(delivery->context);
    delivery->settled = true;
    pn_connection_t *conn = link->session->connection;
    assert(pn_refcount(delivery) == 0);
    if (pni_connection_live(conn)) {
      pn_list_t *pool = link->session->connection->delivery_pool;
      delivery->link = NULL;
      pn_list_add(pool, delivery);
      pooled = true;
      assert(pn_refcount(delivery) == 1);
    }
  }

  if (!pooled) {
    pn_free(delivery->context);
    pn_buffer_free(delivery->tag);
    pn_buffer_free(delivery->bytes);
    pn_disposition_finalize(&delivery->local);
    pn_disposition_finalize(&delivery->remote);
  }

  if (referenced) {
    pn_decref(link);
  }
}

static void pn_disposition_init(pn_disposition_t *ds)
{
  ds->data = pn_data(0);
  ds->annotations = pn_data(0);
  pn_condition_init(&ds->condition);
}

static void pn_disposition_clear(pn_disposition_t *ds)
{
  ds->type = 0;
  ds->section_number = 0;
  ds->section_offset = 0;
  ds->failed = false;
  ds->undeliverable = false;
  ds->settled = false;
  pn_data_clear(ds->data);
  pn_data_clear(ds->annotations);
  pn_condition_clear(&ds->condition);
}

#define pn_delivery_new pn_object_new
#define pn_delivery_refcount pn_object_refcount
#define pn_delivery_decref pn_object_decref
#define pn_delivery_free pn_object_free
#define pn_delivery_reify pn_object_reify
#define pn_delivery_initialize NULL
#define pn_delivery_hashcode NULL
#define pn_delivery_compare NULL
#define pn_delivery_inspect NULL

pn_delivery_tag_t pn_dtag(const char *bytes, size_t size) {
  pn_delivery_tag_t dtag = {size, bytes};
  return dtag;
}

pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag)
{
  assert(link);
  pn_list_t *pool = link->session->connection->delivery_pool;
  pn_delivery_t *delivery = (pn_delivery_t *) pn_list_pop(pool);
  if (!delivery) {
    static const pn_class_t clazz = PN_METACLASS(pn_delivery);
    delivery = (pn_delivery_t *) pn_class_new(&clazz, sizeof(pn_delivery_t));
    if (!delivery) return NULL;
    delivery->tag = pn_buffer(16);
    delivery->bytes = pn_buffer(64);
    pn_disposition_init(&delivery->local);
    pn_disposition_init(&delivery->remote);
    delivery->context = pn_record();
  } else {
    assert(!delivery->state.init);
  }
  delivery->link = link;
  pn_incref(delivery->link);  // keep link until finalized
  pn_buffer_clear(delivery->tag);
  pn_buffer_append(delivery->tag, tag.start, tag.size);
  pn_disposition_clear(&delivery->local);
  pn_disposition_clear(&delivery->remote);
  delivery->updated = false;
  delivery->settled = false;
  LL_ADD(link, unsettled, delivery);
  delivery->referenced = true;
  delivery->work_next = NULL;
  delivery->work_prev = NULL;
  delivery->work = false;
  delivery->tpwork_next = NULL;
  delivery->tpwork_prev = NULL;
  delivery->tpwork = false;
  pn_buffer_clear(delivery->bytes);
  delivery->done = false;
  pn_record_clear(delivery->context);

  // begin delivery state
  delivery->state.init = false;
  delivery->state.sent = false;
  // end delivery state

  if (!link->current)
    link->current = delivery;

  link->unsettled_count++;

  pn_work_update(link->session->connection, delivery);

  // XXX: could just remove incref above
  pn_decref(delivery);

  return delivery;
}

bool pn_delivery_buffered(pn_delivery_t *delivery)
{
  assert(delivery);
  if (delivery->settled) return false;
  if (pn_link_is_sender(delivery->link)) {
    pn_delivery_state_t *state = &delivery->state;
    if (state->sent) {
      return false;
    } else {
      return delivery->done || (pn_buffer_size(delivery->bytes) > 0);
    }
  } else {
    return false;
  }
}

int pn_link_unsettled(pn_link_t *link)
{
  return link->unsettled_count;
}

pn_delivery_t *pn_unsettled_head(pn_link_t *link)
{
  pn_delivery_t *d = link->unsettled_head;
  while (d && d->local.settled) {
    d = d->unsettled_next;
  }
  return d;
}

pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery)
{
  pn_delivery_t *d = delivery->unsettled_next;
  while (d && d->local.settled) {
    d = d->unsettled_next;
  }
  return d;
}

bool pn_delivery_current(pn_delivery_t *delivery)
{
  pn_link_t *link = delivery->link;
  return pn_link_current(link) == delivery;
}

void pn_delivery_dump(pn_delivery_t *d)
{
  char tag[1024];
  pn_bytes_t bytes = pn_buffer_bytes(d->tag);
  pn_quote_data(tag, 1024, bytes.start, bytes.size);
  printf("{tag=%s, local.type=%" PRIu64 ", remote.type=%" PRIu64 ", local.settled=%u, "
         "remote.settled=%u, updated=%u, current=%u, writable=%u, readable=%u, "
         "work=%u}",
         tag, d->local.type, d->remote.type, d->local.settled,
         d->remote.settled, d->updated, pn_delivery_current(d),
         pn_delivery_writable(d), pn_delivery_readable(d), d->work);
}

void *pn_delivery_get_context(pn_delivery_t *delivery)
{
  assert(delivery);
  return pn_record_get(delivery->context, PN_LEGCTX);
}

void pn_delivery_set_context(pn_delivery_t *delivery, void *context)
{
  assert(delivery);
  pn_record_set(delivery->context, PN_LEGCTX, context);
}

pn_record_t *pn_delivery_attachments(pn_delivery_t *delivery)
{
  assert(delivery);
  return delivery->context;
}

uint64_t pn_disposition_type(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->type;
}

pn_data_t *pn_disposition_data(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->data;
}

uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->section_number;
}

void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number)
{
  assert(disposition);
  disposition->section_number = section_number;
}

uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->section_offset;
}

void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset)
{
  assert(disposition);
  disposition->section_offset = section_offset;
}

bool pn_disposition_is_failed(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->failed;
}

void pn_disposition_set_failed(pn_disposition_t *disposition, bool failed)
{
  assert(disposition);
  disposition->failed = failed;
}

bool pn_disposition_is_undeliverable(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->undeliverable;
}

void pn_disposition_set_undeliverable(pn_disposition_t *disposition, bool undeliverable)
{
  assert(disposition);
  disposition->undeliverable = undeliverable;
}

pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition)
{
  assert(disposition);
  return disposition->annotations;
}

pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition)
{
  assert(disposition);
  return &disposition->condition;
}

pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_bytes_t tag = pn_buffer_bytes(delivery->tag);
    return pn_dtag(tag.start, tag.size);
  } else {
    return pn_dtag(0, 0);
  }
}

pn_delivery_t *pn_link_current(pn_link_t *link)
{
  if (!link) return NULL;
  return link->current;
}

static void pni_advance_sender(pn_link_t *link)
{
  link->current->done = true;
  link->queued++;
  link->credit--;
  link->session->outgoing_deliveries++;
  pni_add_tpwork(link->current);
  link->current = link->current->unsettled_next;
}

static void pni_advance_receiver(pn_link_t *link)
{
  link->credit--;
  link->queued--;
  link->session->incoming_deliveries--;

  pn_delivery_t *current = link->current;
  link->session->incoming_bytes -= pn_buffer_size(current->bytes);
  pn_buffer_clear(current->bytes);

  if (!link->session->state.incoming_window) {
    pni_add_tpwork(current);
  }

  link->current = link->current->unsettled_next;
}

bool pn_link_advance(pn_link_t *link)
{
  if (link && link->current) {
    pn_delivery_t *prev = link->current;
    if (link->endpoint.type == SENDER) {
      pni_advance_sender(link);
    } else {
      pni_advance_receiver(link);
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

int pn_link_remote_credit(pn_link_t *link)
{
  assert(link);
  return link->credit - link->queued;
}

bool pn_link_get_drain(pn_link_t *link)
{
  assert(link);
  return link->drain;
}

pn_snd_settle_mode_t pn_link_snd_settle_mode(pn_link_t *link)
{
  return link ? (pn_snd_settle_mode_t)link->snd_settle_mode
      : PN_SND_MIXED;
}

pn_rcv_settle_mode_t pn_link_rcv_settle_mode(pn_link_t *link)
{
  return link ? (pn_rcv_settle_mode_t)link->rcv_settle_mode
      : PN_RCV_FIRST;
}

pn_snd_settle_mode_t pn_link_remote_snd_settle_mode(pn_link_t *link)
{
  return link ? (pn_snd_settle_mode_t)link->remote_snd_settle_mode
      : PN_SND_MIXED;
}

pn_rcv_settle_mode_t pn_link_remote_rcv_settle_mode(pn_link_t *link)
{
  return link ? (pn_rcv_settle_mode_t)link->remote_rcv_settle_mode
      : PN_RCV_FIRST;
}
void pn_link_set_snd_settle_mode(pn_link_t *link, pn_snd_settle_mode_t mode)
{
  if (link)
    link->snd_settle_mode = (uint8_t)mode;
}
void pn_link_set_rcv_settle_mode(pn_link_t *link, pn_rcv_settle_mode_t mode)
{
  if (link)
    link->rcv_settle_mode = (uint8_t)mode;
}

void pn_delivery_settle(pn_delivery_t *delivery)
{
  assert(delivery);
  if (!delivery->local.settled) {
    pn_link_t *link = delivery->link;
    if (pn_delivery_current(delivery)) {
      pn_link_advance(link);
    }

    link->unsettled_count--;
    delivery->local.settled = true;
    pni_add_tpwork(delivery);
    pn_work_update(delivery->link->session->connection, delivery);
    pn_incref(delivery);
    pn_decref(delivery);
  }
}

void pn_link_offered(pn_link_t *sender, int credit)
{
  sender->available = credit;
}

ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n)
{
  pn_delivery_t *current = pn_link_current(sender);
  if (!current) return PN_EOS;
  if (!bytes || !n) return 0;
  pn_buffer_append(current->bytes, bytes, n);
  sender->session->outgoing_bytes += n;
  pni_add_tpwork(current);
  return n;
}

int pn_link_drained(pn_link_t *link)
{
  assert(link);
  int drained = 0;

  if (pn_link_is_sender(link)) {
    if (link->drain && link->credit > 0) {
      link->drained = link->credit;
      link->credit = 0;
      pn_modified(link->session->connection, &link->endpoint, true);
      drained = link->drained;
    }
  } else {
    drained = link->drained;
    link->drained = 0;
  }

  return drained;
}

ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n)
{
  if (!receiver) return PN_ARG_ERR;

  pn_delivery_t *delivery = receiver->current;
  if (delivery) {
    size_t size = pn_buffer_get(delivery->bytes, 0, n, bytes);
    pn_buffer_trim(delivery->bytes, size, 0);
    if (size) {
      receiver->session->incoming_bytes -= size;
      if (!receiver->session->state.incoming_window) {
        pni_add_tpwork(delivery);
      }
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
  assert(receiver);
  assert(pn_link_is_receiver(receiver));
  receiver->credit += credit;
  pn_modified(receiver->session->connection, &receiver->endpoint, true);
  if (!receiver->drain_flag_mode) {
    pn_link_set_drain(receiver, false);
    receiver->drain_flag_mode = false;
  }
}

void pn_link_drain(pn_link_t *receiver, int credit)
{
  assert(receiver);
  assert(pn_link_is_receiver(receiver));
  pn_link_set_drain(receiver, true);
  pn_link_flow(receiver, credit);
  receiver->drain_flag_mode = false;
}

void pn_link_set_drain(pn_link_t *receiver, bool drain)
{
  assert(receiver);
  assert(pn_link_is_receiver(receiver));
  receiver->drain = drain;
  pn_modified(receiver->session->connection, &receiver->endpoint, true);
  receiver->drain_flag_mode = true;
}

bool pn_link_draining(pn_link_t *receiver)
{
  assert(receiver);
  assert(pn_link_is_receiver(receiver));
  return receiver->drain && (pn_link_credit(receiver) > pn_link_queued(receiver));
}

pn_link_t *pn_delivery_link(pn_delivery_t *delivery)
{
  assert(delivery);
  return delivery->link;
}

pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery)
{
  assert(delivery);
  return &delivery->local;
}

uint64_t pn_delivery_local_state(pn_delivery_t *delivery)
{
  assert(delivery);
  return delivery->local.type;
}

pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery)
{
  assert(delivery);
  return &delivery->remote;
}

uint64_t pn_delivery_remote_state(pn_delivery_t *delivery)
{
  assert(delivery);
  return delivery->remote.type;
}

bool pn_delivery_settled(pn_delivery_t *delivery)
{
  return delivery ? delivery->remote.settled : false;
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

void pn_delivery_update(pn_delivery_t *delivery, uint64_t state)
{
  if (!delivery) return;
  delivery->local.type = state;
  pni_add_tpwork(delivery);
}

bool pn_delivery_writable(pn_delivery_t *delivery)
{
  if (!delivery) return false;

  pn_link_t *link = delivery->link;
  return pn_link_is_sender(link) && pn_delivery_current(delivery) && pn_link_credit(link) > 0;
}

bool pn_delivery_readable(pn_delivery_t *delivery)
{
  if (delivery) {
    pn_link_t *link = delivery->link;
    return pn_link_is_receiver(link) && pn_delivery_current(delivery);
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

pn_condition_t *pn_connection_condition(pn_connection_t *connection)
{
  assert(connection);
  return &connection->endpoint.condition;
}

pn_condition_t *pn_connection_remote_condition(pn_connection_t *connection)
{
  assert(connection);
  pn_transport_t *transport = connection->transport;
  return transport ? &transport->remote_condition : NULL;
}

pn_condition_t *pn_session_condition(pn_session_t *session)
{
  assert(session);
  return &session->endpoint.condition;
}

pn_condition_t *pn_session_remote_condition(pn_session_t *session)
{
  assert(session);
  return &session->endpoint.remote_condition;
}

pn_condition_t *pn_link_condition(pn_link_t *link)
{
  assert(link);
  return &link->endpoint.condition;
}

pn_condition_t *pn_link_remote_condition(pn_link_t *link)
{
  assert(link);
  return &link->endpoint.remote_condition;
}

bool pn_condition_is_set(pn_condition_t *condition)
{
  return condition && pn_string_get(condition->name);
}

void pn_condition_clear(pn_condition_t *condition)
{
  assert(condition);
  pn_string_clear(condition->name);
  pn_string_clear(condition->description);
  pn_data_clear(condition->info);
}

const char *pn_condition_get_name(pn_condition_t *condition)
{
  assert(condition);
  return pn_string_get(condition->name);
}

int pn_condition_set_name(pn_condition_t *condition, const char *name)
{
  assert(condition);
  return pn_string_set(condition->name, name);
}

const char *pn_condition_get_description(pn_condition_t *condition)
{
  assert(condition);
  return pn_string_get(condition->description);
}

int pn_condition_set_description(pn_condition_t *condition, const char *description)
{
  assert(condition);
  return pn_string_set(condition->description, description);
}

pn_data_t *pn_condition_info(pn_condition_t *condition)
{
  assert(condition);
  return condition->info;
}

bool pn_condition_is_redirect(pn_condition_t *condition)
{
  const char *name = pn_condition_get_name(condition);
  return name && (!strcmp(name, "amqp:connection:redirect") ||
                  !strcmp(name, "amqp:link:redirect"));
}

const char *pn_condition_redirect_host(pn_condition_t *condition)
{
  pn_data_t *data = pn_condition_info(condition);
  pn_data_rewind(data);
  pn_data_next(data);
  pn_data_enter(data);
  pn_data_lookup(data, "network-host");
  pn_bytes_t host = pn_data_get_bytes(data);
  pn_data_rewind(data);
  return host.start;
}

int pn_condition_redirect_port(pn_condition_t *condition)
{
  pn_data_t *data = pn_condition_info(condition);
  pn_data_rewind(data);
  pn_data_next(data);
  pn_data_enter(data);
  pn_data_lookup(data, "port");
  int port = pn_data_get_int(data);
  pn_data_rewind(data);
  return port;
}

pn_connection_t *pn_event_connection(pn_event_t *event)
{
  pn_session_t *ssn;
  pn_transport_t *transport;

  switch (pn_class_id(pn_event_class(event))) {
  case CID_pn_connection:
    return (pn_connection_t *) pn_event_context(event);
  case CID_pn_transport:
    transport = pn_event_transport(event);
    if (transport)
      return transport->connection;
    return NULL;
  default:
    ssn = pn_event_session(event);
    if (ssn)
     return pn_session_connection(ssn);
  }
  return NULL;
}

pn_session_t *pn_event_session(pn_event_t *event)
{
  pn_link_t *link;
  switch (pn_class_id(pn_event_class(event))) {
  case CID_pn_session:
    return (pn_session_t *) pn_event_context(event);
  default:
    link = pn_event_link(event);
    if (link)
      return pn_link_session(link);
  }
  return NULL;
}

pn_link_t *pn_event_link(pn_event_t *event)
{
  pn_delivery_t *dlv;
  switch (pn_class_id(pn_event_class(event))) {
  case CID_pn_link:
    return (pn_link_t *) pn_event_context(event);
  default:
    dlv = pn_event_delivery(event);
    if (dlv)
      return pn_delivery_link(dlv);
  }
  return NULL;
}

pn_delivery_t *pn_event_delivery(pn_event_t *event)
{
  switch (pn_class_id(pn_event_class(event))) {
  case CID_pn_delivery:
    return (pn_delivery_t *) pn_event_context(event);
  default:
    return NULL;
  }
}

pn_transport_t *pn_event_transport(pn_event_t *event)
{
  switch (pn_class_id(pn_event_class(event))) {
  case CID_pn_transport:
    return (pn_transport_t *) pn_event_context(event);
  default:
    {
      pn_connection_t *conn = pn_event_connection(event);
      if (conn)
        return pn_connection_transport(conn);
      return NULL;
    }
  }
}
