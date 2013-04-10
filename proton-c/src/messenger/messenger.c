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
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../util.h"
#include "../platform.h"
#include "../platform_fmt.h"
#include "store.h"

typedef struct {
  const char *start;
  size_t size;
} pn_group_t;

#define MAX_GROUP (64)

typedef struct {
  size_t groups;
  pn_group_t group[MAX_GROUP];
} pn_matcher_t;

#define PN_MAX_ADDR (1024)

typedef struct {
  char text[PN_MAX_ADDR + 1];
  bool passive;
  char *scheme;
  char *user;
  char *pass;
  char *host;
  char *port;
  char *name;
} pn_address_t;

typedef struct pn_route_t pn_route_t;

#define PN_MAX_PATTERN (255)
#define PN_MAX_ROUTE (255)

struct pn_route_t {
  char pattern[PN_MAX_PATTERN + 1];
  char address[PN_MAX_ROUTE + 1];
  pn_route_t *next;
};


struct pn_messenger_t {
  char *name;
  char *certificate;
  char *private_key;
  char *password;
  char *trusted_certificates;
  int timeout;
  pn_driver_t *driver;
  bool unlimited_credit;
  int credit_batch;
  int credit;
  int distributed;
  uint64_t next_tag;
  pni_store_t *outgoing;
  pni_store_t *incoming;
  pn_subscription_t *subscriptions;
  size_t sub_capacity;
  size_t sub_count;
  pn_subscription_t *incoming_subscription;
  pn_error_t *error;
  pn_route_t *routes;
  pn_matcher_t matcher;
  pn_address_t address;
  pn_tracker_t outgoing_tracker;
  pn_tracker_t incoming_tracker;
};

struct pn_subscription_t {
  char *scheme;
  void *context;
};

typedef struct {
  char *host;
  char *port;
  pn_subscription_t *subscription;
} pn_listener_ctx_t;

pn_subscription_t *pn_subscription(pn_messenger_t *messenger, const char *scheme);

static pn_listener_ctx_t *pn_listener_ctx(pn_listener_t *lnr,
                                          pn_messenger_t *messenger,
                                          const char *scheme,
                                          const char *host,
                                          const char *port)
{
  pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_listener_context(lnr);
  assert(!ctx);
  ctx = (pn_listener_ctx_t *) malloc(sizeof(pn_listener_ctx_t));
  pn_subscription_t *sub = pn_subscription(messenger, scheme);
  ctx->subscription = sub;
  ctx->host = pn_strdup(host);
  ctx->port = pn_strdup(port);
  pn_listener_set_context(lnr, ctx);
  return ctx;
}

static void pn_listener_ctx_free(pn_listener_t *lnr)
{
  pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_listener_context(lnr);
  // XXX: subscriptions are freed when the messenger is freed pn_subscription_free(ctx->subscription);
  free(ctx->host);
  free(ctx->port);
  free(ctx);
  pn_listener_set_context(lnr, NULL);
}

typedef struct {
  char *address;
  char *scheme;
  char *user;
  char *pass;
  char *host;
  char *port;
} pn_connection_ctx_t;

static pn_connection_ctx_t *pn_connection_ctx(pn_connection_t *conn,
                                              const char *scheme,
                                              const char *user,
                                              const char *pass,
                                              const char *host,
                                              const char *port)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);
  assert(!ctx);
  ctx = (pn_connection_ctx_t *) malloc(sizeof(pn_connection_ctx_t));
  ctx->scheme = pn_strdup(scheme);
  ctx->user = pn_strdup(user);
  ctx->pass = pn_strdup(pass);
  ctx->host = pn_strdup(host);
  ctx->port = pn_strdup(port);
  pn_connection_set_context(conn, ctx);
  return ctx;
}

