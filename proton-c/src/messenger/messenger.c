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

#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/object.h>
#include <proton/sasl.h>
#include <proton/session.h>
#include <proton/selector.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "platform.h"
#include "platform_fmt.h"
#include "store.h"
#include "transform.h"
#include "subscription.h"
#include "selectable.h"
#include "log_private.h"

typedef struct pn_link_ctx_t pn_link_ctx_t;

typedef struct {
  pn_string_t *text;
  bool passive;
  char *scheme;
  char *user;
  char *pass;
  char *host;
  char *port;
  char *name;
} pn_address_t;

// algorithm for granting credit to receivers
typedef enum {
  // pn_messenger_recv( X ), where:
  LINK_CREDIT_EXPLICIT, // X > 0
  LINK_CREDIT_AUTO,     // X == -1
  LINK_CREDIT_MANUAL    // X == -2
} pn_link_credit_mode_t;

struct pn_messenger_t {
  pn_address_t address;
  char *name;
  char *certificate;
  char *private_key;
  char *password;
  char *trusted_certificates;
  pn_io_t *io;
  pn_list_t *pending; // pending selectables
  pn_selectable_t *interruptor;
  pn_socket_t ctrl[2];
  pn_list_t *listeners;
  pn_list_t *connections;
  pn_selector_t *selector;
  pn_collector_t *collector;
  pn_list_t *credited;
  pn_list_t *blocked;
  pn_timestamp_t next_drain;
  uint64_t next_tag;
  pni_store_t *outgoing;
  pni_store_t *incoming;
  pn_list_t *subscriptions;
  pn_subscription_t *incoming_subscription;
  pn_error_t *error;
  pn_transform_t *routes;
  pn_transform_t *rewrites;
  pn_tracker_t outgoing_tracker;
  pn_tracker_t incoming_tracker;
  pn_string_t *original;
  pn_string_t *rewritten;
  pn_string_t *domain;
  int timeout;
  int send_threshold;
  pn_link_credit_mode_t credit_mode;
  int credit_batch;  // when LINK_CREDIT_AUTO
  int credit;        // available
  int distributed;   // credit
  int receivers;     // # receiver links
  int draining;      // # links in drain state
  int connection_error;
  int flags;
  int snd_settle_mode;          /* pn_snd_settle_mode_t or -1 for unset */
  pn_rcv_settle_mode_t rcv_settle_mode;
  pn_tracer_t tracer;
  pn_ssl_verify_mode_t ssl_peer_authentication_mode;
  bool blocking;
  bool passive;
  bool interrupted;
  bool worked;
};

#define CTX_HEAD                                \
  pn_messenger_t *messenger;                    \
  pn_selectable_t *selectable;                  \
  bool pending;

typedef struct pn_ctx_t {
  CTX_HEAD
} pn_ctx_t;

typedef struct {
  CTX_HEAD
  char *host;
  char *port;
  pn_subscription_t *subscription;
  pn_ssl_domain_t *domain;
} pn_listener_ctx_t;

typedef struct {
  CTX_HEAD
  pn_connection_t *connection;
  char *address;
  char *scheme;
  char *user;
  char *pass;
  char *host;
  char *port;
  pn_listener_ctx_t *listener;
} pn_connection_ctx_t;

static pn_connection_ctx_t *pni_context(pn_selectable_t *sel)
{
  assert(sel);
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pni_selectable_get_context(sel);
  assert(ctx);
  return ctx;
}

static pn_transport_t *pni_transport(pn_selectable_t *sel)
{
  return pn_connection_transport(pni_context(sel)->connection);
}

static ssize_t pni_connection_capacity(pn_selectable_t *sel)
{
  pn_transport_t *transport = pni_transport(sel);
  ssize_t capacity = pn_transport_capacity(transport);
  if (capacity < 0) {
    if (pn_transport_closed(transport)) {
      pn_selectable_terminate(sel);
    }
  }
  return capacity;
}

bool pn_messenger_flow(pn_messenger_t *messenger);

static ssize_t pni_connection_pending(pn_selectable_t *sel)
{
  pn_connection_ctx_t *ctx = pni_context(sel);
  pn_messenger_flow(ctx->messenger);
  pn_transport_t *transport = pni_transport(sel);
  ssize_t pending = pn_transport_pending(transport);
  if (pending < 0) {
    if (pn_transport_closed(transport)) {
      pn_selectable_terminate(sel);
    }
  }
  return pending;
}

static pn_timestamp_t pni_connection_deadline(pn_selectable_t *sel)
{
  pn_connection_ctx_t *ctx = pni_context(sel);
  return ctx->messenger->next_drain;
}

static void pni_connection_update(pn_selectable_t *sel) {
  ssize_t c = pni_connection_capacity(sel);
  pn_selectable_set_reading(sel, c > 0);
  ssize_t p = pni_connection_pending(sel);
  pn_selectable_set_writing(sel, p > 0);
  pn_selectable_set_deadline(sel, pni_connection_deadline(sel));
  if (c < 0 && p < 0) {
    pn_selectable_terminate(sel);
  }
}

#include <errno.h>

static void pn_error_report(const char *pfx, const char *error)
{
  pn_logf("%s ERROR %s", pfx, error);
}

void pni_modified(pn_ctx_t *ctx)
{
  pn_messenger_t *m = ctx->messenger;
  pn_selectable_t *sel = ctx->selectable;
  if (pn_selectable_is_registered(sel) && !ctx->pending) {
    pn_list_add(m->pending, sel);
    ctx->pending = true;
  }
}

void pni_conn_modified(pn_connection_ctx_t *ctx)
{
  pni_connection_update(ctx->selectable);
  pni_modified((pn_ctx_t *) ctx);
}

void pni_lnr_modified(pn_listener_ctx_t *lnr)
{
  pni_modified((pn_ctx_t *) lnr);
}

int pn_messenger_process_events(pn_messenger_t *messenger);

static void pni_connection_error(pn_selectable_t *sel)
{
  pn_transport_t *transport = pni_transport(sel);
  pn_transport_close_tail(transport);
  pn_transport_close_head(transport);
}

static void pni_connection_readable(pn_selectable_t *sel)
{
  pn_connection_ctx_t *context = pni_context(sel);
  pn_messenger_t *messenger = context->messenger;
  pn_connection_t *connection = context->connection;
  pn_transport_t *transport = pni_transport(sel);
  ssize_t capacity = pn_transport_capacity(transport);
  if (capacity > 0) {
    ssize_t n = pn_recv(messenger->io, pn_selectable_get_fd(sel),
                        pn_transport_tail(transport), capacity);
    if (n <= 0) {
      if (n == 0 || !pn_wouldblock(messenger->io)) {
        if (n < 0) perror("recv");
        pn_transport_close_tail(transport);
        if (!(pn_connection_state(connection) & PN_REMOTE_CLOSED)) {
          pn_error_report("CONNECTION", "connection aborted (remote)");
        }
      }
    } else {
      int err = pn_transport_process(transport, (size_t)n);
      if (err)
        pn_error_copy(messenger->error, pn_transport_error(transport));
    }
  }

  pn_messenger_process_events(messenger);
  pn_messenger_flow(messenger);
  messenger->worked = true;
  pni_conn_modified(context);
}

static void pni_connection_writable(pn_selectable_t *sel)
{
  pn_connection_ctx_t *context = pni_context(sel);
  pn_messenger_t *messenger = context->messenger;
  pn_transport_t *transport = pni_transport(sel);
  ssize_t pending = pn_transport_pending(transport);
  if (pending > 0) {
    ssize_t n = pn_send(messenger->io, pn_selectable_get_fd(sel),
                        pn_transport_head(transport), pending);
    if (n < 0) {
      if (!pn_wouldblock(messenger->io)) {
        perror("send");
        pn_transport_close_head(transport);
      }
    } else {
      pn_transport_pop(transport, n);
    }
  }

  pn_messenger_process_events(messenger);
  pn_messenger_flow(messenger);
  messenger->worked = true;
  pni_conn_modified(context);
}

static void pni_connection_expired(pn_selectable_t *sel)
{
  pn_connection_ctx_t *ctx = pni_context(sel);
  pn_messenger_flow(ctx->messenger);
  ctx->messenger->worked = true;
  pni_conn_modified(ctx);
}

static void pni_messenger_reclaim(pn_messenger_t *messenger, pn_connection_t *conn);

static void pni_connection_finalize(pn_selectable_t *sel)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pni_selectable_get_context(sel);
  pn_socket_t fd = pn_selectable_get_fd(sel);
  pn_close(ctx->messenger->io, fd);
  pn_list_remove(ctx->messenger->pending, sel);
  pni_messenger_reclaim(ctx->messenger, ctx->connection);
}

pn_connection_t *pn_messenger_connection(pn_messenger_t *messenger,
                                         pn_socket_t sock,
                                         const char *scheme,
                                         char *user,
                                         char *pass,
                                         char *host,
                                         char *port,
                                         pn_listener_ctx_t *lnr);

