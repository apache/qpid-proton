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

#include <proton/messenger.h>
#include <proton/driver.h>
#include <proton/util.h>
#include <proton/ssl.h>
#include <proton/buffer.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <uuid/uuid.h>
#include "util.h"

typedef struct {
  size_t capacity;
  int window;
  pn_sequence_t lwm;
  pn_sequence_t mwm;
  pn_sequence_t hwm;
  pn_delivery_t **deliveries;
} pn_queue_t;

struct pn_messenger_t {
  char *name;
  char *certificate;
  char *private_key;
  char *password;
  char *trusted_certificates;
  int timeout;
  pn_driver_t *driver;
  int credit;
  int distributed;
  uint64_t next_tag;
  pn_queue_t outgoing;
  pn_queue_t incoming;
  pn_accept_mode_t accept_mode;
  pn_subscription_t *subscriptions;
  size_t sub_capacity;
  size_t sub_count;
  pn_subscription_t *incoming_subscription;
  pn_buffer_t *buffer;
  pn_error_t *error;
};

struct pn_subscription_t {
  char *scheme;
  void *context;
};

void pn_queue_init(pn_queue_t *queue)
{
  queue->capacity = 1024;
  queue->window = 0;
  queue->lwm = 0;
  queue->mwm = 0;
  queue->hwm = 0;
  queue->deliveries = calloc(queue->capacity, sizeof(pn_delivery_t *));
}

void pn_queue_tini(pn_queue_t *queue)
{
  free(queue->deliveries);
}

bool pn_queue_contains(pn_queue_t *queue, pn_sequence_t id)
{
  return (id - queue->lwm >= 0) && (queue->hwm - id > 0);
}

pn_delivery_t *pn_queue_get(pn_queue_t *queue, pn_sequence_t id)
{
  if (pn_queue_contains(queue, id)) {
    size_t offset = id - queue->lwm;
    assert(offset >= 0 && offset < queue->capacity);
    return queue->deliveries[offset];
  } else {
    return NULL;
  }
}

void pn_queue_gc(pn_queue_t *queue)
{
  size_t count = queue->hwm - queue->lwm;
  size_t delta = 0;

  while (delta < count && !queue->deliveries[delta]) {
    delta++;
  }

  memmove(queue->deliveries, queue->deliveries + delta, (count - delta)*sizeof(pn_delivery_t *));
  queue->lwm += delta;
  if (queue->mwm - queue->lwm < 0) {
    queue->mwm = queue->lwm;
  }
}

void pn_incref(pn_connection_t *conn)
{
  intptr_t refcount = (intptr_t) pn_connection_get_context(conn);
  pn_connection_set_context(conn, (void *) (refcount + 1));
}

void pn_decref(pn_connection_t *conn)
{
  intptr_t refcount = (intptr_t) pn_connection_get_context(conn);
  pn_connection_set_context(conn, (void *) (refcount - 1));
  if (refcount == 1) {
    pn_connection_free(conn);
  }
}

void pn_queue_del(pn_queue_t *queue, pn_delivery_t *delivery)
{
  pn_sequence_t id = (pn_sequence_t) (intptr_t) pn_delivery_get_context(delivery);
  if (pn_queue_contains(queue, id)) {
    size_t offset = id - queue->lwm;
    assert(offset >= 0 && offset < queue->capacity);
    queue->deliveries[offset] = NULL;
    pn_delivery_set_context(delivery, NULL);
    pn_connection_t *conn =
      pn_session_connection(pn_link_session(pn_delivery_link(delivery)));
    pn_decref(conn);
  }
}

pn_sequence_t pn_queue_add(pn_queue_t *queue, pn_delivery_t *delivery)
{
  pn_sequence_t id = queue->hwm++;
  size_t offset = id - queue->lwm;
  PN_ENSUREZ(queue->deliveries, queue->capacity, offset + 1);
  assert(offset >= 0 && offset < queue->capacity);
  queue->deliveries[offset] = delivery;
  pn_delivery_set_context(delivery, (void *) (intptr_t) id);
  pn_connection_t *conn =
    pn_session_connection(pn_link_session(pn_delivery_link(delivery)));
  pn_incref(conn);
  return id;
}

