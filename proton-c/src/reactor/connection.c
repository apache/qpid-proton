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

#include <proton/connection.h>
#include <proton/object.h>
#include <proton/sasl.h>
#include <proton/ssl.h>
#include <proton/transport.h>
#include <proton/url.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "selectable.h"
#include "reactor.h"

// XXX: overloaded for both directions
PN_HANDLE(PN_TRANCTX)
PN_HANDLE(PNI_CONN_URL)

static pn_transport_t *pni_transport(pn_selectable_t *sel) {
  pn_record_t *record = pn_selectable_attachments(sel);
  return (pn_transport_t *) pn_record_get(record, PN_TRANCTX);
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

static ssize_t pni_connection_pending(pn_selectable_t *sel)
{
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
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  pn_timestamp_t deadline = pn_transport_tick(transport, pn_reactor_now(reactor));
  return deadline;
}

static void pni_connection_update(pn_selectable_t *sel) {
  ssize_t c = pni_connection_capacity(sel);
  ssize_t p = pni_connection_pending(sel);
  pn_selectable_set_reading(sel, c > 0);
  pn_selectable_set_writing(sel, p > 0);
  pn_selectable_set_deadline(sel, pni_connection_deadline(sel));
}

void pni_handle_transport(pn_reactor_t *reactor, pn_event_t *event) {
  assert(reactor);
  pn_transport_t *transport = pn_event_transport(event);
  pn_record_t *record = pn_transport_attachments(transport);
  pn_selectable_t *sel = (pn_selectable_t *) pn_record_get(record, PN_TRANCTX);
  if (sel && !pn_selectable_is_terminal(sel)) {
    pni_connection_update(sel);
    pn_reactor_update(reactor, sel);
  }
}

pn_selectable_t *pn_reactor_selectable_transport(pn_reactor_t *reactor, pn_socket_t sock, pn_transport_t *transport);

void pni_handle_open(pn_reactor_t *reactor, pn_event_t *event) {
  assert(reactor);
  assert(event);

  pn_connection_t *conn = pn_event_connection(event);
  if (!(pn_connection_state(conn) & PN_REMOTE_UNINIT)) {
    return;
  }

  pn_transport_t *transport = pn_transport();
  pn_transport_bind(transport, conn);
  pn_decref(transport);
}

void pni_handle_bound(pn_reactor_t *reactor, pn_event_t *event) {
  assert(reactor);
  assert(event);

  pn_connection_t *conn = pn_event_connection(event);
  pn_record_t *record = pn_connection_attachments(conn);
  pn_url_t *url = (pn_url_t *)pn_record_get(record, PNI_CONN_URL);
  const char *host = NULL;
  const char *port = "5672";
  pn_string_t *str = NULL;

  if (url) {
      host = pn_url_get_host(url);
      const char *uport = pn_url_get_port(url);
      if (uport) {
          port = uport;
      } else {
          const char *scheme = pn_url_get_scheme(url);
          if (scheme && strcmp(scheme, "amqps") == 0) {
              port = "5671";
          }
      }
      if (!pn_connection_get_user(conn)) {
          // user did not manually set auth info
          const char *user = pn_url_get_username(url);
          if (user) pn_connection_set_user(conn, user);
          const char *passwd = pn_url_get_password(url);
          if (passwd) pn_connection_set_password(conn, passwd);
      }
  } else {
      // for backward compatibility, see if the connection's hostname can be
      // used for the remote address.  See JIRA PROTON-1133
      const char *hostname = pn_connection_get_hostname(conn);
      if (hostname) {
          str = pn_string(hostname);
          char *h = pn_string_buffer(str);
          // see if a port has been included in the hostname.  This is not
          // allowed by the spec, but the old reactor interface allowed it.
          char *colon = strrchr(h, ':');
          if (colon) {
              *colon = '\0';
              host = h;
              port = colon + 1;
          }
      }
  }

  // host will be NULL if this connection was created via the acceptor, which
  // creates its own transport/socket when created.
  if (!host) {
      return;
  }

  pn_transport_t *transport = pn_event_transport(event);
  pn_socket_t sock = pn_connect(pn_reactor_io(reactor), host, port);
  // invalid sockets are ignored by poll, so we need to do this manualy
  if (sock == PN_INVALID_SOCKET) {
    pn_condition_t *cond = pn_transport_condition(transport);
    pn_condition_set_name(cond, "proton:io");
    pn_condition_set_description(cond, pn_error_text(pn_io_error(pn_reactor_io(reactor))));
    pn_transport_close_tail(transport);
    pn_transport_close_head(transport);
  }
  pn_free(str);
  pn_reactor_selectable_transport(reactor, sock, transport);
}

void pni_handle_final(pn_reactor_t *reactor, pn_event_t *event) {
  assert(reactor);
  assert(event);
  pn_connection_t *conn = pn_event_connection(event);
  pn_list_remove(pn_reactor_children(reactor), conn);
}

static void pni_connection_readable(pn_selectable_t *sel)
{
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  ssize_t capacity = pn_transport_capacity(transport);
  if (capacity > 0) {
    ssize_t n = pn_recv(pn_reactor_io(reactor), pn_selectable_get_fd(sel),
                        pn_transport_tail(transport), capacity);
    if (n <= 0) {
      if (n == 0 || !pn_wouldblock(pn_reactor_io(reactor))) {
        if (n < 0) {
          pn_condition_t *cond = pn_transport_condition(transport);
          pn_condition_set_name(cond, "proton:io");
          pn_condition_set_description(cond, pn_error_text(pn_io_error(pn_reactor_io(reactor))));
        }
        pn_transport_close_tail(transport);
      }
    } else {
      pn_transport_process(transport, (size_t)n);
    }
  }

  ssize_t newcap = pn_transport_capacity(transport);
  //occasionally transport events aren't generated when expected, so
  //the following hack ensures we always update the selector
  if (1 || newcap != capacity) {
    pni_connection_update(sel);
    pn_reactor_update(reactor, sel);
  }
}

static void pni_connection_writable(pn_selectable_t *sel)
{
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  ssize_t pending = pn_transport_pending(transport);
  if (pending > 0) {
    ssize_t n = pn_send(pn_reactor_io(reactor), pn_selectable_get_fd(sel),
                        pn_transport_head(transport), pending);
    if (n < 0) {
      if (!pn_wouldblock(pn_reactor_io(reactor))) {
        pn_condition_t *cond = pn_transport_condition(transport);
        if (!pn_condition_is_set(cond)) {
          pn_condition_set_name(cond, "proton:io");
          pn_condition_set_description(cond, pn_error_text(pn_io_error(pn_reactor_io(reactor))));
        }
        pn_transport_close_head(transport);
      }
    } else {
      pn_transport_pop(transport, n);
    }
  }

  ssize_t newpending = pn_transport_pending(transport);
  if (newpending != pending) {
    pni_connection_update(sel);
    pn_reactor_update(reactor, sel);
  }
}

static void pni_connection_error(pn_selectable_t *sel) {
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  pn_transport_close_head(transport);
  pn_transport_close_tail(transport);
  pn_selectable_terminate(sel);
  pn_reactor_update(reactor, sel);
}

static void pni_connection_expired(pn_selectable_t *sel) {
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  pn_timestamp_t deadline = pn_transport_tick(transport, pn_reactor_now(reactor));
  pn_selectable_set_deadline(sel, deadline);
  ssize_t c = pni_connection_capacity(sel);
  ssize_t p = pni_connection_pending(sel);
  pn_selectable_set_reading(sel, c > 0);
  pn_selectable_set_writing(sel, p > 0);
  pn_reactor_update(reactor, sel);
}

static void pni_connection_finalize(pn_selectable_t *sel) {
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  pn_transport_t *transport = pni_transport(sel);
  pn_record_t *record = pn_transport_attachments(transport);
  pn_record_set(record, PN_TRANCTX, NULL);
  pn_socket_t fd = pn_selectable_get_fd(sel);
  pn_close(pn_reactor_io(reactor), fd);
}

pn_selectable_t *pn_reactor_selectable_transport(pn_reactor_t *reactor, pn_socket_t sock, pn_transport_t *transport) {
  pn_selectable_t *sel = pn_reactor_selectable(reactor);
  pn_selectable_set_fd(sel, sock);
  pn_selectable_on_readable(sel, pni_connection_readable);
  pn_selectable_on_writable(sel, pni_connection_writable);
  pn_selectable_on_error(sel, pni_connection_error);
  pn_selectable_on_expired(sel, pni_connection_expired);
  pn_selectable_on_finalize(sel, pni_connection_finalize);
  pn_record_t *record = pn_selectable_attachments(sel);
  pn_record_def(record, PN_TRANCTX, PN_OBJECT);
  pn_record_set(record, PN_TRANCTX, transport);
  pn_record_t *tr = pn_transport_attachments(transport);
  pn_record_def(tr, PN_TRANCTX, PN_WEAKREF);
  pn_record_set(tr, PN_TRANCTX, sel);
  pni_record_init_reactor(tr, reactor);
  pni_connection_update(sel);
  pn_reactor_update(reactor, sel);
  return sel;
}

pn_connection_t *pn_reactor_connection(pn_reactor_t *reactor, pn_handler_t *handler) {
  assert(reactor);
  pn_connection_t *connection = pn_connection();
  pn_record_t *record = pn_connection_attachments(connection);
  pn_record_set_handler(record, handler);
  pn_connection_collect(connection, pn_reactor_collector(reactor));
  pn_list_add(pn_reactor_children(reactor), connection);
  pni_record_init_reactor(record, reactor);
  pn_decref(connection);
  return connection;
}

pn_connection_t *pn_reactor_connection_to_host(pn_reactor_t *reactor,
                                               const char *host,
                                               const char *port,
                                               pn_handler_t *handler) {
    pn_connection_t *connection = pn_reactor_connection(reactor, handler);
    pn_reactor_set_connection_host(reactor, connection, host, port);
    return connection;
}


void pn_reactor_set_connection_host(pn_reactor_t *reactor,
                                    pn_connection_t *connection,
                                    const char *host,
                                    const char *port)
{
    (void)reactor;  // ignored
    pn_url_t *url = pn_url();
    pn_url_set_host(url, host);
    pn_url_set_port(url, port);
    pn_record_t *record = pn_connection_attachments(connection);
    if (!pn_record_has(record, PNI_CONN_URL)) {
        pn_record_def(record, PNI_CONN_URL, PN_OBJECT);
    }
    pn_record_set(record, PNI_CONN_URL, url);
    pn_decref(url);
}


const char *pn_reactor_get_connection_address(pn_reactor_t *reactor,
                                              pn_connection_t *connection)
{
    (void)reactor;  // ignored
    if (!connection) return NULL;
    pn_record_t *record = pn_connection_attachments(connection);
    pn_url_t *url = (pn_url_t *)pn_record_get(record, PNI_CONN_URL);
    if (!url) return NULL;
    return pn_url_str(url);
}
