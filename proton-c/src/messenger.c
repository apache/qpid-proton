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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include "util.h"

struct pn_messenger_t {
  char *name;
  int timeout;
  pn_driver_t *driver;
  pn_connector_t *connectors[1024];
  size_t size;
  size_t listeners;
  int credit;
  uint64_t next_tag;
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
    m->timeout = -1;
    m->driver = pn_driver();
    m->size = 0;
    m->listeners = 0;
    m->credit = 0;
    m->next_tag = 0;
    m->error = pn_error();
  }

  return m;
}

const char *pn_messenger_name(pn_messenger_t *messenger)
{
  return messenger->name;
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
    pn_driver_free(messenger->driver);
    pn_error_free(messenger->error);
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
    for (int i = 0; i < messenger->size; i++) {
      pn_connector_t *ctor = messenger->connectors[i];
      pn_connection_t *conn = pn_connector_connection(ctor);

      pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
      while (link && messenger->credit > 0) {
        if (pn_is_receiver(link)) {
          pn_flow(link, 1);
          messenger->credit--;
        }
        link = pn_link_next(link, PN_LOCAL_ACTIVE);
      }
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
    pn_set_source(link, pn_remote_source(link));
    pn_set_target(link, pn_remote_target(link));
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
    if (pn_is_receiver(link) && pn_credit(link) > 0) {
      messenger->credit += pn_credit(link);
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
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_process(messenger->connectors[i]);
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
      pn_connector_t *c = pn_listener_accept(l);
      pn_sasl_t *sasl = pn_connector_sasl(c);
      pn_sasl_mechanisms(sasl, "ANONYMOUS");
      pn_sasl_server(sasl);
      pn_sasl_done(sasl, PN_SASL_OK);
      pn_connection_t *conn = pn_connection();
      pn_connection_set_container(conn, messenger->name);
      pn_connector_set_connection(c, conn);
      messenger->connectors[messenger->size++] = c;
    }

    pn_connector_t *c;
    while ((c = pn_driver_connector(messenger->driver))) {
      pn_connector_process(c);
      pn_connection_t *conn = pn_connector_connection(c);
      pn_messenger_endpoints(messenger, conn);
      if (pn_connector_closed(c)) {
        for (int i = 0; i < messenger->size; i++) {
          if (c == messenger->connectors[i]) {
            memmove(messenger->connectors + i, messenger->connectors + i + 1, messenger->size - i - 1);
            messenger->size--;
            pn_connector_free(c);
            pn_messenger_reclaim(messenger, conn);
            pn_connection_free(conn);
            pn_messenger_flow(messenger);
            break;
          }
        }
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
  return messenger->size == 0;
}

int pn_messenger_stop(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;

  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      pn_link_close(link);
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
    pn_connection_close(conn);
  }

  return pn_messenger_sync(messenger, pn_messenger_stopped);
}

static void parse_address(char *address, char **domain, char **name)
{
  *domain = NULL;
  *name = NULL;

  if (!address) return;
  if (address[0] == '/' && address[1] == '/') {
    *domain = address + 2;
    for (char *c = *domain; *c; c++) {
      if (*c == '/') {
        *c = '\0';
        *name = c + 1;
      }
    }
  } else {
    *name = address;
  }
}

bool pn_streq(const char *a, const char *b)
{
  return a == b || (a && b && !strcmp(a, b));
}

pn_connection_t *pn_messenger_domain(pn_messenger_t *messenger, const char *domain)
{
  char buf[strlen(domain) + 1];
  if (domain) {
    strcpy(buf, domain);
  } else {
    buf[0] = '\0';
  }
  char *user = NULL;
  char *pass = NULL;
  char *host = "0.0.0.0";
  char *port = "5672";
  parse_url(buf, &user, &pass, &host, &port);

  for (int i = 0; i < messenger->size; i++) {
    pn_connection_t *connection = pn_connector_connection(messenger->connectors[i]);
    const char *container = pn_connection_remote_container(connection);
    const char *hostname = pn_connection_hostname(connection);
    if (pn_streq(container, domain) || pn_streq(hostname, domain))
      return connection;
  }

  pn_connector_t *connector = pn_connector(messenger->driver, host, port, NULL);
  if (!connector) return NULL;
  messenger->connectors[messenger->size++] = connector;
  pn_sasl_t *sasl = pn_connector_sasl(connector);
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
  char buf[(address ? strlen(address) : 0) + 1];
  if (address) {
    strcpy(buf, address);
  } else {
    buf[0] = '\0';
  }
  char *domain;
  char *name;
  parse_address(address ? buf : NULL, &domain, &name);

  pn_connection_t *connection = pn_messenger_domain(messenger, domain);
  if (!connection) return NULL;

  pn_link_t *link = pn_link_head(connection, PN_LOCAL_ACTIVE);
  while (link) {
    if (pn_is_sender(link) == sender) {
      const char *terminus = pn_is_sender(link) ? pn_target(link) : pn_source(link);
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
    pn_set_target(link, name);
    pn_set_source(link, name);
  } else {
    pn_set_target(link, name);
    pn_set_source(link, name);
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

pn_listener_t *pn_messenger_isource(pn_messenger_t *messenger, const char *source)
{
  char buf[strlen(source) + 1];
  strcpy(buf, source);
  char *domain, *name;
  parse_address(buf, &domain, &name);
  char *user = NULL;
  char *pass = NULL;
  char *host = "0.0.0.0";
  char *port = "5672";
  parse_url(domain + 1, &user, &pass, &host, &port);

  pn_listener_t *listener = pn_listener(messenger->driver, host, port, NULL);
  if (listener) {
    messenger->listeners++;
  }
  return listener;
}

int pn_messenger_subscribe(pn_messenger_t *messenger, const char *source)
{
  int len = strlen(source);
  if (len >= 3 && source[0] == '/' && source[1] == '/' && source[2] == '~') {
    pn_listener_t *lnr = pn_messenger_isource(messenger, source);
    if (lnr) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s", source);
    }
  } else if (len >= 2 && source[0] == '/' && source[1] == '/') {
    pn_link_t *src = pn_messenger_source(messenger, source);
    if (src) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s", source);
    }
  } else {
    return 0;
  }
}

static void outward_munge(pn_messenger_t *mng, pn_message_t *msg)
{
  const char *address = pn_message_get_reply_to(msg);
  int len = address ? strlen(address) : 0;
  if (len > 0 && address[0] != '/') {
    char buf[len + strlen(mng->name) + 4];
    sprintf(buf, "//%s/%s", mng->name, address);
    pn_message_set_reply_to(msg, buf);
  } else if (len == 0) {
    char buf[strlen(mng->name) + 4];
    sprintf(buf, "//%s", mng->name);
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
                           "unable to send to address: %s", address);
  // XXX: proper tag
  char tag[8];
  void *ptr = &tag;
  uint64_t next = messenger->next_tag++;
  *((uint32_t *) ptr) = next;
  pn_delivery(sender, pn_dtag(tag, 8));
  size_t size = 1024;
  // XXX: max message size
  while (size < 16*1024) {
    char encoded[size];
    int err = pn_message_encode(msg, encoded, &size);
    if (err == PN_OVERFLOW) {
      size *= 2;
    } else if (err) {
      return err;
    } else {
      ssize_t n = pn_send(sender, encoded, size);
      if (n < 0) {
        return n;
      } else {
        pn_advance(sender);
        pn_messenger_tsync(messenger, false_pred, 0);
        return 0;
      }
    }
  }

  return PN_ERR;
}

bool pn_messenger_sent(pn_messenger_t *messenger)
{
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_is_sender(link)) {
        pn_delivery_t *d = pn_unsettled_head(link);
        while (d) {
          if (pn_remote_disposition(d) || pn_remote_settled(d)) {
            pn_settle(d);
          } else {
            return false;
          }
          d = pn_unsettled_next(d);
        }
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
  }

  return true;
}

bool pn_messenger_rcvd(pn_messenger_t *messenger)
{
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_delivery_t *d = pn_work_head(conn);
    while (d) {
      if (pn_readable(d)) {
        return true;
      }
      d = pn_work_next(d);
    }
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
  if (!messenger->listeners && !messenger->size)
    return pn_error_format(messenger->error, PN_STATE_ERR, "no valid sources");
  messenger->credit += n;
  pn_messenger_flow(messenger);
  return pn_messenger_sync(messenger, pn_messenger_rcvd);
}

int pn_messenger_get(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;

  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_delivery_t *d = pn_work_head(conn);
    while (d) {
      if (pn_readable(d)) {
        // XXX: buf size, eof, etc
        char buf[1024];
        pn_link_t *l = pn_link(d);
        ssize_t n = pn_recv(l, buf, 1024);
        pn_settle(d);
        if (n < 0) return n;
        if (msg) {
          int err = pn_message_decode(msg, buf, n);
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
  }

  // XXX: need to drain credit before returning EOS

  return PN_EOS;
}

int pn_messenger_queued(pn_messenger_t *messenger, bool sender)
{
  if (!messenger) return 0;

  int result = 0;

  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_is_sender(link)) {
        if (sender) {
          result += pn_queued(link);
        }
      } else if (!sender) {
        result += pn_queued(link);
      }
      link = pn_link_next(link, PN_LOCAL_ACTIVE);
    }
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