void pn_queue_slide(pn_queue_t *queue)
{
  if (queue->window >= 0) {
    while (queue->hwm - queue->mwm > queue->window) {
      pn_delivery_t *d = pn_queue_get(queue, queue->lwm);
      if (d) {
        pn_delivery_settle(d);
        pn_queue_del(queue, d);
      } else {
        pn_queue_gc(queue);
      }
    }
  }
  pn_queue_gc(queue);
}

int pn_queue_update(pn_queue_t *queue, pn_sequence_t id, pn_status_t status,
                    int flags, bool settle, bool match)
{
  if (!pn_queue_contains(queue, id)) {
    return 0;
  }

  size_t start;
  if (PN_CUMULATIVE | flags) {
    start = queue->lwm;
  } else {
    start = id;
  }

  for (pn_sequence_t i = start; i <= id; i++) {
    pn_delivery_t *d = pn_queue_get(queue, i);
    if (d) {
      if (match) {
        pn_delivery_update(d, pn_delivery_remote_state(d));
      } else {
        switch (status) {
        case PN_STATUS_ACCEPTED:
          pn_delivery_update(d, PN_ACCEPTED);
          break;
        case PN_STATUS_REJECTED:
          pn_delivery_update(d, PN_REJECTED);
          break;
        default:
          break;
        }
      }
      if (pn_delivery_local_state(d) && i - queue->mwm < 0) {
        queue->mwm = i;
      }
      if (settle) {
        pn_delivery_settle(d);
        pn_queue_del(queue, d);
      }
    }
  }

  pn_queue_slide(queue);

  return 0;
}

#define OUTGOING (0x0000000000000000)
#define INCOMING (0x1000000000000000)

#define pn_tracker(direction, sequence) ((direction) | (sequence))
#define pn_tracker_direction(tracker) ((tracker) & (0x1000000000000000))
#define pn_tracker_sequence(tracker) ((pn_sequence_t) ((tracker) & (0x00000000FFFFFFFF)))

static char *build_name(const char *name)
{
  if (name) {
    return pn_strdup(name);
  } else {
    char *generated = malloc(37*sizeof(char));
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, generated);
    return generated;
  }
}

pn_messenger_t *pn_messenger(const char *name)
{
  pn_messenger_t *m = (pn_messenger_t *) malloc(sizeof(pn_messenger_t));

  if (m) {
    m->name = build_name(name);
    m->certificate = NULL;
    m->private_key = NULL;
    m->password = NULL;
    m->trusted_certificates = NULL;
    m->timeout = -1;
    m->driver = pn_driver();
    m->credit = 0;
    m->distributed = 0;
    m->next_tag = 0;
    pn_queue_init(&m->outgoing);
    pn_queue_init(&m->incoming);
    m->accept_mode = PN_ACCEPT_MODE_AUTO;
    m->subscriptions = NULL;
    m->sub_capacity = 0;
    m->sub_count = 0;
    m->incoming_subscription = NULL;
    m->buffer = pn_buffer(1024);
    m->error = pn_error();
  }

  return m;
}

const char *pn_messenger_name(pn_messenger_t *messenger)
{
  return messenger->name;
}

int pn_messenger_set_certificate(pn_messenger_t *messenger, const char *certificate)
{
  if (messenger->certificate) free(messenger->certificate);
  messenger->certificate = pn_strdup(certificate);
  return 0;
}

const char *pn_messenger_get_certificate(pn_messenger_t *messenger)
{
  return messenger->certificate;
}

int pn_messenger_set_private_key(pn_messenger_t *messenger, const char *private_key)
{
  if (messenger->private_key) free(messenger->private_key);
  messenger->private_key = pn_strdup(private_key);
  return 0;
}

const char *pn_messenger_get_private_key(pn_messenger_t *messenger)
{
  return messenger->private_key;
}

int pn_messenger_set_password(pn_messenger_t *messenger, const char *password)
{
  if (messenger->password) free(messenger->password);
  messenger->password = pn_strdup(password);
  return 0;
}