static void pni_listener_readable(pn_selectable_t *sel)
{
  pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pni_selectable_get_context(sel);
  pn_subscription_t *sub = ctx->subscription;
  const char *scheme = pn_subscription_scheme(sub);
  char name[1024];
  pn_socket_t sock = pn_accept(ctx->messenger->io, pn_selectable_get_fd(sel), name, 1024);

  pn_transport_t *t = pn_transport();
  pn_transport_set_server(t);
  if (ctx->messenger->flags & PN_FLAGS_ALLOW_INSECURE_MECHS) {
      pn_sasl_t *s = pn_sasl(t);
      pn_sasl_set_allow_insecure_mechs(s, true);
  }
  pn_ssl_t *ssl = pn_ssl(t);
  pn_ssl_init(ssl, ctx->domain, NULL);

  pn_connection_t *conn = pn_messenger_connection(ctx->messenger, sock, scheme, NULL, NULL, NULL, NULL, ctx);
  pn_transport_bind(t, conn);
  pn_decref(t);
  pni_conn_modified((pn_connection_ctx_t *) pn_connection_get_context(conn));
}

static void pn_listener_ctx_free(pn_messenger_t *messenger, pn_listener_ctx_t *ctx);

static void pni_listener_finalize(pn_selectable_t *sel)
{
  pn_listener_ctx_t *lnr = (pn_listener_ctx_t *) pni_selectable_get_context(sel);
  pn_messenger_t *messenger = lnr->messenger;
  pn_close(messenger->io, pn_selectable_get_fd(sel));
  pn_list_remove(messenger->pending, sel);
  pn_listener_ctx_free(messenger, lnr);
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

static pn_listener_ctx_t *pn_listener_ctx(pn_messenger_t *messenger,
                                          const char *scheme,
                                          const char *host,
                                          const char *port)
{
  pn_socket_t socket = pn_listen(messenger->io, host, port ? port : default_port(scheme));
  if (socket == PN_INVALID_SOCKET) {
    pn_error_copy(messenger->error, pn_io_error(messenger->io));
    pn_error_format(messenger->error, PN_ERR, "CONNECTION ERROR (%s:%s): %s\n",
                    messenger->address.host, messenger->address.port,
                    pn_error_text(messenger->error));

    return NULL;
  }

  pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_class_new(PN_OBJECT, sizeof(pn_listener_ctx_t));
  ctx->messenger = messenger;
  ctx->domain = pn_ssl_domain(PN_SSL_MODE_SERVER);
  if (messenger->certificate) {
    int err = pn_ssl_domain_set_credentials(ctx->domain, messenger->certificate,
                                            messenger->private_key,
                                            messenger->password);

    if (err) {
      pn_error_format(messenger->error, PN_ERR, "invalid credentials");
      pn_ssl_domain_free(ctx->domain);
      pn_free(ctx);
      pn_close(messenger->io, socket);
      return NULL;
    }
  }

  if (!(scheme && !strcmp(scheme, "amqps"))) {
    pn_ssl_domain_allow_unsecured_client(ctx->domain);
  }

  pn_subscription_t *sub = pn_subscription(messenger, scheme, host, port);
  ctx->subscription = sub;
  ctx->host = pn_strdup(host);
  ctx->port = pn_strdup(port);

  pn_selectable_t *selectable = pn_selectable();
  pn_selectable_set_reading(selectable, true);
  pn_selectable_on_readable(selectable, pni_listener_readable);
  pn_selectable_on_release(selectable, pn_selectable_free);
  pn_selectable_on_finalize(selectable, pni_listener_finalize);
  pn_selectable_set_fd(selectable, socket);
  pni_selectable_set_context(selectable, ctx);
  pn_list_add(messenger->pending, selectable);
  ctx->selectable = selectable;
  ctx->pending = true;

  pn_list_add(messenger->listeners, ctx);
  return ctx;
}

static void pn_listener_ctx_free(pn_messenger_t *messenger, pn_listener_ctx_t *ctx)
{
  pn_list_remove(messenger->listeners, ctx);
  // XXX: subscriptions are freed when the messenger is freed pn_subscription_free(ctx->subscription);
  free(ctx->host);
  free(ctx->port);
  pn_ssl_domain_free(ctx->domain);
  pn_free(ctx);
}

static pn_connection_ctx_t *pn_connection_ctx(pn_messenger_t *messenger,
                                              pn_connection_t *conn,
                                              pn_socket_t sock,
                                              const char *scheme,
                                              const char *user,
                                              const char *pass,
                                              const char *host,
                                              const char *port,
                                              pn_listener_ctx_t *lnr)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);
  assert(!ctx);
  ctx = (pn_connection_ctx_t *) malloc(sizeof(pn_connection_ctx_t));
  ctx->messenger = messenger;
  ctx->connection = conn;
  pn_selectable_t *sel = pn_selectable();
  ctx->selectable = sel;
  pn_selectable_on_error(sel, pni_connection_error);
  pn_selectable_on_readable(sel, pni_connection_readable);
  pn_selectable_on_writable(sel, pni_connection_writable);
  pn_selectable_on_expired(sel, pni_connection_expired);
  pn_selectable_on_release(sel, pn_selectable_free);
  pn_selectable_on_finalize(sel, pni_connection_finalize);
  pn_selectable_set_fd(ctx->selectable, sock);
  pni_selectable_set_context(ctx->selectable, ctx);
  pn_list_add(messenger->pending, ctx->selectable);
  ctx->pending = true;
  ctx->scheme = pn_strdup(scheme);
  ctx->user = pn_strdup(user);
  ctx->pass = pn_strdup(pass);
  ctx->host = pn_strdup(host);
  ctx->port = pn_strdup(port);
  ctx->listener = lnr;
  pn_connection_set_context(conn, ctx);
  return ctx;
}

static void pn_connection_ctx_free(pn_connection_t *conn)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);
  if (ctx) {
    pni_selectable_set_context(ctx->selectable, NULL);
    free(ctx->scheme);
    free(ctx->user);
    free(ctx->pass);
    free(ctx->host);
    free(ctx->port);
    free(ctx);
    pn_connection_set_context(conn, NULL);
  }
}

#define OUTGOING (0x0000000000000000)
#define INCOMING (0x1000000000000000)

#define pn_tracker(direction, sequence) ((direction) | (sequence))
#define pn_tracker_direction(tracker) ((tracker) & (0x1000000000000000))
#define pn_tracker_sequence(tracker) ((pn_sequence_t) ((tracker) & (0x00000000FFFFFFFF)))


static char *build_name(const char *name)
{
  static bool seeded = false;
  // UUID standard format: 8-4-4-4-12 (36 chars, 32 alphanumeric and 4 hypens)
  static const char *uuid_fmt = "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X";

  int count = 0;
  char *generated;
  uint8_t bytes[16];
  unsigned int r = 0;

  if (name) {
    return pn_strdup(name);
  }

  if (!seeded) {
    int pid = pn_i_getpid();
    int nowish = (int)pn_i_now();
    // the lower bits of time are the most random, shift pid to push some
    // randomness into the higher order bits
    srand(nowish ^ (pid<<16));
    seeded = true;
  }

  while (count < 16) {
    if (!r) {
      r =  (unsigned int) rand();
    }

    bytes[count] = r & 0xFF;
    r >>= 8;
    count++;
  }

  // From RFC4122, the version bits are set to 0100
  bytes[6] = (bytes[6] & 0x0F) | 0x40;

  // From RFC4122, the top two bits of byte 8 get set to 01
  bytes[8] = (bytes[8] & 0x3F) | 0x80;

  generated = (char *) malloc(37*sizeof(char));
  sprintf(generated, uuid_fmt,
	  bytes[0], bytes[1], bytes[2], bytes[3],
	  bytes[4], bytes[5], bytes[6], bytes[7],
	  bytes[8], bytes[9], bytes[10], bytes[11],
	  bytes[12], bytes[13], bytes[14], bytes[15]);
  return generated;
}

struct pn_link_ctx_t {
  pn_subscription_t *subscription;
};

// compute the maximum amount of credit each receiving link is
// entitled to.  The actual credit given to the link depends on what
// amount of credit is actually available.
static int per_link_credit( pn_messenger_t *messenger )
{
  if (messenger->receivers == 0) return 0;
  int total = messenger->credit + messenger->distributed;
  return pn_max(total/messenger->receivers, 1);
}

static void link_ctx_setup( pn_messenger_t *messenger,
                            pn_connection_t *connection,
                            pn_link_t *link )
{
  if (pn_link_is_receiver(link)) {
    messenger->receivers++;
    pn_link_ctx_t *ctx = (pn_link_ctx_t *) calloc(1, sizeof(pn_link_ctx_t));
    assert( ctx );
    assert( !pn_link_get_context(link) );
    pn_link_set_context( link, ctx );
    pn_list_add(messenger->blocked, link);
  }
}