static void pn_connection_ctx_free(pn_connection_t *conn)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);
  free(ctx->scheme);
  free(ctx->user);
  free(ctx->pass);
  free(ctx->host);
  free(ctx->port);
  free(ctx);
  pn_connection_set_context(conn, NULL);
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
    return pn_i_genuuid();
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
    m->unlimited_credit = false;
    m->credit_batch = 10;
    m->credit = 0;
    m->distributed = 0;
    m->next_tag = 0;
    m->outgoing = pni_store();
    m->incoming = pni_store();
    m->subscriptions = NULL;
    m->sub_capacity = 0;
    m->sub_count = 0;
    m->incoming_subscription = NULL;
    m->error = pn_error();
    m->routes = NULL;
    m->outgoing_tracker = 0;
    m->incoming_tracker = 0;
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

static void pni_driver_reclaim(pn_driver_t *driver)
{
  pn_listener_t *l = pn_listener_head(driver);
  while (l) {
    pn_listener_ctx_free(l);
    l = pn_listener_next(l);
  }
}

void pn_messenger_free(pn_messenger_t *messenger)
{
  if (messenger) {
    free(messenger->name);
    free(messenger->certificate);
    free(messenger->private_key);
    free(messenger->password);
    free(messenger->trusted_certificates);
    pni_driver_reclaim(messenger->driver);
    pn_driver_free(messenger->driver);
    pn_error_free(messenger->error);
    pni_store_free(messenger->incoming);
    pni_store_free(messenger->outgoing);
    for (unsigned i = 0; i < messenger->sub_count; i++) {
      free(messenger->subscriptions[i].scheme);
    }
    free(messenger->subscriptions);
    pn_route_t *route = messenger->routes;
    while (route) {
      pn_route_t *next = route->next;
      free(route);
      route = next;
    }
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
  int link_ct = 0;
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_receiver(link)) link_ct++;
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    ctor = pn_connector_next(ctor);
  }

  if (link_ct == 0) return;

  if (messenger->unlimited_credit)
    messenger->credit = link_ct * messenger->credit_batch;

  int batch = (messenger->credit < link_ct) ? 1
    : (messenger->credit/link_ct);

  ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_receiver(link)) {

        int have = pn_link_credit(link);
        if (have < batch) {
          int need = batch - have;
          int amount = (messenger->credit < need) ? messenger->credit : need;
          pn_link_flow(link, amount);
          messenger->distributed += amount;
          messenger->credit -= amount;
          if (messenger->credit == 0) return;
        }
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    ctor = pn_connector_next(ctor);
  }
}

static void pn_transport_config(pn_messenger_t *messenger,
                                pn_connector_t *connector,
                                pn_connection_t *connection)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(connection);
  pn_transport_t *transport = pn_connector_transport(connector);
  if (ctx->scheme && !strcmp(ctx->scheme, "amqps")) {
    pn_ssl_domain_t *d = pn_ssl_domain( PN_SSL_MODE_CLIENT );
    if (messenger->certificate && messenger->private_key) {
      pn_ssl_domain_set_credentials( d, messenger->certificate,
                                     messenger->private_key,
                                     messenger->password);
    }
    if (messenger->trusted_certificates) {
      pn_ssl_domain_set_trusted_ca_db(d, messenger->trusted_certificates);
      pn_ssl_domain_set_peer_authentication(d, PN_SSL_VERIFY_PEER, NULL);
    } else {
      pn_ssl_domain_set_peer_authentication(d, PN_SSL_ANONYMOUS_PEER, NULL);
    }
    pn_ssl_t *ssl = pn_ssl(transport);
    pn_ssl_init(ssl, d, NULL);
    pn_ssl_domain_free( d );
  }

  pn_sasl_t *sasl = pn_sasl(transport);
  if (ctx->user) {
    pn_sasl_plain(sasl, ctx->user, ctx->pass);
  } else {
    pn_sasl_mechanisms(sasl, "ANONYMOUS");
    pn_sasl_client(sasl);
  }
}

static void pn_error_report(const char *pfx, const char *error)
{
  fprintf(stderr, "%s ERROR %s\n", pfx, error);
}