const char *pn_messenger_get_password(pn_messenger_t *messenger)
{
  return messenger->password;
}

int pn_messenger_set_trusted_certificates(pn_messenger_t *messenger, const char *trusted_certificates)
{
  if (messenger->trusted_certificates) free(messenger->trusted_certificates);
  messenger->trusted_certificates = pn_strdup(trusted_certificates);
  return 0;
}

const char *pn_messenger_get_trusted_certificates(pn_messenger_t *messenger)
{
  return messenger->trusted_certificates;
}

int pn_messenger_set_timeout(pn_messenger_t *messenger, int timeout)
{
  if (!messenger) return PN_ARG_ERR;
  messenger->timeout = timeout;
  return 0;
}

int pn_messenger_get_timeout(pn_messenger_t *messenger)
{
  return messenger ? messenger->timeout : 0;
}

void pn_messenger_free(pn_messenger_t *messenger)
{
  if (messenger) {
    free(messenger->name);
    free(messenger->certificate);
    free(messenger->private_key);
    free(messenger->password);
    free(messenger->trusted_certificates);
    pn_driver_free(messenger->driver);
    pn_buffer_free(messenger->buffer);
    pn_error_free(messenger->error);
    pn_queue_tini(&messenger->incoming);
    pn_queue_tini(&messenger->outgoing);
    for (int i = 0; i < messenger->sub_count; i++) {
      free(messenger->subscriptions[i].scheme);
    }
    free(messenger->subscriptions);
    free(messenger);
  }
}

int pn_messenger_errno(pn_messenger_t *messenger)
{
  if (messenger) {
    return pn_error_code(messenger->error);
  } else {
    return PN_ARG_ERR;
  }
}

const char *pn_messenger_error(pn_messenger_t *messenger)
{
  if (messenger) {
    return pn_error_text(messenger->error);
  } else {
    return NULL;
  }
}

void pn_messenger_flow(pn_messenger_t *messenger)
{
  while (messenger->credit > 0) {
    int prev = messenger->credit;
    pn_connector_t *ctor = pn_connector_head(messenger->driver);
    while (ctor) {
      pn_connection_t *conn = pn_connector_connection(ctor);

      pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
      while (link && messenger->credit > 0) {
        if (pn_link_is_receiver(link)) {
          pn_link_flow(link, 1);
          messenger->credit--;
          messenger->distributed++;
        }
        link = pn_link_next(link, PN_LOCAL_ACTIVE);
      }

      ctor = pn_connector_next(ctor);
    }
    if (messenger->credit == prev) break;
  }
}