static void link_ctx_release( pn_messenger_t *messenger, pn_link_t *link )
{
  if (pn_link_is_receiver(link)) {
    pn_link_ctx_t *ctx = (pn_link_ctx_t *) pn_link_get_context( link );
    if (!ctx) return;
    assert( messenger->receivers > 0 );
    messenger->receivers--;
    if (pn_link_get_drain(link)) {
      pn_link_set_drain(link, false);
      assert( messenger->draining > 0 );
      messenger->draining--;
    }
    pn_list_remove(messenger->credited, link);
    pn_list_remove(messenger->blocked, link);
    pn_link_set_context( link, NULL );
    free( ctx );
  }
}

static void pni_interruptor_readable(pn_selectable_t *sel)
{
  pn_messenger_t *messenger = (pn_messenger_t *) pni_selectable_get_context(sel);
  char buf[1024];
  pn_read(messenger->io, pn_selectable_get_fd(sel), buf, 1024);
  messenger->interrupted = true;
}

static void pni_interruptor_finalize(pn_selectable_t *sel)
{
  pn_messenger_t *messenger = (pn_messenger_t *) pni_selectable_get_context(sel);
  messenger->interruptor = NULL;
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
    m->blocking = true;
    m->passive = false;
    m->io = pn_io();
    m->pending = pn_list(PN_WEAKREF, 0);
    m->interruptor = pn_selectable();
    pn_selectable_set_reading(m->interruptor, true);
    pn_selectable_on_readable(m->interruptor, pni_interruptor_readable);
    pn_selectable_on_release(m->interruptor, pn_selectable_free);
    pn_selectable_on_finalize(m->interruptor, pni_interruptor_finalize);
    pn_list_add(m->pending, m->interruptor);
    m->interrupted = false;
    // Explicitly initialise pipe file descriptors to invalid values in case pipe
    // fails, if we don't do this m->ctrl[0] could default to 0 - which is stdin.
    m->ctrl[0] = -1;
    m->ctrl[1] = -1;
    pn_pipe(m->io, m->ctrl);
    pn_selectable_set_fd(m->interruptor, m->ctrl[0]);
    pni_selectable_set_context(m->interruptor, m);
    m->listeners = pn_list(PN_WEAKREF, 0);
    m->connections = pn_list(PN_WEAKREF, 0);
    m->selector = pn_io_selector(m->io);
    m->collector = pn_collector();
    m->credit_mode = LINK_CREDIT_EXPLICIT;
    m->credit_batch = 1024;
    m->credit = 0;
    m->distributed = 0;
    m->receivers = 0;
    m->draining = 0;
    m->credited = pn_list(PN_WEAKREF, 0);
    m->blocked = pn_list(PN_WEAKREF, 0);
    m->next_drain = 0;
    m->next_tag = 0;
    m->outgoing = pni_store();
    m->incoming = pni_store();
    m->subscriptions = pn_list(PN_OBJECT, 0);
    m->incoming_subscription = NULL;
    m->error = pn_error();
    m->routes = pn_transform();
    m->rewrites = pn_transform();
    m->outgoing_tracker = 0;
    m->incoming_tracker = 0;
    m->address.text = pn_string(NULL);
    m->original = pn_string(NULL);
    m->rewritten = pn_string(NULL);
    m->domain = pn_string(NULL);
    m->connection_error = 0;
    m->flags = PN_FLAGS_ALLOW_INSECURE_MECHS; // TODO: Change this back to 0 for the Proton 0.11 release
    m->snd_settle_mode = -1;    /* Default depends on sender/receiver */
    m->rcv_settle_mode = PN_RCV_FIRST;
    m->tracer = NULL;
    m->ssl_peer_authentication_mode = PN_SSL_VERIFY_PEER_NAME;
  }

  return m;
}