static void pn_condition_report(const char *pfx, pn_condition_t *condition)
{
  if (pn_condition_is_redirect(condition)) {
    fprintf(stderr, "%s NOTICE (%s) redirecting to %s:%i\n",
            pfx,
            pn_condition_get_name(condition),
            pn_condition_redirect_host(condition),
            pn_condition_redirect_port(condition));
  } else if (pn_condition_is_set(condition)) {
    char error[1024];
    snprintf(error, 1024, "(%s) %s",
             pn_condition_get_name(condition),
             pn_condition_get_description(condition));
    pn_error_report(pfx, error);
  }
}

int pni_pump_in(pn_messenger_t *messenger, const char *address, pn_link_t *receiver)
{
  pn_delivery_t *d = pn_link_current(receiver);
  if (!pn_delivery_readable(d) && !pn_delivery_partial(d)) {
    return 0;
  }

  pni_entry_t *entry = pni_store_put(messenger->incoming, address);
  pn_buffer_t *buf = pni_entry_bytes(entry);
  pni_entry_set_delivery(entry, d);

  pn_subscription_t *sub = (pn_subscription_t *) pn_link_get_context(receiver);
  pni_entry_set_context(entry, sub);

  size_t pending = pn_delivery_pending(d);
  int err = pn_buffer_ensure(buf, pending + 1);
  if (err) return pn_error_format(messenger->error, err, "get: error growing buffer");
  char *encoded = pn_buffer_bytes(buf).start;
  ssize_t n = pn_link_recv(receiver, encoded, pending);
  if (n != (ssize_t) pending) {
    return pn_error_format(messenger->error, n, "didn't receive pending bytes: %" PN_ZI, n);
  }
  n = pn_link_recv(receiver, encoded + pending, 1);
  pn_link_advance(receiver);
  if (n != PN_EOS) {
    return pn_error_format(messenger->error, n, "PN_EOS expected");
  }
  pn_buffer_append(buf, encoded, pending); // XXX

  return 0;
}

int pni_pump_out(pn_messenger_t *messenger, const char *address, pn_link_t *sender);