void pn_messenger_endpoints(pn_messenger_t *messenger, pn_connection_t *conn)
{
  if (pn_connection_state(conn) | PN_LOCAL_UNINIT) {
    pn_connection_open(conn);
  }

  pn_delivery_t *d = pn_work_head(conn);
  while (d) {
    pn_link_t *link = pn_delivery_link(d);
    if (pn_delivery_updated(d) && pn_link_is_sender(link)) {
      pn_delivery_update(d, pn_delivery_remote_state(d));
    }
    pn_delivery_clear(d);
    d = pn_work_next(d);
  }

  pn_queue_slide(&messenger->outgoing);

  if (pn_work_head(conn)) {
    return;
  }

  pn_session_t *ssn = pn_session_head(conn, PN_LOCAL_UNINIT);
  while (ssn) {
    pn_session_open(ssn);
    ssn = pn_session_next(ssn, PN_LOCAL_UNINIT);
  }

  pn_link_t *link = pn_link_head(conn, PN_LOCAL_UNINIT);
  while (link) {
    pn_terminus_copy(pn_link_source(link), pn_link_remote_source(link));
    pn_terminus_copy(pn_link_target(link), pn_link_remote_target(link));
    pn_link_open(link);
    link = pn_link_next(link, PN_LOCAL_UNINIT);
  }

  pn_messenger_flow(messenger);

  ssn = pn_session_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while (ssn) {
    pn_session_close(ssn);
    ssn = pn_session_next(ssn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  link = pn_link_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while (link) {
    pn_link_close(link);
    link = pn_link_next(link, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  if (pn_connection_state(conn) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)) {
    pn_connection_close(conn);
  }
}

void pn_messenger_reclaim(pn_messenger_t *messenger, pn_connection_t *conn)
{
  pn_link_t *link = pn_link_head(conn, 0);
  while (link) {
    if (pn_link_is_receiver(link) && pn_link_credit(link) > 0) {
      int credit = pn_link_credit(link);
      messenger->credit += credit;
      messenger->distributed -= credit;
    }
    link = pn_link_next(link, 0);
  }
}

static long int millis(struct timeval tv)
{
  return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

int pn_messenger_tsync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *), int timeout)
{
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connector_process(ctor);
    ctor = pn_connector_next(ctor);
  }

  struct timeval now;
  if (gettimeofday(&now, NULL)) pn_fatal("gettimeofday failed\n");
  long int deadline = millis(now) + timeout;
  bool pred;

  while (true) {
    pred = predicate(messenger);
    int remaining = deadline - millis(now);
    if (pred || (timeout >= 0 && remaining < 0)) break;

    int error = pn_driver_wait(messenger->driver, remaining);
    if (error)
        return error;

    pn_listener_t *l;
    while ((l = pn_driver_listener(messenger->driver))) {
      pn_subscription_t *sub = (pn_subscription_t *) pn_listener_context(l);
      char *scheme = sub->scheme;
      pn_connector_t *c = pn_listener_accept(l);
      pn_transport_t *t = pn_connector_transport(c);
      pn_ssl_t *ssl = pn_ssl(t);
      pn_ssl_init(ssl, PN_SSL_MODE_SERVER);
      if (messenger->certificate) {
        pn_ssl_set_credentials(ssl, messenger->certificate,
                               messenger->private_key,
                               messenger->password);
      }
      if (!(scheme && !strcmp(scheme, "amqps"))) {
        pn_ssl_allow_unsecured_client(ssl);
      }
      pn_sasl_t *sasl = pn_sasl(t);
      pn_sasl_mechanisms(sasl, "ANONYMOUS");
      pn_sasl_server(sasl);
      pn_sasl_done(sasl, PN_SASL_OK);
      pn_connection_t *conn = pn_connection();
      pn_incref(conn);
      pn_connection_set_container(conn, messenger->name);
      pn_connector_set_connection(c, conn);
    }

    pn_connector_t *c;
    while ((c = pn_driver_connector(messenger->driver))) {
      pn_connector_process(c);
      pn_connection_t *conn = pn_connector_connection(c);
      pn_messenger_endpoints(messenger, conn);
      if (pn_connector_closed(c)) {
        pn_connector_free(c);
        pn_messenger_reclaim(messenger, conn);
        pn_decref(conn);
        pn_messenger_flow(messenger);
      } else {
        pn_connector_process(c);
      }
    }

    if (timeout >= 0) {
      if (gettimeofday(&now, NULL)) pn_fatal("gettimeofday failed\n");
    }
  }

  return pred ? 0 : PN_TIMEOUT;
}

int pn_messenger_sync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *))
{
  return pn_messenger_tsync(messenger, predicate, messenger->timeout);
}

int pn_messenger_start(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;
  // right now this is a noop
  return 0;
}

bool pn_messenger_stopped(pn_messenger_t *messenger)
{
  return pn_connector_head(messenger->driver) == NULL;
}

int pn_messenger_stop(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;

  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      pn_link_close(link);
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    pn_connection_close(conn);
    ctor = pn_connector_next(ctor);
  }

  pn_listener_t *l = pn_listener_head(messenger->driver);
  while (l) {
    pn_listener_close(l);
    pn_listener_t *prev = l;
    l = pn_listener_next(l);
    pn_listener_free(prev);
  }

  return pn_messenger_sync(messenger, pn_messenger_stopped);
}

static bool pn_streq(const char *a, const char *b)
{
  return a == b || (a && b && !strcmp(a, b));
}

static const char *default_port(const char *scheme)
{
  if (scheme && pn_streq(scheme, "amqps"))
    return "5671";
  else
    return "5672";
}

