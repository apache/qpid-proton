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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include "util.h"

struct pn_messenger_t {
  char *name;
  char *certificate;
  char *private_key;
  char *password;
  char *trusted_certificates;
  int timeout;
  pn_driver_t *driver;
  int credit;
  uint64_t next_tag;
  pn_buffer_t *buffer;
  pn_error_t *error;
};

char *build_name(const char *name)
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
  pn_messenger_t *m = malloc(sizeof(pn_messenger_t));

  if (m) {
    m->name = build_name(name);
    m->certificate = NULL;
    m->private_key = NULL;
    m->password = NULL;
    m->trusted_certificates = NULL;
    m->timeout = -1;
    m->driver = pn_driver();
    m->credit = 0;
    m->next_tag = 0;
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
      messenger->credit += pn_link_credit(link);
    }
    link = pn_link_next(link, 0);
  }
}

long int millis(struct timeval tv)
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

    pn_driver_wait(messenger->driver, remaining);

    pn_listener_t *l;
    while ((l = pn_driver_listener(messenger->driver))) {
      char *scheme = pn_listener_context(l);
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
        pn_connection_free(conn);
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
    free(pn_listener_context(prev));
    pn_listener_free(prev);
  }

  return pn_messenger_sync(messenger, pn_messenger_stopped);
}

bool pn_streq(const char *a, const char *b)
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
  pn_connection_set_container(connection, messenger->name);
  pn_connection_set_hostname(connection, domain);
  pn_connection_open(connection);
  pn_connector_set_connection(connector, connection);

  return connection;
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
  if (sender) {
    pn_terminus_set_address(pn_link_target(link), name);
    pn_terminus_set_address(pn_link_source(link), name);
  } else {
    pn_terminus_set_address(pn_link_target(link), name);
    pn_terminus_set_address(pn_link_source(link), name);
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

int pn_messenger_subscribe(pn_messenger_t *messenger, const char *source)
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
                                     port ? port : default_port(scheme),
                                     pn_strdup(scheme));
    if (lnr) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s (%s)", source,
                             pn_driver_error(messenger->driver));
    }
  } else {
    pn_link_t *src = pn_messenger_source(messenger, source);
    if (src) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s (%s)", source,
                             pn_driver_error(messenger->driver));
    }
  }
}

static void outward_munge(pn_messenger_t *mng, pn_message_t *msg)
{
  const char *address = pn_message_get_reply_to(msg);
  int len = address ? strlen(address) : 0;
  if (len > 1 && address[0] == '~' && address[1] == '/') {
    char buf[len + strlen(mng->name) + 4];
    sprintf(buf, "amqp://%s/%s", mng->name, address);
    pn_message_set_reply_to(msg, buf);
  } else if (len == 0) {
    char buf[strlen(mng->name) + 4];
    sprintf(buf, "amqp://%s", mng->name);
    pn_message_set_reply_to(msg, buf);
  }
}

bool false_pred(pn_messenger_t *messenger) { return false; }

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
      *((uint32_t *) ptr) = next;
      pn_delivery(sender, pn_dtag(tag, 8));
      ssize_t n = pn_link_send(sender, encoded, size);
      if (n < 0) {
        return pn_error_format(messenger->error, n, "send error: %s",
                               pn_error_text(pn_link_error(sender)));
      } else {
        pn_link_advance(sender);
        pn_messenger_tsync(messenger, false_pred, 0);
        return 0;
      }
    }
  }

  return PN_ERR;
}

bool pn_messenger_sent(pn_messenger_t *messenger)
{
  pn_connector_t *ctor = pn_connector_head(messenger->driver);
  while (ctor) {
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_link_is_sender(link)) {
        pn_delivery_t *d = pn_unsettled_head(link);
        while (d) {
          if (pn_delivery_remote_state(d) || pn_delivery_settled(d)) {
            pn_delivery_settle(d);
          } else {
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
      if (pn_delivery_readable(d)) {
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
  messenger->credit += n;
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
      if (pn_delivery_readable(d)) {
        pn_link_t *l = pn_delivery_link(d);
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
        pn_delivery_update(d, PN_ACCEPTED);
        pn_delivery_settle(d);
        if (n != PN_EOS) {
          return pn_error_format(messenger->error, n, "PN_EOS expected");
        }
        if (msg) {
          int err = pn_message_decode(msg, encoded, pending);
          if (err) {
            return pn_error_format(messenger->error, err, "error decoding message: %s",
                                   pn_message_error(msg));
          } else {
            return 0;
          }
        } else {
          return 0;
        }
      }
      d = pn_work_next(d);
    }

    ctor = pn_connector_next(ctor);
  }

  // XXX: need to drain credit before returning EOS

  return PN_EOS;
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