int pni_messenger_add_subscription(pn_messenger_t *messenger, pn_subscription_t *subscription)
{
  return pn_list_add(messenger->subscriptions, subscription);
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

bool pn_messenger_is_blocking(pn_messenger_t *messenger)
{
  assert(messenger);
  return messenger->blocking;
}

int pn_messenger_set_blocking(pn_messenger_t *messenger, bool blocking)
{
  messenger->blocking = blocking;
  return 0;
}

bool pn_messenger_is_passive(pn_messenger_t *messenger)
{
  assert(messenger);
  return messenger->passive;
}

int pn_messenger_set_passive(pn_messenger_t *messenger, bool passive)
{
  messenger->passive = passive;
  return 0;
}

pn_selectable_t *pn_messenger_selectable(pn_messenger_t *messenger)
{
  assert(messenger);
  pn_messenger_process_events(messenger);
  pn_list_t *p = messenger->pending;
  size_t n = pn_list_size(p);
  if (n) {
    pn_selectable_t *s = (pn_selectable_t *) pn_list_get(p, n - 1);
    pn_list_del(p, n-1, 1);
    // this is a total hack, messenger has selectables whose context
    // are the messenger itself and whose context share a common
    // prefix that is described by pn_ctx_t
    void *c = pni_selectable_get_context(s);
    if (c != messenger) {
      pn_ctx_t *ctx = (pn_ctx_t *) c;
      ctx->pending = false;
    }
    return s;
  } else {
    return NULL;
  }
}

static void pni_reclaim(pn_messenger_t *messenger)
{
  while (pn_list_size(messenger->listeners)) {
    pn_listener_ctx_t *l = (pn_listener_ctx_t *) pn_list_get(messenger->listeners, 0);
    pn_listener_ctx_free(messenger, l);
  }

  while (pn_list_size(messenger->connections)) {
    pn_connection_t *c = (pn_connection_t *) pn_list_get(messenger->connections, 0);
    pni_messenger_reclaim(messenger, c);
  }
}

void pn_messenger_free(pn_messenger_t *messenger)
{
  if (messenger) {
    pn_free(messenger->domain);
    pn_free(messenger->rewritten);
    pn_free(messenger->original);
    pn_free(messenger->address.text);
    free(messenger->name);
    free(messenger->certificate);
    free(messenger->private_key);
    free(messenger->password);
    free(messenger->trusted_certificates);
    pni_reclaim(messenger);
    pn_free(messenger->pending);
    pn_selectable_free(messenger->interruptor);
    pn_close(messenger->io, messenger->ctrl[0]);
    pn_close(messenger->io, messenger->ctrl[1]);
    pn_free(messenger->listeners);
    pn_free(messenger->connections);
    pn_selector_free(messenger->selector);
    pn_collector_free(messenger->collector);
    pn_error_free(messenger->error);
    pni_store_free(messenger->incoming);
    pni_store_free(messenger->outgoing);
    pn_free(messenger->subscriptions);
    pn_free(messenger->rewrites);
    pn_free(messenger->routes);
    pn_free(messenger->credited);
    pn_free(messenger->blocked);
    pn_free(messenger->io);
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

pn_error_t *pn_messenger_error(pn_messenger_t *messenger)
{
  assert(messenger);
  return messenger->error;
}

// Run the credit scheduler, grant flow as needed.  Return True if
// credit allocation for any link has changed.
bool pn_messenger_flow(pn_messenger_t *messenger)
{
  bool updated = false;
  if (messenger->receivers == 0) {
    messenger->next_drain = 0;
    return updated;
  }

  if (messenger->credit_mode == LINK_CREDIT_AUTO) {
    // replenish, but limit the max total messages buffered
    const int max = messenger->receivers * messenger->credit_batch;
    const int used = messenger->distributed + pn_messenger_incoming(messenger);
    if (max > used)
      messenger->credit = max - used;
  } else if (messenger->credit_mode == LINK_CREDIT_MANUAL) {
    messenger->next_drain = 0;
    return false;
  }

  const int batch = per_link_credit(messenger);
  while (messenger->credit > 0 && pn_list_size(messenger->blocked)) {
    pn_link_t *link = (pn_link_t *) pn_list_get(messenger->blocked, 0);
    pn_list_del(messenger->blocked, 0, 1);

    const int more = pn_min( messenger->credit, batch );
    messenger->distributed += more;
    messenger->credit -= more;
    pn_link_flow(link, more);
    pn_list_add(messenger->credited, link);
    updated = true;
  }

  if (!pn_list_size(messenger->blocked)) {
    messenger->next_drain = 0;
  } else {
    // not enough credit for all links
    if (!messenger->draining) {
      pn_logf("%s: let's drain", messenger->name);
      if (messenger->next_drain == 0) {
        messenger->next_drain = pn_i_now() + 250;
        pn_logf("%s: initializing next_drain", messenger->name);
      } else if (messenger->next_drain <= pn_i_now()) {
        // initiate drain, free up at most enough to satisfy blocked
        messenger->next_drain = 0;
        int needed = pn_list_size(messenger->blocked) * batch;
        for (size_t i = 0; i < pn_list_size(messenger->credited); i++) {
          pn_link_t *link = (pn_link_t *) pn_list_get(messenger->credited, i);
          if (!pn_link_get_drain(link)) {
            pn_link_set_drain(link, true);
            needed -= pn_link_remote_credit(link);
            messenger->draining++;
            updated = true;
          }

          if (needed <= 0) {
            break;
          }
        }
      } else {
        pn_logf("%s: delaying", messenger->name);
      }
    }
  }
  return updated;
}

static int pn_transport_config(pn_messenger_t *messenger,
                               pn_connection_t *connection)
{
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(connection);
  pn_transport_t *transport = pn_connection_transport(connection);
  if (messenger->tracer)
    pn_transport_set_tracer(transport, messenger->tracer);
  if (ctx->scheme && !strcmp(ctx->scheme, "amqps")) {
    pn_ssl_domain_t *d = pn_ssl_domain(PN_SSL_MODE_CLIENT);
    if (messenger->certificate) {
      int err = pn_ssl_domain_set_credentials( d, messenger->certificate,
                                               messenger->private_key,
                                               messenger->password);
      if (err) {
        pn_ssl_domain_free(d);
        pn_error_report("CONNECTION", "invalid credentials");
        return err;
      }
    }
    if (messenger->trusted_certificates) {
      int err = pn_ssl_domain_set_trusted_ca_db(d, messenger->trusted_certificates);
      if (err) {
        pn_ssl_domain_free(d);
        pn_error_report("CONNECTION", "invalid certificate db");
        return err;
      }
      err = pn_ssl_domain_set_peer_authentication(
          d, messenger->ssl_peer_authentication_mode, NULL);
      if (err) {
        pn_ssl_domain_free(d);
        pn_error_report("CONNECTION", "error configuring ssl to verify peer");
      }
    } else {
      int err = pn_ssl_domain_set_peer_authentication(d, PN_SSL_ANONYMOUS_PEER, NULL);
      if (err) {
        pn_ssl_domain_free(d);
        pn_error_report("CONNECTION", "error configuring ssl for anonymous peer");
        return err;
      }
    }
    pn_ssl_t *ssl = pn_ssl(transport);
    pn_ssl_init(ssl, d, NULL);
    pn_ssl_domain_free( d );
  }

  return 0;
}

static void pn_condition_report(const char *pfx, pn_condition_t *condition)
{
  if (pn_condition_is_redirect(condition)) {
    pn_logf("%s NOTICE (%s) redirecting to %s:%i",
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
  if (!pn_delivery_readable(d) || pn_delivery_partial(d)) {
    return 0;
  }

  pni_entry_t *entry = pni_store_put(messenger->incoming, address);
  pn_buffer_t *buf = pni_entry_bytes(entry);
  pni_entry_set_delivery(entry, d);

  pn_link_ctx_t *ctx = (pn_link_ctx_t *) pn_link_get_context( receiver );
  pni_entry_set_context(entry, ctx ? ctx->subscription : NULL);

  size_t pending = pn_delivery_pending(d);
  int err = pn_buffer_ensure(buf, pending + 1);
  if (err) return pn_error_format(messenger->error, err, "get: error growing buffer");
  char *encoded = pn_buffer_memory(buf).start;
  ssize_t n = pn_link_recv(receiver, encoded, pending);
  if (n != (ssize_t) pending) {
    return pn_error_format(messenger->error, n,
                           "didn't receive pending bytes: %" PN_ZI " %" PN_ZI,
                           n, pending);
  }
  n = pn_link_recv(receiver, encoded + pending, 1);
  pn_link_advance(receiver);

  pn_link_t *link = receiver;

  if (messenger->credit_mode != LINK_CREDIT_MANUAL) {
    // account for the used credit
    assert(ctx);
    assert(messenger->distributed);
    messenger->distributed--;

    // replenish if low (< 20% maximum batch) and credit available
    if (!pn_link_get_drain(link) && pn_list_size(messenger->blocked) == 0 &&
        messenger->credit > 0) {
      const int max = per_link_credit(messenger);
      const int lo_thresh = (int)(max * 0.2 + 0.5);
      if (pn_link_remote_credit(link) < lo_thresh) {
        const int more =
            pn_min(messenger->credit, max - pn_link_remote_credit(link));
        messenger->credit -= more;
        messenger->distributed += more;
        pn_link_flow(link, more);
      }
    }
    // check if blocked
    if (pn_list_index(messenger->blocked, link) < 0 &&
        pn_link_remote_credit(link) == 0) {
      pn_list_remove(messenger->credited, link);
      if (pn_link_get_drain(link)) {
        pn_link_set_drain(link, false);
        assert(messenger->draining > 0);
        messenger->draining--;
      }
      pn_list_add(messenger->blocked, link);
    }
  }

  if (n != PN_EOS) {
    return pn_error_format(messenger->error, n, "PN_EOS expected");
  }
  pn_buffer_append(buf, encoded, pending); // XXX

  return 0;
}

void pni_messenger_reclaim_link(pn_messenger_t *messenger, pn_link_t *link)
{
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
      if (pn_delivery_buffered(d)) {
        pni_entry_set_status(e, PN_STATUS_ABORTED);
      }
    }
    d = pn_unsettled_next(d);
  }

  link_ctx_release(messenger, link);
}

void pni_messenger_reclaim(pn_messenger_t *messenger, pn_connection_t *conn)
{
  if (!conn) return;

  pn_link_t *link = pn_link_head(conn, 0);
  while (link) {
    pni_messenger_reclaim_link(messenger, link);
    link = pn_link_next(link, 0);
  }

  pn_list_remove(messenger->connections, conn);
  pn_connection_ctx_free(conn);
  pn_transport_free(pn_connection_transport(conn));
  pn_connection_free(conn);
}

pn_connection_t *pn_messenger_connection(pn_messenger_t *messenger,
                                         pn_socket_t sock,
                                         const char *scheme,
                                         char *user,
                                         char *pass,
                                         char *host,
                                         char *port,
                                         pn_listener_ctx_t *lnr)
{
  pn_connection_t *connection = pn_connection();
  if (!connection) return NULL;
  pn_connection_collect(connection, messenger->collector);
  pn_connection_ctx(messenger, connection, sock, scheme, user, pass, host, port, lnr);

  pn_connection_set_container(connection, messenger->name);
  pn_connection_set_hostname(connection, host);
  pn_connection_set_user(connection, user);
  pn_connection_set_password(connection, pass);

  pn_list_add(messenger->connections, connection);

  return connection;
}

void pn_messenger_process_connection(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_connection_t *conn = pn_event_connection(event);
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);

  if (pn_connection_state(conn) & PN_LOCAL_UNINIT) {
    pn_connection_open(conn);
  }

  if (pn_connection_state(conn) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)) {
    pn_condition_t *condition = pn_connection_remote_condition(conn);
    pn_condition_report("CONNECTION", condition);
    pn_connection_close(conn);
    if (pn_condition_is_redirect(condition)) {
      const char *host = pn_condition_redirect_host(condition);
      char buf[1024];
      sprintf(buf, "%i", pn_condition_redirect_port(condition));

      pn_close(messenger->io, pn_selectable_get_fd(ctx->selectable));
      pn_socket_t sock = pn_connect(messenger->io, host, buf);
      pn_selectable_set_fd(ctx->selectable, sock);
      pn_transport_unbind(pn_connection_transport(conn));
      pn_connection_reset(conn);
      pn_transport_t *t = pn_transport();
      if (messenger->flags & PN_FLAGS_ALLOW_INSECURE_MECHS &&
          messenger->address.user && messenger->address.pass) {
        pn_sasl_t *s = pn_sasl(t);
        pn_sasl_set_allow_insecure_mechs(s, true);
      }
      pn_transport_bind(t, conn);
      pn_decref(t);
      pn_transport_config(messenger, conn);
    }
  }
}

void pn_messenger_process_session(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_session_t *ssn = pn_event_session(event);

  if (pn_session_state(ssn) & PN_LOCAL_UNINIT) {
    pn_session_open(ssn);
  }

  if (pn_session_state(ssn) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)) {
    pn_session_close(ssn);
  }
}