pn_connection_t *pn_messenger_resolve(pn_messenger_t *messenger, char *address, char **name)
{
  char domain[strlen(address) + 1];
  char *scheme = NULL;
  char *user = NULL;
  char *pass = NULL;
  char *host = "0.0.0.0";
  char *port = NULL;
  parse_url(address, &scheme, &user, &pass, &host, &port, name);

  domain[0] = '\0';

  if (user) {
    strcat(domain, user);
    strcat(domain, "@");
  }
  strcat(domain, host);
  if (port) {
    strcat(domain, ":");
    strcat(domain, port);
  }

  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *connection = pn_connector_connection(ctor);
    const char *container = pn_connection_remote_container(connection);
    const char *hostname = pn_connection_get_hostname(connection);
    if (pn_streq(container, domain) || pn_streq(hostname, domain))
      return connection;
    ctor = pn_connector_next(ctor);
  }

  pn_connector_t *connector = pn_connector(messenger->driver, host,
                                           port ? port : default_port(scheme),
                                           NULL);
  if (!connector) return NULL;
  pn_transport_t *transport = pn_connector_transport(connector);
  if (scheme && !strcmp(scheme, "amqps")) {
    pn_ssl_t *ssl = pn_ssl(transport);
    pn_ssl_init(ssl, PN_SSL_MODE_CLIENT);
    if (messenger->certificate && messenger->private_key) {
      pn_ssl_set_credentials(ssl, messenger->certificate,
                             messenger->private_key,
                             messenger->password);
    }
    if (messenger->trusted_certificates) {
      pn_ssl_set_trusted_ca_db(ssl, messenger->trusted_certificates);
      pn_ssl_set_peer_authentication(ssl, PN_SSL_VERIFY_PEER, NULL);
    } else {
      pn_ssl_set_peer_authentication(ssl, PN_SSL_ANONYMOUS_PEER, NULL);
    }
  }

  pn_sasl_t *sasl = pn_sasl(transport);
  if (user) {
    pn_sasl_plain(sasl, user, pass);
  } else {
    pn_sasl_mechanisms(sasl, "ANONYMOUS");
    pn_sasl_client(sasl);
  }
  pn_connection_t *connection = pn_connection();
  pn_incref(connection);
  pn_connection_set_container(connection, messenger->name);
  pn_connection_set_hostname(connection, domain);
  pn_connection_open(connection);
  pn_connector_set_connection(connector, connection);

  return connection;
}

pn_subscription_t *pn_subscription(pn_messenger_t *messenger, const char *scheme)
{
  PN_ENSURE(messenger->subscriptions, messenger->sub_capacity, messenger->sub_count + 1);
  pn_subscription_t *sub = messenger->subscriptions + messenger->sub_count++;
  sub->scheme = pn_strdup(scheme);
  sub->context = NULL;
  return sub;
}

void *pn_subscription_get_context(pn_subscription_t *sub)
{
  assert(sub);
  return sub->context;
}

void pn_subscription_set_context(pn_subscription_t *sub, void *context)
{
  assert(sub);
  sub->context = context;
}

pn_link_t *pn_messenger_link(pn_messenger_t *messenger, const char *address, bool sender)
{
  char copy[(address ? strlen(address) : 0) + 1];
  if (address) {
    strcpy(copy, address);
  } else {
    copy[0] = '\0';
  }
  char *name = NULL;
  pn_connection_t *connection = pn_messenger_resolve(messenger, copy, &name);
  if (!connection) return NULL;

  pn_link_t *link = pn_link_head(connection, PN_LOCAL_ACTIVE);
  while (link) {
    if (pn_link_is_sender(link) == sender) {
      const char *terminus = pn_link_is_sender(link) ?
        pn_terminus_get_address(pn_link_target(link)) :
        pn_terminus_get_address(pn_link_source(link));
      if (pn_streq(name, terminus))
        return link;
    }
    link = pn_link_next(link, PN_LOCAL_ACTIVE);
  }

  pn_session_t *ssn = pn_session(connection);
  pn_session_open(ssn);
  link = sender ? pn_sender(ssn, "sender-xxx") : pn_receiver(ssn, "receiver-xxx");
  // XXX
  pn_terminus_set_address(pn_link_target(link), name);
  pn_terminus_set_address(pn_link_source(link), name);
  if (!sender) {
    pn_subscription_t *sub = pn_subscription(messenger, NULL);
    pn_link_set_context(link, sub);
  }
  pn_link_open(link);
  return link;
}