void pn_messenger_endpoints(pn_messenger_t *messenger, pn_connection_t *conn, pn_connector_t *ctor)
{
  if (pn_connection_state(conn) & PN_LOCAL_UNINIT) {
    pn_connection_open(conn);
  }

  pn_delivery_t *d = pn_work_head(conn);
  while (d) {
    pn_link_t *link = pn_delivery_link(d);
    if (pn_delivery_updated(d)) {
      if (pn_link_is_sender(link)) {
        pn_delivery_update(d, pn_delivery_remote_state(d));
      }
      pni_entry_t *e = (pni_entry_t *) pn_delivery_get_context(d);
      if (e) pni_entry_updated(e);
    }
    pn_delivery_clear(d);
    if (pn_delivery_readable(d)) {
      pni_pump_in(messenger, pn_terminus_get_address(pn_link_source(link)), link);
    }
    d = pn_work_next(d);
  }

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
    if (pn_link_is_receiver(link)) {
      pn_listener_t *listener = pn_connector_listener(ctor);
      pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_listener_context(listener);
      pn_link_set_context(link, ctx ? ctx->subscription : NULL);
    }
    link = pn_link_next(link, PN_LOCAL_UNINIT);
  }

  link = pn_link_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE);
  while (link) {
    if (pn_link_is_sender(link)) {
      pni_pump_out(messenger, pn_terminus_get_address(pn_link_target(link)), link);
    }
    link = pn_link_next(link, PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE);
  }

  pn_messenger_flow(messenger);

  ssn = pn_session_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while (ssn) {
    pn_condition_report("SESSION", pn_session_remote_condition(ssn));
    pn_session_close(ssn);
    ssn = pn_session_next(ssn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  link = pn_link_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while (link) {
    pn_condition_report("LINK", pn_link_remote_condition(link));
    pn_link_close(link);
    // XXX: should free link
    link = pn_link_next(link, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  if (pn_connection_state(conn) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)) {
    pn_condition_t *condition = pn_connection_remote_condition(conn);
    pn_condition_report("CONNECTION", condition);
    pn_connection_close(conn);
    if (pn_condition_is_redirect(condition)) {
      const char *host = pn_condition_redirect_host(condition);
      char buf[1024];
      sprintf(buf, "%i", pn_condition_redirect_port(condition));

      pn_connector_process(ctor);
      pn_connector_set_connection(ctor, NULL);
      pn_driver_t *driver = messenger->driver;
      pn_connector_t *connector = pn_connector(driver, host, buf, NULL);
      pn_transport_unbind(pn_connector_transport(ctor));
      pn_connection_reset(conn);
      pn_transport_config(messenger, connector, conn);
      pn_connector_set_connection(connector, conn);
    }
  } else if (pn_connector_closed(ctor) && !(pn_connection_state(conn) & PN_REMOTE_CLOSED)) {
    pn_error_report("CONNECTION", "connection aborted");
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

    pn_delivery_t *d = pn_unsettled_head(link);
    while (d) {
      pni_entry_t *e = (pni_entry_t *) pn_delivery_get_context(d);
      if (e) {
        pni_entry_set_delivery(e, NULL);
      }
      d = pn_unsettled_next(d);
    }

    link = pn_link_next(link, 0);
  }
}


pn_connection_t *pn_messenger_connection(pn_messenger_t *messenger,
                                         char *scheme,
                                         char *user,
                                         char *pass,
                                         char *host,
                                         char *port)
{
  pn_connection_t *connection = pn_connection();
  if (!connection) return NULL;
  pn_connection_ctx(connection, scheme, user, pass, host, port);

  pn_connection_set_container(connection, messenger->name);
  pn_connection_set_hostname(connection, host);
  return connection;
}

int pn_messenger_tsync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *), int timeout)
{
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_messenger_endpoints(messenger, conn, ctor);
    pn_connector_process(ctor);
    ctor = pn_connector_next(ctor);
  }

  pn_timestamp_t now = pn_i_now();
  long int deadline = now + timeout;
  bool pred;

  while (true) {
    pred = predicate(messenger);
    int remaining = deadline - now;
    if (pred || (timeout >= 0 && remaining < 0)) break;

    int error = pn_driver_wait(messenger->driver, remaining);
    if (error)
        return error;

    pn_listener_t *l;
    while ((l = pn_driver_listener(messenger->driver))) {
      pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_listener_context(l);
      pn_subscription_t *sub = ctx->subscription;
      char *scheme = sub->scheme;
      pn_connector_t *c = pn_listener_accept(l);
      pn_transport_t *t = pn_connector_transport(c);

      pn_ssl_domain_t *d = pn_ssl_domain( PN_SSL_MODE_SERVER );
      if (messenger->certificate) {
        pn_ssl_domain_set_credentials(d, messenger->certificate,
                                      messenger->private_key,
                                      messenger->password);
      }
      if (!(scheme && !strcmp(scheme, "amqps"))) {
        pn_ssl_domain_allow_unsecured_client(d);
      }
      pn_ssl_t *ssl = pn_ssl(t);
      pn_ssl_init(ssl, d, NULL);
      pn_ssl_domain_free( d );

      pn_sasl_t *sasl = pn_sasl(t);
      pn_sasl_mechanisms(sasl, "ANONYMOUS");
      pn_sasl_server(sasl);
      pn_sasl_done(sasl, PN_SASL_OK);
      pn_connection_t *conn =
        pn_messenger_connection(messenger, scheme, NULL, NULL, NULL, NULL);
      pn_connector_set_connection(c, conn);
    }

    pn_connector_t *c;
    while ((c = pn_driver_connector(messenger->driver))) {
      pn_connector_process(c);
      pn_connection_t *conn = pn_connector_connection(c);
      pn_messenger_endpoints(messenger, conn, c);
      if (pn_connector_closed(c)) {
        pn_connector_free(c);
        if (conn) {
          pn_messenger_reclaim(messenger, conn);
          pn_connection_ctx_free(conn);
          pn_connection_free(conn);
          pn_messenger_flow(messenger);
        }
      } else {
        pn_connector_process(c);
      }
    }

    if (timeout >= 0) {
      now = pn_i_now();
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
    pn_listener_ctx_free(prev);
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

static void pni_sub(pn_matcher_t *matcher, size_t group, const char *text, size_t matched)
{
  if (group > matcher->groups) {
    matcher->groups = group;
  }
  matcher->group[group].start = text - matched;
  matcher->group[group].size = matched;
}

static bool pni_match_r(pn_matcher_t *matcher, const char *pattern, const char *text, size_t group, size_t matched)
{
  bool match;

  char p = *pattern;
  char c = *text;

  switch (p) {
  case '\0': return c == '\0';
  case '%':
  case '*':
    switch (c) {
    case '\0':
      match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
      if (match) pni_sub(matcher, group, text, matched);
      return match;
    case '/':
      if (p == '%') {
        match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
        if (match) pni_sub(matcher, group, text, matched);
        return match;
      }
    default:
      match = pni_match_r(matcher, pattern, text + 1, group, matched + 1);
      if (!match) {
        match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
        if (match) pni_sub(matcher, group, text, matched);
      }
      return match;
    }
  default:
    return c == p && pni_match_r(matcher, pattern + 1, text + 1, group, 0);
  }
}

static bool pni_match(pn_matcher_t *matcher, const char *pattern, const char *text)
{
  matcher->groups = 0;
  if (pni_match_r(matcher, pattern, text, 1, 0)) {
    matcher->group[0].start = text;
    matcher->group[0].size = strlen(text);
    return true;
  } else {
    matcher->groups = 0;
    return false;
  }
}

static size_t pni_substitute(pn_matcher_t *matcher, const char *pattern, char *dest, size_t limit)
{
  size_t result = 0;

  while (*pattern) {
    switch (*pattern) {
    case '$':
      pattern++;
      if (*pattern == '$') {
        if (result < limit) {
          *dest++ = *pattern++;
        }
        result++;
      } else {
        size_t idx = 0;
        while (isdigit(*pattern)) {
          idx *= 10;
          idx += *pattern++ - '0';
        }

        if (idx <= matcher->groups) {
          pn_group_t *group = &matcher->group[idx];
          for (size_t i = 0; i < group->size; i++) {
            if (result < limit) {
              *dest++ = group->start[i];
            }
            result++;
          }
        }
      }
      break;
    default:
      if (result < limit) {
        *dest++ = *pattern++;
      }
      result++;
      break;
    }
  }

  if (result < limit) {
    *dest = '\0';
  }
  result++;

  return result;
}

static void pni_parse(pn_address_t *address)
{
  address->passive = false;
  address->scheme = NULL;
  address->user = NULL;
  address->pass = NULL;
  address->host = NULL;
  address->port = NULL;
  address->name = NULL;
  parse_url(address->text, &address->scheme, &address->user, &address->pass,
            &address->host, &address->port, &address->name);
  if (address->host[0] == '~') {
    address->passive = true;
    address->host++;
  }
}

static int pni_route(pn_messenger_t *messenger, const char *address)
{
  pn_address_t *addr = &messenger->address;
  pn_route_t *route = messenger->routes;
  while (route) {
    if (pni_match(&messenger->matcher, route->pattern, address)) {
      size_t n = pni_substitute(&messenger->matcher, route->address, addr->text, PN_MAX_ADDR);
      if (n < PN_MAX_ADDR) {
        pni_parse(addr);
        return 0;
      } else {
        return pn_error_format(messenger->error, PN_ERR,
                               "routing address exceeded maximum length: (%s -> %s)",
                               route->pattern, route->address);
      }
    }
    route = route->next;
  }

  strcpy(addr->text, address);
  pni_parse(addr);
  return 0;
}

pn_connection_t *pn_messenger_resolve(pn_messenger_t *messenger, const char *address, char **name)
{
  char domain[1024];
  if (sizeof(domain) < strlen(address) + 1) {
    pn_error_format(messenger->error, PN_ERR,
                    "address exceeded maximum length: %s", address);
    return NULL;
  }

  int err = pni_route(messenger, address);
  if (err) return NULL;

  bool passive = messenger->address.passive;
  char *scheme = messenger->address.scheme;
  char *user = messenger->address.user;
  char *pass = messenger->address.pass;
  char *host = messenger->address.host;
  char *port = messenger->address.port;
  *name = messenger->address.name;

  if (passive) {
    pn_listener_t *lnr = pn_listener_head(messenger->driver);
    while (lnr) {
      pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_listener_context(lnr);
      if (pn_streq(host, ctx->host) && pn_streq(port, ctx->port)) {
        return NULL;
      }
      lnr = pn_listener_next(lnr);
    }

    lnr = pn_listener(messenger->driver, host, port ? port : default_port(scheme), NULL);
    if (lnr) {
      pn_listener_ctx(lnr, messenger, scheme, host, port);
    } else {
      pn_error_format(messenger->error, PN_ERR,
                      "unable to bind to address %s: %s:%s", address, host, port,
                      pn_driver_error(messenger->driver));
    }

    return NULL;
  }

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
    pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(connection);
    if (pn_streq(scheme, ctx->scheme) && pn_streq(user, ctx->user) &&
        pn_streq(pass, ctx->pass) && pn_streq(host, ctx->host) &&
        pn_streq(port, ctx->port)) {
      return connection;
    }
    const char *container = pn_connection_remote_container(connection);
    if (pn_streq(container, domain)) {
      return connection;
    }
    ctor = pn_connector_next(ctor);
  }

  pn_connector_t *connector = pn_connector(messenger->driver, host,
                                           port ? port : default_port(scheme),
                                           NULL);
  if (!connector) {
    pn_error_format(messenger->error, PN_ERR,
                    "unable to connect to %s: %s", address,
                    pn_driver_error(messenger->driver));
    return NULL;
  }

  pn_connection_t *connection =
    pn_messenger_connection(messenger, scheme, user, pass, host, port);
  pn_transport_config(messenger, connector, connection);
  pn_connection_open(connection);
  pn_connector_set_connection(connector, connection);

  return connection;
}

pn_subscription_t *pn_subscription(pn_messenger_t *messenger, const char *scheme)
{
  PN_ENSURE(messenger->subscriptions, messenger->sub_capacity, messenger->sub_count + 1, pn_subscription_t);
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
  char *name = NULL;
  pn_connection_t *connection = pn_messenger_resolve(messenger, address, &name);
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
  pni_route(messenger, source);
  if (pn_error_code(messenger->error)) return NULL;

  bool passive = messenger->address.passive;
  char *scheme = messenger->address.scheme;
  char *host = messenger->address.host;
  char *port = messenger->address.port;

  if (passive) {
    pn_listener_t *lnr = pn_listener(messenger->driver, host,
                                     port ? port : default_port(scheme), NULL);
    if (lnr) {
      pn_listener_ctx_t *ctx = pn_listener_ctx(lnr, messenger, scheme, host, port);
      return ctx->subscription;
    } else {
      pn_error_format(messenger->error, PN_ERR,
                      "unable to subscribe to address %s: %s", source,
                      pn_driver_error(messenger->driver));
      return NULL;
    }
  } else {
    pn_link_t *src = pn_messenger_source(messenger, source);
    if (!src) return NULL;
    pn_subscription_t *sub = (pn_subscription_t *) pn_link_get_context(src);
    return sub;
  }
}

int pn_messenger_get_outgoing_window(pn_messenger_t *messenger)
{
  return pni_store_get_window(messenger->outgoing);
}

int pn_messenger_set_outgoing_window(pn_messenger_t *messenger, int window)
{
  if (window >= PN_SESSION_WINDOW) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "specified window (%i) exceeds max (%i)",
                           window, PN_SESSION_WINDOW);
  }

  pni_store_set_window(messenger->outgoing, window);
  return 0;
}