void pn_messenger_process_link(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_link_t *link = pn_event_link(event);
  pn_connection_t *conn = pn_event_connection(event);
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);

  if (pn_link_state(link) & PN_LOCAL_UNINIT) {
    pn_terminus_copy(pn_link_source(link), pn_link_remote_source(link));
    pn_terminus_copy(pn_link_target(link), pn_link_remote_target(link));
    link_ctx_setup( messenger, conn, link );
    pn_link_open(link);
    if (pn_link_is_receiver(link)) {
      pn_listener_ctx_t *lnr = ctx->listener;
      ((pn_link_ctx_t *)pn_link_get_context(link))->subscription = lnr ? lnr->subscription : NULL;
    }
  }

  if (pn_link_state(link) & PN_REMOTE_ACTIVE) {
    pn_link_ctx_t *ctx = (pn_link_ctx_t *) pn_link_get_context(link);
    if (ctx) {
      const char *addr = pn_terminus_get_address(pn_link_remote_source(link));
      if (ctx->subscription) {
        pni_subscription_set_address(ctx->subscription, addr);
      }
    }
  }

  if (pn_link_state(link) & PN_REMOTE_CLOSED) {
    if (PN_LOCAL_ACTIVE & pn_link_state(link)) {
      pn_condition_report("LINK", pn_link_remote_condition(link));
      pn_link_close(link);
      pni_messenger_reclaim_link(messenger, link);
      pn_link_free(link);
    }
  }
}

int pni_pump_out(pn_messenger_t *messenger, const char *address, pn_link_t *sender);

void pn_messenger_process_flow(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_link_t *link = pn_event_link(event);

  if (pn_link_is_sender(link)) {
    pni_pump_out(messenger, pn_terminus_get_address(pn_link_target(link)), link);
  } else {
    // account for any credit left over after draining links has completed
    if (pn_link_get_drain(link)) {
      if (!pn_link_draining(link)) {
        // drain completed!
        int drained = pn_link_drained(link);
        messenger->distributed -= drained;
        messenger->credit += drained;
        pn_link_set_drain(link, false);
        messenger->draining--;
        pn_list_remove(messenger->credited, link);
        pn_list_add(messenger->blocked, link);
      }
    }
  }
}

void pn_messenger_process_delivery(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_delivery_t *d = pn_event_delivery(event);
  pn_link_t *link = pn_event_link(event);
  if (pn_delivery_updated(d)) {
    if (pn_link_is_sender(link)) {
      pn_delivery_update(d, pn_delivery_remote_state(d));
    }
    pni_entry_t *e = (pni_entry_t *) pn_delivery_get_context(d);
    if (e) pni_entry_updated(e);
  }
  pn_delivery_clear(d);
  if (pn_delivery_readable(d)) {
    int err = pni_pump_in(messenger, pn_terminus_get_address(pn_link_source(link)), link);
    if (err) {
      pn_logf("%s", pn_error_text(messenger->error));
    }
  }
}

void pn_messenger_process_transport(pn_messenger_t *messenger, pn_event_t *event)
{
  pn_connection_t *conn = pn_event_connection(event);
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(conn);
  if (ctx) {
    pni_conn_modified(ctx);
  }
}

int pn_messenger_process_events(pn_messenger_t *messenger)
{
  int processed = 0;
  pn_event_t *event;
  while ((event = pn_collector_peek(messenger->collector))) {
    processed++;
    switch (pn_event_type(event)) {
    case PN_CONNECTION_INIT:
      pn_logf("connection created: %p", (void *) pn_event_connection(event));
      break;
    case PN_SESSION_INIT:
      pn_logf("session created: %p", (void *) pn_event_session(event));
      break;
    case PN_LINK_INIT:
      pn_logf("link created: %p", (void *) pn_event_link(event));
      break;
    case PN_CONNECTION_REMOTE_OPEN:
    case PN_CONNECTION_REMOTE_CLOSE:
    case PN_CONNECTION_LOCAL_OPEN:
    case PN_CONNECTION_LOCAL_CLOSE:
      pn_messenger_process_connection(messenger, event);
      break;
    case PN_SESSION_REMOTE_OPEN:
    case PN_SESSION_REMOTE_CLOSE:
    case PN_SESSION_LOCAL_OPEN:
    case PN_SESSION_LOCAL_CLOSE:
      pn_messenger_process_session(messenger, event);
      break;
    case PN_LINK_REMOTE_OPEN:
    case PN_LINK_REMOTE_CLOSE:
    case PN_LINK_REMOTE_DETACH:
    case PN_LINK_LOCAL_OPEN:
    case PN_LINK_LOCAL_CLOSE:
    case PN_LINK_LOCAL_DETACH:
      pn_messenger_process_link(messenger, event);
      break;
    case PN_LINK_FLOW:
      pn_messenger_process_flow(messenger, event);
      break;
    case PN_DELIVERY:
      pn_messenger_process_delivery(messenger, event);
      break;
    case PN_TRANSPORT:
    case PN_TRANSPORT_ERROR:
    case PN_TRANSPORT_HEAD_CLOSED:
    case PN_TRANSPORT_TAIL_CLOSED:
    case PN_TRANSPORT_CLOSED:
      pn_messenger_process_transport(messenger, event);
      break;
    case PN_EVENT_NONE:
      break;
    case PN_CONNECTION_BOUND:
      break;
    case PN_CONNECTION_UNBOUND:
      break;
    case PN_CONNECTION_FINAL:
      break;
    case PN_SESSION_FINAL:
      break;
    case PN_LINK_FINAL:
      break;
    default:
      break;
    }
    pn_collector_pop(messenger->collector);
  }

  return processed;
}

/**
 * Function to invoke AMQP related timer events, such as a heartbeat to prevent
 * remote_idle timeout events
 */
static void pni_messenger_tick(pn_messenger_t *messenger)
{
  for (size_t i = 0; i < pn_list_size(messenger->connections); i++) {
    pn_connection_t *connection =
        (pn_connection_t *)pn_list_get(messenger->connections, i);
    pn_transport_t *transport = pn_connection_transport(connection);
    if (transport) {
      pn_transport_tick(transport, pn_i_now());

      // if there is pending data, such as an empty heartbeat frame, call
      // process events. This should kick off the chain of selectables for
      // reading/writing.
      ssize_t pending = pn_transport_pending(transport);
      if (pending > 0) {
        pn_connection_ctx_t *cctx =
            (pn_connection_ctx_t *)pn_connection_get_context(connection);
        pn_messenger_process_events(messenger);
        pn_messenger_flow(messenger);
        pni_conn_modified(pni_context(cctx->selectable));
      }
    }
  }
}

int pn_messenger_process(pn_messenger_t *messenger)
{
  bool doMessengerTick = true;
  pn_selectable_t *sel;
  int events;
  while ((sel = pn_selector_next(messenger->selector, &events))) {
    if (events & PN_READABLE) {
      pn_selectable_readable(sel);
    }
    if (events & PN_WRITABLE) {
      pn_selectable_writable(sel);
      doMessengerTick = false;
    }
    if (events & PN_EXPIRED) {
      pn_selectable_expired(sel);
    }
    if (events & PN_ERROR) {
      pn_selectable_error(sel);
    }
  }
  // ensure timer events are processed. Cannot call this inside the while loop
  // as the timer events are not seen by the selector
  if (doMessengerTick) {
    pni_messenger_tick(messenger);
  }
  if (messenger->interrupted) {
    messenger->interrupted = false;
    return PN_INTR;
  } else {
    return 0;
  }
}

pn_timestamp_t pn_messenger_deadline(pn_messenger_t *messenger)
{
  // If the scheduler detects credit imbalance on the links, wake up
  // in time to service credit drain
  return messenger->next_drain;
}

int pni_wait(pn_messenger_t *messenger, int timeout)
{
  bool wake = false;
  pn_selectable_t *sel;
  while ((sel = pn_messenger_selectable(messenger))) {
    if (pn_selectable_is_terminal(sel)) {
      if (pn_selectable_is_registered(sel)) {
        pn_selector_remove(messenger->selector, sel);
      }
      pn_selectable_free(sel);
      // we can't wait if we end up freeing anything because we could
      // be waiting on the stopped predicate which might become true
      // as a result of the free
      wake = true;
    } else if (pn_selectable_is_registered(sel)) {
      pn_selector_update(messenger->selector, sel);
    } else {
      pn_selector_add(messenger->selector, sel);
      pn_selectable_set_registered(sel, true);
    }
  }

  if (wake) return 0;

  return pn_selector_select(messenger->selector, timeout);
}

int pn_messenger_tsync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *), int timeout)
{
  if (messenger->passive) {
    bool pred = predicate(messenger);
    return pred ? 0 : PN_INPROGRESS;
  }

  pn_timestamp_t now = pn_i_now();
  long int deadline = now + timeout;
  bool pred;

  while (true) {
    int error = pn_messenger_process(messenger);
    pred = predicate(messenger);
    if (error == PN_INTR) {
      return pred ? 0 : PN_INTR;
    }
    int remaining = deadline - now;
    if (pred || (timeout >= 0 && remaining < 0)) break;

    pn_timestamp_t mdeadline = pn_messenger_deadline(messenger);
    if (mdeadline) {
      if (now >= mdeadline)
        remaining = 0;
      else {
        const int delay = mdeadline - now;
        remaining = (remaining < 0) ? delay : pn_min( remaining, delay );
      }
    }
    error = pni_wait(messenger, remaining);
    if (error) return error;

    if (timeout >= 0) {
      now = pn_i_now();
    }
  }

  return pred ? 0 : PN_TIMEOUT;
}