pn_link_t *pn_messenger_source(pn_messenger_t *messenger, const char *source)
{
  return pn_messenger_link(messenger, source, false);
}

pn_link_t *pn_messenger_target(pn_messenger_t *messenger, const char *target)
{
  return pn_messenger_link(messenger, target, true);
}

pn_subscription_t *pn_messenger_subscribe(pn_messenger_t *messenger, const char *source)
{
  char copy[strlen(source) + 1];
  strcpy(copy, source);

  char *scheme = NULL;
  char *user = NULL;
  char *pass = NULL;
  char *host = "0.0.0.0";
  char *port = NULL;
  char *path = NULL;

  parse_url(copy, &scheme, &user, &pass, &host, &port, &path);

  if (host[0] == '~') {
    pn_listener_t *lnr = pn_listener(messenger->driver, host + 1,
                                     port ? port : default_port(scheme), NULL);
    if (lnr) {
      pn_subscription_t *sub = pn_subscription(messenger, scheme);
      pn_listener_set_context(lnr, sub);
      return sub;
    } else {
      pn_error_format(messenger->error, PN_ERR,
                      "unable to subscribe to source: %s (%s)", source,
                      pn_driver_error(messenger->driver));
      return NULL;
    }
  } else {
    pn_link_t *src = pn_messenger_source(messenger, source);
    if (src) {
      pn_subscription_t *sub = pn_link_get_context(src);
      return sub;
    } else {
      pn_error_format(messenger->error, PN_ERR,
                      "unable to subscribe to source: %s (%s)", source,
                      pn_driver_error(messenger->driver));
      return NULL;
    }
  }
}

pn_accept_mode_t pn_messenger_get_accept_mode(pn_messenger_t *messenger)
{
  return messenger->accept_mode;
}

int pn_messenger_set_accept_mode(pn_messenger_t *messenger, pn_accept_mode_t mode)
{
  messenger->accept_mode = mode;
  return 0;
}

int pn_messenger_get_outgoing_window(pn_messenger_t *messenger)
{
  return messenger->outgoing.window;
}

int pn_messenger_set_outgoing_window(pn_messenger_t *messenger, int window)
{
  if (window >= PN_SESSION_WINDOW) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "specified window (%i) exceeds max (%i)",
                           window, PN_SESSION_WINDOW);
  }

  messenger->outgoing.window = window;
  return 0;
}

int pn_messenger_get_incoming_window(pn_messenger_t *messenger)
{
  return messenger->incoming.window;
}

int pn_messenger_set_incoming_window(pn_messenger_t *messenger, int window)
{
  if (window >= PN_SESSION_WINDOW) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "specified window (%i) exceeds max (%i)",
                           window, PN_SESSION_WINDOW);
  }

  messenger->incoming.window = window;
  return 0;
}

static void outward_munge(pn_messenger_t *mng, pn_message_t *msg)
{
  const char *address = pn_message_get_reply_to(msg);
  int len = address ? strlen(address) : 0;
  if (len > 1 && address[0] == '~' && address[1] == '/') {
    char buf[len + strlen(mng->name) + 9];
    sprintf(buf, "amqp://%s/%s", mng->name, address + 2);
    pn_message_set_reply_to(msg, buf);
  } else if (len == 0) {
    char buf[strlen(mng->name) + 8];
    sprintf(buf, "amqp://%s", mng->name);
    pn_message_set_reply_to(msg, buf);
  }
}

// static bool false_pred(pn_messenger_t *messenger) { return false; }