int pn_messenger_get_incoming_window(pn_messenger_t *messenger)
{
  return pni_store_get_window(messenger->incoming);
}

int pn_messenger_set_incoming_window(pn_messenger_t *messenger, int window)
{
  if (window >= PN_SESSION_WINDOW) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "specified window (%i) exceeds max (%i)",
                           window, PN_SESSION_WINDOW);
  }

  pni_store_set_window(messenger->incoming, window);
  return 0;
}

static void outward_munge(pn_messenger_t *mng, pn_message_t *msg)
{
  char stackbuf[256];
  char *heapbuf = NULL;
  char *buf = stackbuf;
  const char *address = pn_message_get_reply_to(msg);
  int len = address ? strlen(address) : 0;
  if (len > 1 && address[0] == '~' && address[1] == '/') {
    unsigned needed = len + strlen(mng->name) + 9;
    if (needed > sizeof(stackbuf)) {
      heapbuf = (char *) malloc(needed);
      buf = heapbuf;
    }
    sprintf(buf, "amqp://%s/%s", mng->name, address + 2);
    pn_message_set_reply_to(msg, buf);
  } else if (len == 0) {
    unsigned needed = strlen(mng->name) + 8;
    if (needed > sizeof(stackbuf)) {
      heapbuf = (char *) malloc(needed);
      buf = heapbuf;
    }
    sprintf(buf, "amqp://%s", mng->name);
    pn_message_set_reply_to(msg, buf);
  }
  if (heapbuf) free (heapbuf);
}