int pn_messenger_sync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *))
{
  if (messenger->blocking) {
    return pn_messenger_tsync(messenger, predicate, messenger->timeout);
  } else {
    int err = pn_messenger_tsync(messenger, predicate, 0);
    if (err == PN_TIMEOUT) {
      return PN_INPROGRESS;
    } else {
      return err;
    }
  }
}

static void pni_parse(pn_address_t *address);
pn_connection_t *pn_messenger_resolve(pn_messenger_t *messenger,
                                      const char *address, char **name);
int pn_messenger_work(pn_messenger_t *messenger, int timeout);

int pn_messenger_start(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;

  int error = 0;

  // When checking of routes is required we attempt to resolve each route
  // with a substitution that has a defined scheme, address and port. If
  // any of theses routes is invalid an appropriate error code will be
  // returned. Currently no attempt is made to check the name part of the
  // address, as the intent here is to fail fast if the addressed host
  // is invalid or unavailable.
  if (messenger->flags & PN_FLAGS_CHECK_ROUTES) {
    pn_list_t *substitutions = pn_list(PN_WEAKREF, 0);
    pn_transform_get_substitutions(messenger->routes, substitutions);
    for (size_t i = 0; i < pn_list_size(substitutions) && error == 0; i++) {
      pn_string_t *substitution = (pn_string_t *)pn_list_get(substitutions, i);
      if (substitution) {
        pn_address_t addr;
        addr.text = pn_string(NULL);
        error = pn_string_copy(addr.text, substitution);
        if (!error) {
          pni_parse(&addr);
          if (addr.scheme && strlen(addr.scheme) > 0 &&
              !strstr(addr.scheme, "$") && addr.host && strlen(addr.host) > 0 &&
              !strstr(addr.host, "$") && addr.port && strlen(addr.port) > 0 &&
              !strstr(addr.port, "$")) {
            pn_string_t *check_addr = pn_string(NULL);
            // ipv6 hosts need to be wrapped in [] within a URI
            if (strstr(addr.host, ":")) {
              pn_string_format(check_addr, "%s://[%s]:%s/", addr.scheme,
                               addr.host, addr.port);
            } else {
              pn_string_format(check_addr, "%s://%s:%s/", addr.scheme,
                               addr.host, addr.port);
            }
            char *name = NULL;
            pn_connection_t *connection = pn_messenger_resolve(
                messenger, pn_string_get(check_addr), &name);
            pn_free(check_addr);
            if (!connection) {
              if (pn_error_code(messenger->error) == 0)
                pn_error_copy(messenger->error, pn_io_error(messenger->io));
              pn_error_format(messenger->error, PN_ERR,
                              "CONNECTION ERROR (%s:%s): %s\n",
                              messenger->address.host, messenger->address.port,
                              pn_error_text(messenger->error));
              error = pn_error_code(messenger->error);
            } else {
              // Send and receive outstanding messages until connection
              // completes or an error occurs
              int work = pn_messenger_work(messenger, -1);
              pn_connection_ctx_t *cctx =
                  (pn_connection_ctx_t *)pn_connection_get_context(connection);
              while ((work > 0 ||
                      (pn_connection_state(connection) & PN_REMOTE_UNINIT) ||
                      pni_connection_pending(cctx->selectable) != (ssize_t)0) &&
                     pn_error_code(messenger->error) == 0)
                work = pn_messenger_work(messenger, 0);
              if (work < 0 && work != PN_TIMEOUT) {
                error = work;
              } else {
                error = pn_error_code(messenger->error);
              }
            }
          }
          pn_free(addr.text);
        }
      }
    }
    pn_free(substitutions);
  }

  return error;
}

bool pn_messenger_stopped(pn_messenger_t *messenger)
{
  return pn_list_size(messenger->connections) == 0 && pn_list_size(messenger->listeners) == 0;
}

int pn_messenger_stop(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;

  for (size_t i = 0; i < pn_list_size(messenger->connections); i++) {
    pn_connection_t *conn = (pn_connection_t *) pn_list_get(messenger->connections, i);
    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      pn_link_close(link);
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    pn_connection_close(conn);
  }

  for (size_t i = 0; i < pn_list_size(messenger->listeners); i++) {
    pn_listener_ctx_t *lnr = (pn_listener_ctx_t *) pn_list_get(messenger->listeners, i);
    pn_selectable_terminate(lnr->selectable);
    pni_lnr_modified(lnr);
  }

  return pn_messenger_sync(messenger, pn_messenger_stopped);
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
  pni_parse_url(pn_string_buffer(address->text), &address->scheme, &address->user,
            &address->pass, &address->host, &address->port, &address->name);
  if (address->host[0] == '~') {
    address->passive = true;
    address->host++;
  }
}

static int pni_route(pn_messenger_t *messenger, const char *address)
{
  pn_address_t *addr = &messenger->address;
  int err = pn_transform_apply(messenger->routes, address, addr->text);
  if (err) return pn_error_format(messenger->error, PN_ERR,
                                  "transformation error");
  pni_parse(addr);
  return 0;
}

pn_connection_t *pn_messenger_resolve(pn_messenger_t *messenger, const char *address, char **name)
{
  assert(messenger);
  messenger->connection_error = 0;
  pn_string_t *domain = messenger->domain;

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
    for (size_t i = 0; i < pn_list_size(messenger->listeners); i++) {
      pn_listener_ctx_t *ctx = (pn_listener_ctx_t *) pn_list_get(messenger->listeners, i);
      if (pn_streq(host, ctx->host) && pn_streq(port, ctx->port)) {
        return NULL;
      }
    }

    pn_listener_ctx(messenger, scheme, host, port);
    return NULL;
  }

  pn_string_set(domain, "");

  if (user) {
    pn_string_addf(domain, "%s@", user);
  }
  pn_string_addf(domain, "%s", host);
  if (port) {
    pn_string_addf(domain, ":%s", port);
  }

  for (size_t i = 0; i < pn_list_size(messenger->connections); i++) {
    pn_connection_t *connection = (pn_connection_t *) pn_list_get(messenger->connections, i);
    pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(connection);
    if (pn_streq(scheme, ctx->scheme) && pn_streq(user, ctx->user) &&
        pn_streq(pass, ctx->pass) && pn_streq(host, ctx->host) &&
        pn_streq(port, ctx->port)) {
      return connection;
    }
    const char *container = pn_connection_remote_container(connection);
    if (pn_streq(container, pn_string_get(domain))) {
      return connection;
    }
  }

  pn_socket_t sock = pn_connect(messenger->io, host, port ? port : default_port(scheme));
  if (sock == PN_INVALID_SOCKET) {
    pn_error_copy(messenger->error, pn_io_error(messenger->io));
    pn_error_format(messenger->error, PN_ERR, "CONNECTION ERROR (%s:%s): %s\n",
                    messenger->address.host, messenger->address.port,
                    pn_error_text(messenger->error));
    return NULL;
  }

  pn_connection_t *connection =
    pn_messenger_connection(messenger, sock, scheme, user, pass, host, port, NULL);
  pn_transport_t *transport = pn_transport();
  if (messenger->flags & PN_FLAGS_ALLOW_INSECURE_MECHS && user && pass) {
      pn_sasl_t *s = pn_sasl(transport);
      pn_sasl_set_allow_insecure_mechs(s, true);
  }
  pn_transport_bind(transport, connection);
  pn_decref(transport);
  pn_connection_ctx_t *ctx = (pn_connection_ctx_t *) pn_connection_get_context(connection);
  pn_selectable_t *sel = ctx->selectable;
  err = pn_transport_config(messenger, connection);
  if (err) {
    pn_selectable_free(sel);
    messenger->connection_error = err;
    return NULL;
  }

  pn_connection_open(connection);

  return connection;
}

pn_link_t *pn_messenger_get_link(pn_messenger_t *messenger,
                                           const char *address, bool sender)
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
  return NULL;
}