int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;
  if (!msg) return pn_error_set(messenger->error, PN_ARG_ERR, "null message");
  outward_munge(messenger, msg);
  const char *address = pn_message_get_address(msg);
  pn_link_t *sender = pn_messenger_target(messenger, address);
  if (!sender)
    return pn_error_format(messenger->error, PN_ERR,
                           "unable to send to address: %s (%s)", address,
                           pn_driver_error(messenger->driver));

  pn_buffer_t *buf = messenger->buffer;

  while (true) {
    char *encoded = pn_buffer_bytes(buf).start;
    size_t size = pn_buffer_capacity(buf);
    int err = pn_message_encode(msg, encoded, &size);
    if (err == PN_OVERFLOW) {
      err = pn_buffer_ensure(buf, 2*pn_buffer_capacity(buf));
      if (err) return pn_error_format(messenger->error, err, "put: error growing buffer");
    } else if (err) {
      return pn_error_format(messenger->error, err, "encode error: %s",
                             pn_message_error(msg));
    } else {
      // XXX: proper tag
      char tag[8];
      void *ptr = &tag;
      uint64_t next = messenger->next_tag++;
      *((uint64_t *) ptr) = next;
      pn_delivery_t *d = pn_delivery(sender, pn_dtag(tag, 8));
      ssize_t n = pn_link_send(sender, encoded, size);
      if (n < 0) {
        return pn_error_format(messenger->error, n, "send error: %s",
                               pn_error_text(pn_link_error(sender)));
      } else {
        pn_link_advance(sender);
        pn_queue_add(&messenger->outgoing, d);
        // XXX: doing this every time is slow, need to be smarter
        //pn_messenger_tsync(messenger, false_pred, 0);
        return 0;
      }
    }
  }

  return PN_ERR;
}

pn_tracker_t pn_messenger_outgoing_tracker(pn_messenger_t *messenger)
{
  return pn_tracker(OUTGOING, messenger->outgoing.hwm - 1);
}

pn_queue_t *pn_tracker_queue(pn_messenger_t *messenger, pn_tracker_t tracker)
{
  if (pn_tracker_direction(tracker) == OUTGOING) {
    return &messenger->outgoing;
  } else {
    return &messenger->incoming;
  }
}

static pn_status_t disp2status(pn_disposition_t disp)
{
  if (!disp) return PN_STATUS_PENDING;

  switch (disp) {
  case PN_ACCEPTED:
    return PN_STATUS_ACCEPTED;
  case PN_REJECTED:
    return PN_STATUS_REJECTED;
  default:
    assert(0);
  }

  return 0;
}

pn_status_t pn_messenger_status(pn_messenger_t *messenger, pn_tracker_t tracker)
{
  pn_queue_t *queue = pn_tracker_queue(messenger, tracker);
  pn_delivery_t *d = pn_queue_get(queue, pn_tracker_sequence(tracker));
  if (d) {
    if (pn_delivery_remote_state(d))
      return disp2status(pn_delivery_remote_state(d));
    else if (pn_delivery_settled(d))
      return disp2status(pn_delivery_local_state(d));
    else
      return PN_STATUS_PENDING;
  } else {
    return PN_STATUS_UNKNOWN;
  }
}

int pn_messenger_settle(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  pn_queue_t *queue = pn_tracker_queue(messenger, tracker);
  return pn_queue_update(queue, pn_tracker_sequence(tracker), 0, flags, true, true);
}

bool pn_messenger_sent(pn_messenger_t *messenger)
{
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_sender(link)) {
        if (pn_link_queued(link)) {
          return false;
        }

        pn_delivery_t *d = pn_unsettled_head(link);
        while (d) {
          if (!pn_delivery_remote_state(d) && !pn_delivery_settled(d)) {
            return false;
          }
          d = pn_unsettled_next(d);
        }
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }

    ctor = pn_connector_next(ctor);
  }

  return true;
}

bool pn_messenger_rcvd(pn_messenger_t *messenger)
{
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_delivery_t *d = pn_work_head(conn);
    while (d) {
      if (pn_delivery_readable(d) && !pn_delivery_partial(d)) {
        return true;
      }
      d = pn_work_next(d);
    }
    ctor = pn_connector_next(ctor);
  }

  return false;
}

int pn_messenger_send(pn_messenger_t *messenger)
{
  return pn_messenger_sync(messenger, pn_messenger_sent);
}

