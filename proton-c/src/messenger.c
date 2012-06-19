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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"

struct pn_messenger_t {
  pn_driver_t *driver;
  pn_connector_t *connectors[1024];
  size_t size;
  size_t listeners;
  int credit;
  uint64_t next_tag;
  pn_error_t *error;
};

pn_messenger_t *pn_messenger()
{
  pn_messenger_t *m = malloc(sizeof(pn_messenger_t));

  if (m) {
    m->driver = pn_driver();
    m->size = 0;
    m->listeners = 0;
    m->credit = 0;
    m->next_tag = 0;
    m->error = pn_error();
  }

  return m;
}

void pn_messenger_free(pn_messenger_t *messenger)
{
  if (messenger) {
    pn_driver_destroy(messenger->driver);
    pn_error_free(messenger->error);
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

int pn_messenger_sync(pn_messenger_t *messenger, bool (*predicate)(pn_messenger_t *))
{
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_process(messenger->connectors[i]);
  }

  while (!predicate(messenger)) {
    pn_driver_wait(messenger->driver, -1);

    pn_listener_t *l;
    while ((l = pn_driver_listener(messenger->driver))) {
      pn_connector_t *c = pn_listener_accept(l);
      pn_sasl_t *sasl = pn_connector_sasl(c);
      pn_sasl_mechanisms(sasl, "ANONYMOUS");
      pn_sasl_server(sasl);
      pn_sasl_done(sasl, PN_SASL_OK);
      pn_connection_t *conn = pn_connection();
      pn_connection_set_container(conn, pn_listener_context(l));
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
            pn_connector_destroy(c);
            pn_messenger_reclaim(messenger, conn);
            pn_connection_destroy(conn);
            pn_messenger_flow(messenger);
            break;
          }
        }
      } else {
        pn_connector_process(c);
      }
    }
  }

  return 0;
}

bool pn_messenger_linked(pn_messenger_t *messenger)
{
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_state_t state = pn_connection_state(conn);
    if ((state == (PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT)) ||
        (state == (PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE))) {
      return false;
    }

    if (pn_link_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT) ||
        pn_link_head(conn, PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE)) {
      return false;
    }
  }

  return true;
}

int pn_messenger_start(pn_messenger_t *messenger)
{
  if (!messenger) return PN_ARG_ERR;
  return pn_messenger_sync(messenger, pn_messenger_linked);
}

bool pn_messenger_unlinked(pn_messenger_t *messenger)
{
  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);
    pn_state_t state = pn_connection_state(conn);
    if (state != (PN_LOCAL_CLOSED | PN_REMOTE_CLOSED))
      return false;
  }
  return true;
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

  return pn_messenger_sync(messenger, pn_messenger_unlinked);
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
  for (int i = 0; i < messenger->size; i++) {
    pn_connection_t *connection = pn_connector_connection(messenger->connectors[i]);
    const char *container = pn_connection_remote_container(connection);
    const char *hostname = pn_connection_hostname(connection);
    if (pn_streq(container, domain) || pn_streq(hostname, domain))
      return connection;
  }

  pn_connector_t *connector = pn_connector(messenger->driver, domain, "5672", NULL);
  if (!connector) return NULL;
  messenger->connectors[messenger->size++] = connector;
  pn_sasl_t *sasl = pn_connector_sasl(connector);
  pn_sasl_mechanisms(sasl, "ANONYMOUS");
  pn_sasl_client(sasl);
  pn_connection_t *connection = pn_connection();
  pn_connection_set_container(connection, domain);
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
  char *domain;
  char *name;
  parse_address(buf, &domain, &name);

  pn_listener_t *listener = pn_listener(messenger->driver, domain + 1, "5672", pn_strdup(domain + 1));
  if (listener) {
    messenger->listeners++;
  }
  return listener;
}

int pn_messenger_subscribe(pn_messenger_t *messenger, const char *source)
{
  if (strlen(source) >= 3 && source[0] == '/' && source[1] == '/' && source[2] == '~') {
    pn_listener_t *lnr = pn_messenger_isource(messenger, source);
    if (lnr) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s", source);
    }
  } else {
    pn_link_t *src = pn_messenger_source(messenger, source);
    if (src) {
      return 0;
    } else {
      return pn_error_format(messenger->error, PN_ERR,
                             "unable to subscribe to source: %s", source);
    }
  }
}

int pn_messenger_put(pn_messenger_t *messenger, pn_message_t *msg)
{
  if (!messenger) return PN_ARG_ERR;
  if (!msg) return pn_error_set(messenger->error, PN_ARG_ERR, "null message");
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
    int err = pn_message_encode(msg, PN_AMQP, encoded, &size);
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
        return 0;
      }
    }
  }

  return PN_ERR;
}

bool pn_messenger_sent(pn_messenger_t *messenger)
{
  //  if (!pn_messenger_linked(messenger)) return false;

  for (int i = 0; i < messenger->size; i++) {
    pn_connector_t *ctor = messenger->connectors[i];
    pn_connection_t *conn = pn_connector_connection(ctor);

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_ACTIVE);
    while (link) {
      if (pn_is_sender(link)) {
        pn_delivery_t *d = pn_unsettled_head(link);
        while (d) {
          if (pn_remote_disp(d) || pn_remote_settled(d)) {
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
  //  if (!pn_messenger_linked(messenger)) return false;

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
        int err = pn_message_decode(msg, PN_AMQP, buf, n);
        if (err)
          return pn_error_format(messenger->error, err, "error decoding message: %s",
                                 pn_message_error(msg));
        else
          return 0;
      }
      d = pn_work_next(d);
    }
  }

  // XXX: need to drain credit before returning EOS

  return PN_EOS;
}

int pn_messenger_queued(pn_messenger_t *messenger, bool sender)
{
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