pn_link_t *pn_messenger_link(pn_messenger_t *messenger, const char *address,
                             bool sender, pn_seconds_t timeout)
{
  char *name = NULL;
  pn_connection_t *connection = pn_messenger_resolve(messenger, address, &name);
  if (!connection)
    return NULL;
  pn_connection_ctx_t *cctx =
      (pn_connection_ctx_t *)pn_connection_get_context(connection);

  pn_link_t *link = pn_messenger_get_link(messenger, address, sender);
  if (link)
    return link;

  pn_session_t *ssn = pn_session(connection);
  pn_session_open(ssn);
  if (sender) {
    link = pn_sender(ssn, "sender-xxx");
  } else {
    if (name) {
      link = pn_receiver(ssn, name);
    } else {
      link = pn_receiver(ssn, "");
    }
  }

  if ((sender && pn_messenger_get_outgoing_window(messenger)) ||
      (!sender && pn_messenger_get_incoming_window(messenger))) {
    if (messenger->snd_settle_mode == -1) { /* Choose default based on sender/receiver */
      /* For a sender use MIXED so the application can decide whether each
         message is settled or not. For a receiver request UNSETTLED, since the
         user set an incoming_window which means they want to decide settlement.
      */
      pn_link_set_snd_settle_mode(link, sender ? PN_SND_MIXED : PN_SND_UNSETTLED);
    } else {                    /* Respect user setting */
      pn_link_set_snd_settle_mode(link, (pn_snd_settle_mode_t)messenger->snd_settle_mode);
    }
    pn_link_set_rcv_settle_mode(link, messenger->rcv_settle_mode);
  }
  if (pn_streq(name, "#")) {
    if (pn_link_is_sender(link)) {
      pn_terminus_set_dynamic(pn_link_target(link), true);
    } else {
      pn_terminus_set_dynamic(pn_link_source(link), true);
    }
  } else {
    pn_terminus_set_address(pn_link_target(link), name);
    pn_terminus_set_address(pn_link_source(link), name);
  }
  link_ctx_setup( messenger, connection, link );

  if (timeout > 0) {
    pn_terminus_set_expiry_policy(pn_link_target(link), PN_EXPIRE_WITH_LINK);
    pn_terminus_set_expiry_policy(pn_link_source(link), PN_EXPIRE_WITH_LINK);
    pn_terminus_set_timeout(pn_link_target(link), timeout);
    pn_terminus_set_timeout(pn_link_source(link), timeout);
  }

  if (!sender) {
    pn_link_ctx_t *ctx = (pn_link_ctx_t *)pn_link_get_context(link);
    assert( ctx );
    ctx->subscription = pn_subscription(messenger, cctx->scheme, cctx->host,
                                        cctx->port);
  }
  pn_link_open(link);
  return link;
}

pn_link_t *pn_messenger_source(pn_messenger_t *messenger, const char *source,
                               pn_seconds_t timeout)
{
  return pn_messenger_link(messenger, source, false, timeout);
}

pn_link_t *pn_messenger_target(pn_messenger_t *messenger, const char *target,
                               pn_seconds_t timeout)
{
  return pn_messenger_link(messenger, target, true, timeout);
}

pn_subscription_t *pn_messenger_subscribe(pn_messenger_t *messenger, const char *source)
{
  return pn_messenger_subscribe_ttl(messenger, source, 0);
}

pn_subscription_t *pn_messenger_subscribe_ttl(pn_messenger_t *messenger,
                                              const char *source,
                                              pn_seconds_t timeout)
{
  pni_route(messenger, source);
  if (pn_error_code(messenger->error)) return NULL;

  bool passive = messenger->address.passive;
  char *scheme = messenger->address.scheme;
  char *host = messenger->address.host;
  char *port = messenger->address.port;

  if (passive) {
    pn_listener_ctx_t *ctx = pn_listener_ctx(messenger, scheme, host, port);
    if (ctx) {
      return ctx->subscription;
    } else {
      return NULL;
    }
  } else {
    pn_link_t *src = pn_messenger_source(messenger, source, timeout);
    if (!src) return NULL;
    pn_link_ctx_t *ctx = (pn_link_ctx_t *) pn_link_get_context( src );
    return ctx ? ctx->subscription : NULL;
  }
}

int pn_messenger_get_outgoing_window(pn_messenger_t *messenger)
{
  return pni_store_get_window(messenger->outgoing);
}

int pn_messenger_set_outgoing_window(pn_messenger_t *messenger, int window)
{
  pni_store_set_window(messenger->outgoing, window);
  return 0;
}

int pn_messenger_get_incoming_window(pn_messenger_t *messenger)
{
  return pni_store_get_window(messenger->incoming);
}

int pn_messenger_set_incoming_window(pn_messenger_t *messenger, int window)
{
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
  } else if (len == 1 && address[0] == '~') {
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

int pni_bump_out(pn_messenger_t *messenger, const char *address)
{
  pni_entry_t *entry = pni_store_get(messenger->outgoing, address);
  if (!entry) return 0;

  pni_entry_set_status(entry, PN_STATUS_ABORTED);
  pni_entry_free(entry);
  return 0;
}

int pni_pump_out(pn_messenger_t *messenger, const char *address, pn_link_t *sender)
{
  pni_entry_t *entry = pni_store_get(messenger->outgoing, address);
  if (!entry) {
    pn_link_drained(sender);
    return 0;
  }

  pn_buffer_t *buf = pni_entry_bytes(entry);
  pn_bytes_t bytes = pn_buffer_bytes(buf);
  const char *encoded = bytes.start;
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
    return 0;
  }
}

static void pni_default_rewrite(pn_messenger_t *messenger, const char *address,
                                pn_string_t *dst)
{
  pn_address_t *addr = &messenger->address;
  if (address && strstr(address, "@")) {
    int err = pn_string_set(addr->text, address);
    if (err) assert(false);
    pni_parse(addr);
    if (addr->user || addr->pass)
    {
      pn_string_format(messenger->rewritten, "%s%s%s%s%s%s%s",
                       addr->scheme ? addr->scheme : "",
                       addr->scheme ? "://" : "",
                       addr->host,
                       addr->port ? ":" : "",
                       addr->port ? addr->port : "",
                       addr->name ? "/" : "",
                       addr->name ? addr->name : "");
    }
  }
}

static void pni_rewrite(pn_messenger_t *messenger, pn_message_t *msg)
{
  const char *address = pn_message_get_address(msg);
  pn_string_set(messenger->original, address);

  int err = pn_transform_apply(messenger->rewrites, address,
                               messenger->rewritten);
  if (err) assert(false);
  if (!pn_transform_matched(messenger->rewrites)) {
    pni_default_rewrite(messenger, pn_string_get(messenger->rewritten),
                        messenger->rewritten);
  }
  pn_message_set_address(msg, pn_string_get(messenger->rewritten));
}

static void pni_restore(pn_messenger_t *messenger, pn_message_t *msg)
{
  pn_message_set_address(msg, pn_string_get(messenger->original));
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

  messenger->outgoing_tracker = pn_tracker(OUTGOING, pni_entry_track(entry));
  pn_buffer_t *buf = pni_entry_bytes(entry);

  pni_rewrite(messenger, msg);
  while (true) {
    char *encoded = pn_buffer_memory(buf).start;
    size_t size = pn_buffer_capacity(buf);
    int err = pn_message_encode(msg, encoded, &size);
    if (err == PN_OVERFLOW) {
      err = pn_buffer_ensure(buf, 2*pn_buffer_capacity(buf));
      if (err) {
        pni_entry_free(entry);
        pni_restore(messenger, msg);
        return pn_error_format(messenger->error, err, "put: error growing buffer");
      }
    } else if (err) {
      pni_restore(messenger, msg);
      return pn_error_format(messenger->error, err, "encode error: %s",
                             pn_message_error(msg));
    } else {
      pni_restore(messenger, msg);
      pn_buffer_append(buf, encoded, size); // XXX
      pn_link_t *sender = pn_messenger_target(messenger, address, 0);
      if (!sender) {
        int err = pn_error_code(messenger->error);
        if (err) {
          return err;
        } else if (messenger->connection_error) {
          return pni_bump_out(messenger, address);
        } else {
          return 0;
        }
      } else {
        return pni_pump_out(messenger, address, sender);
      }
    }
  }

  return PN_ERR;
}

pn_tracker_t pn_messenger_outgoing_tracker(pn_messenger_t *messenger)
{
  assert(messenger);
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
  pni_entry_t *e = pni_store_entry(store, pn_tracker_sequence(tracker));
  if (e) {
    return pni_entry_get_status(e);
  } else {
    return PN_STATUS_UNKNOWN;
  }
}

pn_delivery_t *pn_messenger_delivery(pn_messenger_t *messenger,
                                     pn_tracker_t tracker)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  pni_entry_t *e = pni_store_entry(store, pn_tracker_sequence(tracker));
  if (e) {
    return pni_entry_get_delivery(e);
  } else {
    return NULL;
  }
}

bool pn_messenger_buffered(pn_messenger_t *messenger, pn_tracker_t tracker)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  pni_entry_t *e = pni_store_entry(store, pn_tracker_sequence(tracker));
  if (e) {
    pn_delivery_t *d = pni_entry_get_delivery(e);
    if (d) {
      bool b = pn_delivery_buffered(d);
      return b;
    } else {
      return true;
    }
  } else {
    return false;
  }
}

int pn_messenger_settle(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  return pni_store_update(store, pn_tracker_sequence(tracker), PN_STATUS_UNKNOWN, flags, true, true);
}