int pn_messenger_recv(pn_messenger_t *messenger, int n)
{
  if (!messenger) return PN_ARG_ERR;
  if (!pn_listener_head(messenger->driver) && !pn_connector_head(messenger->driver))
    return pn_error_format(messenger->error, PN_STATE_ERR, "no valid sources");
  int total = messenger->credit + messenger->distributed;
  if (n > total)
    messenger->credit += (n - total);
  pn_messenger_flow(messenger);
  return pn_messenger_sync(messenger, pn_messenger_rcvd);
}

int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;

  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_delivery_t *d = pn_work_head(conn);
    while (d) {
      if (pn_delivery_readable(d) && !pn_delivery_partial(d)) {
        pn_link_t *l = pn_delivery_link(d);
        pn_subscription_t *sub = pn_link_get_context(l);
        size_t pending = pn_delivery_pending(d);
        pn_buffer_t *buf = messenger->buffer;
        int err = pn_buffer_ensure(buf, pending + 1);
        if (err) return pn_error_format(messenger->error, err, "get: error growing buffer");
        char *encoded = pn_buffer_bytes(buf).start;
        ssize_t n = pn_link_recv(l, encoded, pending);
        if (n != pending) {
          return pn_error_format(messenger->error, n, "didn't receive pending bytes: %zi", n);
        }
        n = pn_link_recv(l, encoded + pending, 1);
        pn_link_advance(l);
        messenger->distributed--;
        if (n != PN_EOS) {
          return pn_error_format(messenger->error, n, "PN_EOS expected");
        }
        pn_queue_add(&messenger->incoming, d);
        messenger->incoming_subscription = sub;
        if (msg) {
          int err = pn_message_decode(msg, encoded, pending);
          if (err) {
            return pn_error_format(messenger->error, err, "error decoding message: %s",
                                   pn_message_error(msg));
          } else {
            if (messenger->accept_mode == PN_ACCEPT_MODE_AUTO) {
              return pn_messenger_accept(messenger,
                                         pn_messenger_incoming_tracker(messenger),
                                         0);
            } else {
              return 0;
            }
          }
        } else {
          if (messenger->accept_mode == PN_ACCEPT_MODE_AUTO) {
            return pn_messenger_accept(messenger,
                                       pn_messenger_incoming_tracker(messenger),
                                       0);
          } else {
            return 0;
          }
        }
      }
      d = pn_work_next(d);
    }

    ctor = pn_connector_next(ctor);
  }

  // XXX: need to drain credit before returning EOS

  return PN_EOS;
}

pn_tracker_t pn_messenger_incoming_tracker(pn_messenger_t *messenger)
{
  return pn_tracker(INCOMING, messenger->incoming.hwm - 1);
}

pn_subscription_t *pn_messenger_incoming_subscription(pn_messenger_t *messenger)
{
  assert(messenger);
  return messenger->incoming_subscription;
}

int pn_messenger_accept(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  if (pn_tracker_direction(tracker) != INCOMING) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "invalid tracker, incoming tracker required");
  }

  return pn_queue_update(&messenger->incoming, pn_tracker_sequence(tracker),
                         PN_ACCEPTED, flags, false, false);
}

int pn_messenger_reject(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  if (pn_tracker_direction(tracker) != INCOMING) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "invalid tracker, incoming tracker required");
  }

  return pn_queue_update(&messenger->incoming, pn_tracker_sequence(tracker),
                         PN_REJECTED, flags, false, false);
}

int pn_messenger_queued(pn_messenger_t *messenger, bool sender)
{
  if (!messenger) return 0;

  int result = 0;

  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_sender(link)) {
        if (sender) {
          result += pn_link_queued(link);
        }
      } else if (!sender) {
        result += pn_link_queued(link);
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    ctor = pn_connector_next(ctor);
  }

  return result;
}

int pn_messenger_outgoing(pn_messenger_t *messenger)
{
  return pn_messenger_queued(messenger, true);
}

int pn_messenger_incoming(pn_messenger_t *messenger)
{
  return pn_messenger_queued(messenger, false);
}