// static bool false_pred(pn_messenger_t *messenger) { return false; }

int pni_pump_out(pn_messenger_t *messenger, const char *address, pn_link_t *sender)
{
  pni_entry_t *entry = pni_store_get(messenger->outgoing, address);
  if (!entry) return 0;
  pn_buffer_t *buf = pni_entry_bytes(entry);
  pn_bytes_t bytes = pn_buffer_bytes(buf);
  char *encoded = bytes.start;
  size_t size = bytes.size;

  // XXX: proper tag
  char tag[8];
  void *ptr = &tag;
  uint64_t next = messenger->next_tag++;
  *((uint64_t *) ptr) = next;
  pn_delivery_t *d = pn_delivery(sender, pn_dtag(tag, 8));
  pni_entry_set_delivery(entry, d);
  ssize_t n = pn_link_send(sender, encoded, size);
  if (n < 0) {
    pni_entry_free(entry);
    return pn_error_format(messenger->error, n, "send error: %s",
                           pn_error_text(pn_link_error(sender)));
  } else {
    pn_link_advance(sender);
    pni_entry_free(entry);
    // XXX: doing this every time is slow, need to be smarter
    //pn_messenger_tsync(messenger, false_pred, 0);
    return 0;
  }
}

int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;
  if (!msg) return pn_error_set(messenger->error, PN_ARG_ERR, "null message");
  outward_munge(messenger, msg);
  const char *address = pn_message_get_address(msg);

  pni_entry_t *entry = pni_store_put(messenger->outgoing, address);
  if (!entry)
    return pn_error_format(messenger->error, PN_ERR, "store error");

  messenger->outgoing_tracker = pn_tracker(OUTGOING, pni_entry_tracker(entry));
  pn_buffer_t *buf = pni_entry_bytes(entry);

  while (true) {
    char *encoded = pn_buffer_bytes(buf).start;
    size_t size = pn_buffer_capacity(buf);
    int err = pn_message_encode(msg, encoded, &size);
    if (err == PN_OVERFLOW) {
      err = pn_buffer_ensure(buf, 2*pn_buffer_capacity(buf));
      if (err) {
        pni_entry_free(entry);
        return pn_error_format(messenger->error, err, "put: error growing buffer");
      }
    } else if (err) {
      return pn_error_format(messenger->error, err, "encode error: %s",
                             pn_message_error(msg));
    } else {
      pn_buffer_append(buf, encoded, size); // XXX
      pn_link_t *sender = pn_messenger_target(messenger, address);
      if (!sender) return 0;
      return pni_pump_out(messenger, address, sender);
    }
  }

  return PN_ERR;
}