// true if all pending output has been sent to peer
bool pn_messenger_sent(pn_messenger_t *messenger)
{
  int total = pni_store_size(messenger->outgoing);

  for (size_t i = 0; i < pn_list_size(messenger->connections); i++)
  {
    pn_connection_t *conn = (pn_connection_t *) pn_list_get(messenger->connections, i);
    // check if transport is done generating output
    pn_transport_t *transport = pn_connection_transport(conn);
    if (transport) {
      if (!pn_transport_quiesced(transport)) {
        return false;
      }
    }

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_sender(link)) {
        total += pn_link_queued(link);

        pn_delivery_t *d = pn_unsettled_head(link);
        while (d) {
          if (!pn_delivery_remote_state(d) && !pn_delivery_settled(d)) {
            total++;
          }
          d = pn_unsettled_next(d);
        }
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
  }

  return total <= messenger->send_threshold;
}

bool pn_messenger_rcvd(pn_messenger_t *messenger)
{
  if (pni_store_size(messenger->incoming) > 0) return true;

  for (size_t i = 0; i < pn_list_size(messenger->connections); i++)
  {
    pn_connection_t *conn = (pn_connection_t *) pn_list_get(messenger->connections, i);

    pn_delivery_t *d = pn_work_head(conn);
    while (d) {
      if (pn_delivery_readable(d) && !pn_delivery_partial(d)) {
        return true;
      }
      d = pn_work_next(d);
    }
  }

  if (!pn_list_size(messenger->connections) && !pn_list_size(messenger->listeners)) {
    return true;
  } else {
    return false;
  }
}

static bool work_pred(pn_messenger_t *messenger) {
  return messenger->worked;
}

int pn_messenger_work(pn_messenger_t *messenger, int timeout)
{
  messenger->worked = false;
  int err = pn_messenger_tsync(messenger, work_pred, timeout);
  if (err) {
    return err;
  }
  return (int) (messenger->worked ? 1 : 0);
}

int pni_messenger_work(pn_messenger_t *messenger)
{
  if (messenger->blocking) {
    return pn_messenger_work(messenger, messenger->timeout);
  } else {
    int err = pn_messenger_work(messenger, 0);
    if (err == PN_TIMEOUT) {
      return PN_INPROGRESS;
    } else {
      return err;
    }
  }
}

int pn_messenger_interrupt(pn_messenger_t *messenger)
{
  assert(messenger);
  ssize_t n = pn_write(messenger->io, messenger->ctrl[1], "x", 1);
  if (n <= 0) {
    return n;
  } else {
    return 0;
  }
}

int pn_messenger_send(pn_messenger_t *messenger, int n)
{
  if (n == -1) {
    messenger->send_threshold = 0;
  } else {
    messenger->send_threshold = pn_messenger_outgoing(messenger) - n;
    if (messenger->send_threshold < 0)
      messenger->send_threshold = 0;
  }
  return pn_messenger_sync(messenger, pn_messenger_sent);
}

int pn_messenger_recv(pn_messenger_t *messenger, int n)
{
  if (!messenger) return PN_ARG_ERR;
  if (messenger->blocking && !pn_list_size(messenger->listeners)
      && !pn_list_size(messenger->connections))
    return pn_error_format(messenger->error, PN_STATE_ERR, "no valid sources");

  // re-compute credit, and update credit scheduler
  if (n == -2) {
    messenger->credit_mode = LINK_CREDIT_MANUAL;
  } else if (n == -1) {
    messenger->credit_mode = LINK_CREDIT_AUTO;
  } else {
    messenger->credit_mode = LINK_CREDIT_EXPLICIT;
    if (n > messenger->distributed)
      messenger->credit = n - messenger->distributed;
    else  // cancel unallocated
      messenger->credit = 0;
  }
  pn_messenger_flow(messenger);
  int err = pn_messenger_sync(messenger, pn_messenger_rcvd);
  if (err) return err;
  if (!pn_messenger_incoming(messenger) &&
      messenger->blocking &&
      !pn_list_size(messenger->listeners) &&
      !pn_list_size(messenger->connections)) {
    return pn_error_format(messenger->error, PN_STATE_ERR, "no valid sources");
  } else {
    return 0;
  }
}

int pn_messenger_receiving(pn_messenger_t *messenger)
{
  assert(messenger);
  return messenger->credit + messenger->distributed;
}

int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;

  pni_entry_t *entry = pni_store_get(messenger->incoming, NULL);
  // XXX: need to drain credit before returning EOS
  if (!entry) return PN_EOS;

  messenger->incoming_tracker = pn_tracker(INCOMING, pni_entry_track(entry));
  pn_buffer_t *buf = pni_entry_bytes(entry);
  pn_bytes_t bytes = pn_buffer_bytes(buf);
  const char *encoded = bytes.start;
  size_t size = bytes.size;

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
  assert(messenger);
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
                          PN_STATUS_ACCEPTED, flags, false, false);
}

int pn_messenger_reject(pn_messenger_t *messenger, pn_tracker_t tracker, int flags)
{
  if (pn_tracker_direction(tracker) != INCOMING) {
    return pn_error_format(messenger->error, PN_ARG_ERR,
                           "invalid tracker, incoming tracker required");
  }

  return pni_store_update(messenger->incoming, pn_tracker_sequence(tracker),
                          PN_STATUS_REJECTED, flags, false, false);
}

pn_link_t *pn_messenger_tracker_link(pn_messenger_t *messenger,
                                               pn_tracker_t tracker)
{
  pni_store_t *store = pn_tracker_store(messenger, tracker);
  pni_entry_t *e = pni_store_entry(store, pn_tracker_sequence(tracker));
  if (e) {
    pn_delivery_t *d = pni_entry_get_delivery(e);
    if (d) {
      return pn_delivery_link(d);
    }
  }
  return NULL;
}

int pn_messenger_queued(pn_messenger_t *messenger, bool sender)
{
  if (!messenger) return 0;

  int result = 0;

  for (size_t i = 0; i < pn_list_size(messenger->connections); i++) {
    pn_connection_t *conn = (pn_connection_t *) pn_list_get(messenger->connections, i);

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
  pn_transform_rule(messenger->routes, pattern, address);
  return 0;
}

int pn_messenger_rewrite(pn_messenger_t *messenger, const char *pattern, const char *address)
{
  pn_transform_rule(messenger->rewrites, pattern, address);
  return 0;
}

int pn_messenger_set_flags(pn_messenger_t *messenger, const int flags)
{
  if (!messenger)
    return PN_ARG_ERR;
  if (flags == 0) {
    messenger->flags = 0;
  } else if (flags & (PN_FLAGS_CHECK_ROUTES | PN_FLAGS_ALLOW_INSECURE_MECHS)) {
    messenger->flags |= flags;
  } else {
    return PN_ARG_ERR;
  }
  return 0;
}

int pn_messenger_get_flags(pn_messenger_t *messenger)
{
  return messenger ? messenger->flags : 0;
}

int pn_messenger_set_snd_settle_mode(pn_messenger_t *messenger,
                                     const pn_snd_settle_mode_t mode)
{
  if (!messenger)
    return PN_ARG_ERR;
  messenger->snd_settle_mode = mode;
  return 0;
}

int pn_messenger_set_rcv_settle_mode(pn_messenger_t *messenger,
                                     const pn_rcv_settle_mode_t mode)
{
  if (!messenger)
    return PN_ARG_ERR;
  messenger->rcv_settle_mode = mode;
  return 0;
}

void pn_messenger_set_tracer(pn_messenger_t *messenger, pn_tracer_t tracer)
{
  assert(messenger);
  assert(tracer);

  messenger->tracer = tracer;
}

pn_millis_t pn_messenger_get_remote_idle_timeout(pn_messenger_t *messenger,
                                                 const char *address)
{
  if (!messenger)
    return PN_ARG_ERR;

  pn_address_t addr;
  addr.text = pn_string(address);
  pni_parse(&addr);

  pn_millis_t timeout = -1;
  for (size_t i = 0; i < pn_list_size(messenger->connections); i++) {
    pn_connection_t *connection =
        (pn_connection_t *)pn_list_get(messenger->connections, i);
    pn_connection_ctx_t *ctx =
        (pn_connection_ctx_t *)pn_connection_get_context(connection);
    if (pn_streq(addr.scheme, ctx->scheme) && pn_streq(addr.host, ctx->host) &&
        pn_streq(addr.port, ctx->port)) {
      pn_transport_t *transport = pn_connection_transport(connection);
      if (transport)
        timeout = pn_transport_get_remote_idle_timeout(transport);
      break;
    }
  }
  return timeout;
}

int
pn_messenger_set_ssl_peer_authentication_mode(pn_messenger_t *messenger,
                                              const pn_ssl_verify_mode_t mode)
{
  if (!messenger)
    return PN_ARG_ERR;
  messenger->ssl_peer_authentication_mode = mode;
  return 0;
}