pn_tracker_t pn_messenger_outgoing_tracker(pn_messenger_t *messenger)
{
  return messenger->outgoing_tracker;
}

pni_store_t *pn_tracker_store(pn_messenger_t *messenger, pn_tracker_t tracker)
{
  if (pn_tracker_direction(tracker) == OUTGOING) {
    return messenger->outgoing;
  } else {
    return messenger->incoming;
  }
}

pn_status_t pn_messenger_status(pn_messenger_t *messenger, pn_tracker_t tracker)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  pni_entry_t *e = pni_store_track(store, pn_tracker_sequence(tracker));
  if (e) {
    return pni_entry_get_status(e);
  } else {
    return PN_STATUS_UNKNOWN;
  }
}

int pn_messenger_settle(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  return pni_store_update(store, pn_tracker_sequence(tracker), (pn_status_t) 0, flags, true, true);
}

// true if all pending output has been sent to peer
bool pn_messenger_sent(pn_messenger_t *messenger)
{
  if (pni_store_size(messenger->outgoing) > 0) return false;

  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {

    // check if transport is done generating output
    pn_transport_t *transport = pn_connector_transport(ctor);
    if (transport) {
      if (!pn_transport_quiesced(transport)) {
        pn_connector_process(ctor);
        return false;
      }
    }

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
  if (pni_store_size(messenger->incoming) > 0) return true;

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

  if (!pn_connector_head(messenger->driver) && !pn_listener_head(messenger->driver)) {
    return true;
  } else {
    return false;
  }
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
  if (n == -1) {
    messenger->unlimited_credit = true;
  } else  {
    messenger->unlimited_credit = false;
    if (n > total)
      messenger->credit += (n - total);
  }
  pn_messenger_flow(messenger);
  int err = pn_messenger_sync(messenger, pn_messenger_rcvd);
  if (err) return err;
  if (!pn_messenger_incoming(messenger) &&
      !pn_listener_head(messenger->driver) &&
      !pn_connector_head(messenger->driver)) {
    return pn_error_format(messenger->error, PN_STATE_ERR, "no valid sources");
  } else {
    return 0;
  }
}

int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;

  pni_entry_t *entry = pni_store_get(messenger->incoming, NULL);
  // XXX: need to drain credit before returning EOS
  if (!entry) return PN_EOS;

  messenger->incoming_tracker = pn_tracker(INCOMING, pni_entry_tracker(entry));
  pn_buffer_t *buf = pni_entry_bytes(entry);
  pn_bytes_t bytes = pn_buffer_bytes(buf);
  const char *encoded = bytes.start;
  size_t size = bytes.size;

  messenger->distributed--;
  messenger->incoming_subscription = (pn_subscription_t *) pni_entry_get_context(entry);

  if (msg) {
    int err = pn_message_decode(msg, encoded, size);
    pni_entry_free(entry);
    if (err) {
      return pn_error_format(messenger->error, err, "error decoding message: %s",
                             pn_message_error(msg));
    } else {
      return 0;
    }
  } else {
    pni_entry_free(entry);
    return 0;
  }
}

pn_tracker_t pn_messenger_incoming_tracker(pn_messenger_t *messenger)
{
  return messenger->incoming_tracker;
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

  return pni_store_update(messenger->incoming, pn_tracker_sequence(tracker),
                          (pn_status_t) PN_ACCEPTED, flags, false, false);
}

int pn_messenger_reject(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  if (pn_tracker_direction(tracker) != INCOMING) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "invalid tracker, incoming tracker required");
  }

  return pni_store_update(messenger->incoming, pn_tracker_sequence(tracker),
                          (pn_status_t) PN_REJECTED, flags, false, false);
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
  return pni_store_size(messenger->outgoing) + pn_messenger_queued(messenger, true);
}

int pn_messenger_incoming(pn_messenger_t *messenger)
{
  return pni_store_size(messenger->incoming) + pn_messenger_queued(messenger, false);
}

int pn_messenger_route(pn_messenger_t *messenger, const char *pattern, const char *address)
{
  if (strlen(pattern) > PN_MAX_PATTERN || strlen(address) > PN_MAX_ROUTE) {
    return PN_ERR;
  }
  pn_route_t *route = (pn_route_t *) malloc(sizeof(pn_route_t));
  if (!route) return PN_ERR;

  strcpy(route->pattern, pattern);
  strcpy(route->address, address);
  route->next = NULL;

  pn_route_t *tail = messenger->routes;
  if (!tail) {
    messenger->routes = route;
  } else {
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = route;
  }

  return 0;
}
