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

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>

#include <proton/driver.h>
#include <proton/driver_extras.h>
#include <proton/error.h>
#include <proton/io.h>
#include <proton/sasl.h>
#include <proton/ssl.h>
#include <proton/object.h>
#include <proton/selector.h>
#include <proton/types.h>
#include "selectable.h"
#include "util.h"
#include "platform.h"

/*
 * This driver provides limited thread safety for some operations on pn_connector_t objects.
 *
 * These calls are: pn_connector_process(), pn_connector_activate(), pn_connector_activated(),
 * pn_connector_close(), and others that only touch the connection object, i.e.
 * pn_connector_context().  These calls provide limited safety in that simultaneous calls are
 * not allowed to the same pn_connector_t object.
 *
 * The application must call pn_driver_wakeup() and resume its wait loop logic if a call to
 * pn_wait() may have overlapped with any of the above calls that could affect a pn_wait()
 * outcome.
 */

/* Decls */

#define PN_SEL_RD (0x0001)
#define PN_SEL_WR (0x0002)

struct pn_driver_t {
  pn_error_t *error;
  pn_io_t *io;
  pn_selector_t *selector;
  pn_listener_t *listener_head;
  pn_listener_t *listener_tail;
  pn_listener_t *listener_next;
  pn_connector_t *connector_head;
  pn_connector_t *connector_tail;
  pn_listener_t *ready_listener_head;
  pn_listener_t *ready_listener_tail;
  pn_connector_t *ready_connector_head;
  pn_connector_t *ready_connector_tail;
  pn_selectable_t *ctrl_selectable;
  size_t listener_count;
  size_t connector_count;
  pn_socket_t ctrl[2]; //pipe for updating selectable status
  pn_trace_t trace;
};

typedef enum {LISTENER, CONNECTOR} sel_type_t;

struct pn_listener_t {
  sel_type_t type;
  pn_driver_t *driver;
  pn_listener_t *listener_next;
  pn_listener_t *listener_prev;
  pn_listener_t *ready_listener_next;
  pn_listener_t *ready_listener_prev;
  void *context;
  pn_selectable_t *selectable;
  bool pending;
  bool closed;
};

#define PN_NAME_MAX (256)

struct pn_connector_t {
  sel_type_t type;
  pn_driver_t *driver;
  pn_connector_t *connector_next;
  pn_connector_t *connector_prev;
  pn_connector_t *ready_connector_next;
  pn_connector_t *ready_connector_prev;
  char name[PN_NAME_MAX];
  pn_timestamp_t wakeup;
  pn_timestamp_t posted_wakeup;
  pn_connection_t *connection;
  pn_transport_t *transport;
  pn_sasl_t *sasl;
  pn_listener_t *listener;
  void *context;
  pn_selectable_t *selectable;
  int idx;
  int status;
  int posted_status;
  pn_trace_t trace;
  bool pending_tick;
  bool pending_read;
  bool pending_write;
  bool closed;
  bool input_done;
  bool output_done;
};

static void get_new_events(pn_driver_t *);

/* Impls */

// listener

static void driver_listener_readable(pn_selectable_t *sel)
{
  // do nothing
}

static void driver_listener_writable(pn_selectable_t *sel)
{
  // do nothing
}

static void driver_listener_expired(pn_selectable_t *sel)
{
  // do nothing
}

static ssize_t driver_listener_capacity(pn_selectable_t *sel)
{
  return 1;
}

static ssize_t driver_listener_pending(pn_selectable_t *sel)
{
  return 0;
}

static pn_timestamp_t driver_listener_deadline(pn_selectable_t *sel)
{
  return 0;
}

static void driver_listener_finalize(pn_selectable_t *sel)
{
  // do nothing
}


static void pn_driver_add_listener(pn_driver_t *d, pn_listener_t *l)
{
  if (!l->driver) return;
  LL_ADD(d, listener, l);
  l->driver = d;
  d->listener_count++;
  pn_selector_add(d->selector, l->selectable);
}

static void ready_listener_list_remove(pn_driver_t *d, pn_listener_t *l)
{
  LL_REMOVE(d, ready_listener, l);
  l->ready_listener_next = NULL;
  l->ready_listener_prev = NULL;
}

static void pn_driver_remove_listener(pn_driver_t *d, pn_listener_t *l)
{
  if (!l->driver) return;

  pn_selector_remove(d->selector, l->selectable);
  if (l == d->ready_listener_head || l->ready_listener_prev)
    ready_listener_list_remove(d, l);

  if (l == d->listener_next) {
    d->listener_next = l->listener_next;
  }
  LL_REMOVE(d, listener, l);
  l->driver = NULL;
  d->listener_count--;
}

pn_listener_t *pn_listener(pn_driver_t *driver, const char *host,
                           const char *port, void* context)
{
  if (!driver) return NULL;

  pn_socket_t sock = pn_listen(driver->io, host, port);

  if (sock == PN_INVALID_SOCKET) {
    return NULL;
  } else {
    pn_listener_t *l = pn_listener_fd(driver, sock, context);

    if (driver->trace & (PN_TRACE_FRM | PN_TRACE_RAW | PN_TRACE_DRV))
      fprintf(stderr, "Listening on %s:%s\n", host, port);
    return l;
  }
}

pn_listener_t *pn_listener_fd(pn_driver_t *driver, pn_socket_t fd, void *context)
{
  if (!driver) return NULL;

  pn_listener_t *l = (pn_listener_t *) malloc(sizeof(pn_listener_t));
  if (!l) return NULL;
  l->type = LISTENER;
  l->driver = driver;
  l->listener_next = NULL;
  l->listener_prev = NULL;
  l->ready_listener_next = NULL;
  l->ready_listener_prev = NULL;
  l->pending = false;
  l->closed = false;
  l->context = context;
  l->selectable = pni_selectable(driver_listener_capacity,
                                 driver_listener_pending,
                                 driver_listener_deadline,
                                 driver_listener_readable,
                                 driver_listener_writable,
                                 driver_listener_expired,
                                 driver_listener_finalize);
  pni_selectable_set_fd(l->selectable, fd);
  pni_selectable_set_context(l->selectable, l);
  pn_driver_add_listener(driver, l);
  return l;
}

pn_socket_t pn_listener_get_fd(pn_listener_t *listener)
{
  assert(listener);
  return pn_selectable_fd(listener->selectable);
}

pn_listener_t *pn_listener_head(pn_driver_t *driver)
{
  return driver ? driver->listener_head : NULL;
}

pn_listener_t *pn_listener_next(pn_listener_t *listener)
{
  return listener ? listener->listener_next : NULL;
}

void pn_listener_trace(pn_listener_t *l, pn_trace_t trace) {
  // XXX
}

void *pn_listener_context(pn_listener_t *l) {
  return l ? l->context : NULL;
}

void pn_listener_set_context(pn_listener_t *listener, void *context)
{
  assert(listener);
  listener->context = context;
}

pn_connector_t *pn_listener_accept(pn_listener_t *l)
{
  if (!l || !l->pending) return NULL;
  char name[PN_NAME_MAX];

  pn_socket_t sock = pn_accept(l->driver->io, pn_selectable_fd(l->selectable), name, PN_NAME_MAX);
  if (sock == PN_INVALID_SOCKET) {
    return NULL;
  } else {
    if (l->driver->trace & (PN_TRACE_FRM | PN_TRACE_RAW | PN_TRACE_DRV))
      fprintf(stderr, "Accepted from %s\n", name);
    pn_connector_t *c = pn_connector_fd(l->driver, sock, NULL);
    snprintf(c->name, PN_NAME_MAX, "%s", name);
    c->listener = l;
    return c;
  }
}

void pn_listener_close(pn_listener_t *l)
{
  if (!l) return;
  if (l->closed) return;

  pn_close(l->driver->io, pn_selectable_fd(l->selectable));
  l->closed = true;
}

void pn_listener_free(pn_listener_t *l)
{
  if (!l) return;

  if (l->driver) pn_driver_remove_listener(l->driver, l);
  pn_selectable_free(l->selectable);
  free(l);
}

// connector

static ssize_t driver_connection_capacity(pn_selectable_t *sel)
{
  pn_connector_t *c = (pn_connector_t *) pni_selectable_get_context(sel);
  return c->posted_status & PN_SEL_RD ? 1 : 0;
}

static ssize_t driver_connection_pending(pn_selectable_t *sel)
{
  pn_connector_t *c = (pn_connector_t *) pni_selectable_get_context(sel);
  return c->posted_status & PN_SEL_WR ? 1 : 0;
}

static pn_timestamp_t driver_connection_deadline(pn_selectable_t *sel)
{
  pn_connector_t *c = (pn_connector_t *) pni_selectable_get_context(sel);
  return c->posted_wakeup;
}

static void driver_connection_readable(pn_selectable_t *sel)
{
  // do nothing
}

static void driver_connection_writable(pn_selectable_t *sel)
{
  // do nothing
}

static void driver_connection_expired(pn_selectable_t *sel)
{
  // do nothing
}

static void driver_connection_finalize(pn_selectable_t *sel)
{
  // do nothing
}

static void pn_driver_add_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;
  LL_ADD(d, connector, c);
  c->driver = d;
  d->connector_count++;
  pn_selector_add(d->selector, c->selectable);
}

static void ready_connector_list_remove(pn_driver_t *d, pn_connector_t *c)
{
  LL_REMOVE(d, ready_connector, c);
  c->ready_connector_next = NULL;
  c->ready_connector_prev = NULL;
}

static void pn_driver_remove_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;

  pn_selector_remove(d->selector, c->selectable);
  if (c == d->ready_connector_head || c->ready_connector_prev)
    ready_connector_list_remove(d, c);

  LL_REMOVE(d, connector, c);
  c->driver = NULL;
  d->connector_count--;
}

pn_connector_t *pn_connector(pn_driver_t *driver, const char *host,
                             const char *port, void *context)
{
  if (!driver) return NULL;

  pn_socket_t sock = pn_connect(driver->io, host, port);
  pn_connector_t *c = pn_connector_fd(driver, sock, context);
  snprintf(c->name, PN_NAME_MAX, "%s:%s", host, port);
  if (driver->trace & (PN_TRACE_FRM | PN_TRACE_RAW | PN_TRACE_DRV))
    fprintf(stderr, "Connected to %s\n", c->name);
  return c;
}

pn_connector_t *pn_connector_fd(pn_driver_t *driver, pn_socket_t fd, void *context)
{
  if (!driver) return NULL;

  pn_connector_t *c = (pn_connector_t *) malloc(sizeof(pn_connector_t));
  if (!c) return NULL;
  c->type = CONNECTOR;
  c->driver = driver;
  c->connector_next = NULL;
  c->connector_prev = NULL;
  c->ready_connector_next = NULL;
  c->ready_connector_prev = NULL;
  c->pending_tick = false;
  c->pending_read = false;
  c->pending_write = false;
  c->name[0] = '\0';
  c->status = PN_SEL_RD | PN_SEL_WR;
  c->posted_status = -1;
  c->trace = driver->trace;
  c->closed = false;
  c->wakeup = 0;
  c->posted_wakeup = 0;
  c->connection = NULL;
  c->transport = pn_transport();
  c->sasl = pn_sasl(c->transport);
  c->input_done = false;
  c->output_done = false;
  c->context = context;
  c->listener = NULL;
  c->selectable = pni_selectable(driver_connection_capacity,
                                 driver_connection_pending,
                                 driver_connection_deadline,
                                 driver_connection_readable,
                                 driver_connection_writable,
                                 driver_connection_expired,
                                 driver_connection_finalize);
  pni_selectable_set_fd(c->selectable, fd);
  pni_selectable_set_context(c->selectable, c);
  pn_connector_trace(c, driver->trace);

  pn_driver_add_connector(driver, c);
  return c;
}

pn_socket_t pn_connector_get_fd(pn_connector_t *connector)
{
  assert(connector);
  return pn_selectable_fd(connector->selectable);
}

pn_connector_t *pn_connector_head(pn_driver_t *driver)
{
  return driver ? driver->connector_head : NULL;
}

pn_connector_t *pn_connector_next(pn_connector_t *connector)
{
  return connector ? connector->connector_next : NULL;
}

void pn_connector_trace(pn_connector_t *ctor, pn_trace_t trace)
{
  if (!ctor) return;
  ctor->trace = trace;
  if (ctor->transport) pn_transport_trace(ctor->transport, trace);
}

pn_sasl_t *pn_connector_sasl(pn_connector_t *ctor)
{
  return ctor ? ctor->sasl : NULL;
}

pn_transport_t *pn_connector_transport(pn_connector_t *ctor)
{
  return ctor ? ctor->transport : NULL;
}

void pn_connector_set_connection(pn_connector_t *ctor, pn_connection_t *connection)
{
  if (!ctor) return;
  if (ctor->connection) {
    pn_decref(ctor->connection);
    pn_transport_unbind(ctor->transport);
  }
  ctor->connection = connection;
  if (ctor->connection) {
    pn_incref(ctor->connection);
    pn_transport_bind(ctor->transport, connection);
  }
  if (ctor->transport) pn_transport_trace(ctor->transport, ctor->trace);
}

pn_connection_t *pn_connector_connection(pn_connector_t *ctor)
{
  return ctor ? ctor->connection : NULL;
}

void *pn_connector_context(pn_connector_t *ctor)
{
  return ctor ? ctor->context : NULL;
}

void pn_connector_set_context(pn_connector_t *ctor, void *context)
{
  if (!ctor) return;
  ctor->context = context;
}

const char *pn_connector_name(const pn_connector_t *ctor)
{
  if (!ctor) return 0;
  return ctor->name;
}

pn_listener_t *pn_connector_listener(pn_connector_t *ctor)
{
  return ctor ? ctor->listener : NULL;
}

void pn_connector_close(pn_connector_t *ctor)
{
  // XXX: should probably signal engine and callback here
  if (!ctor) return;

  ctor->status = 0;
  pn_close(ctor->driver->io, pn_selectable_fd(ctor->selectable));
  ctor->closed = true;
}

bool pn_connector_closed(pn_connector_t *ctor)
{
  return ctor ? ctor->closed : true;
}

void pn_connector_free(pn_connector_t *ctor)
{
  if (!ctor) return;

  if (ctor->driver) pn_driver_remove_connector(ctor->driver, ctor);
  pn_transport_free(ctor->transport);
  ctor->transport = NULL;
  if (ctor->connection) pn_decref(ctor->connection);
  ctor->connection = NULL;
  pn_selectable_free(ctor->selectable);
  free(ctor);
}

void pn_connector_activate(pn_connector_t *ctor, pn_activate_criteria_t crit)
{
    switch (crit) {
    case PN_CONNECTOR_WRITABLE :
        ctor->status |= PN_SEL_WR;
        break;

    case PN_CONNECTOR_READABLE :
        ctor->status |= PN_SEL_RD;
        break;
    }
}


bool pn_connector_activated(pn_connector_t *ctor, pn_activate_criteria_t crit)
{
    bool result = false;

    switch (crit) {
    case PN_CONNECTOR_WRITABLE :
        result = ctor->pending_write;
        ctor->pending_write = false;
        ctor->status &= ~PN_SEL_WR;
        break;

    case PN_CONNECTOR_READABLE :
        result = ctor->pending_read;
        ctor->pending_read = false;
        ctor->status &= ~PN_SEL_RD;
        break;
    }

    return result;
}

static pn_timestamp_t pn_connector_tick(pn_connector_t *ctor, pn_timestamp_t now)
{
  if (!ctor->transport) return 0;
  return pn_transport_tick(ctor->transport, now);
}

void pn_connector_process(pn_connector_t *c)
{
  if (c) {
    if (c->closed) return;

    pn_transport_t *transport = c->transport;
    pn_socket_t sock = pn_selectable_fd(c->selectable);

    ///
    /// Socket read
    ///
    if (!c->input_done) {
      ssize_t capacity = pn_transport_capacity(transport);
      if (capacity > 0) {
        c->status |= PN_SEL_RD;
        if (c->pending_read) {
          c->pending_read = false;
          ssize_t n =  pn_recv(c->driver->io, sock, pn_transport_tail(transport), capacity);
          if (n < 0) {
            if (errno != EAGAIN) {
              perror("read");
              c->status &= ~PN_SEL_RD;
              c->input_done = true;
              pn_transport_close_tail( transport );
            }
          } else if (n == 0) {
            c->status &= ~PN_SEL_RD;
            c->input_done = true;
            pn_transport_close_tail( transport );
          } else {
            if (pn_transport_process(transport, (size_t) n) < 0) {
              c->status &= ~PN_SEL_RD;
              c->input_done = true;
            }
          }
        }
      }

      capacity = pn_transport_capacity(transport);

      if (capacity < 0) {
        c->status &= ~PN_SEL_RD;
        c->input_done = true;
      }
    }

    ///
    /// Event wakeup
    ///
    c->wakeup = pn_connector_tick(c, pn_i_now());

    ///
    /// Socket write
    ///
    if (!c->output_done) {
      ssize_t pending = pn_transport_pending(transport);
      if (pending > 0) {
        c->status |= PN_SEL_WR;
        if (c->pending_write) {
          c->pending_write = false;
          ssize_t n = pn_send(c->driver->io, sock, pn_transport_head(transport), pending);
          if (n < 0) {
            // XXX
            if (errno != EAGAIN) {
              perror("send");
              c->output_done = true;
              c->status &= ~PN_SEL_WR;
              pn_transport_close_head( transport );
            }
          } else if (n) {
            pn_transport_pop(transport, (size_t) n);
          }
        }
      } else if (pending == 0) {
        c->status &= ~PN_SEL_WR;
      } else {
        c->output_done = true;
        c->status &= ~PN_SEL_WR;
      }
    }

    // Closed?

    if (c->input_done && c->output_done) {
      if (c->trace & (PN_TRACE_FRM | PN_TRACE_RAW | PN_TRACE_DRV)) {
        fprintf(stderr, "Closed %s\n", c->name);
      }
      pn_connector_close(c);
    }
  }
}

// driver

static pn_selectable_t *create_ctrl_selectable(pn_socket_t fd);

pn_driver_t *pn_driver()
{
  pn_driver_t *d = (pn_driver_t *) malloc(sizeof(pn_driver_t));
  if (!d) return NULL;

  d->error = pn_error();
  d->io = pn_io();
  d->selector = pn_io_selector(d->io);
  d->listener_head = NULL;
  d->listener_tail = NULL;
  d->listener_next = NULL;
  d->ready_listener_head = NULL;
  d->ready_listener_tail = NULL;
  d->connector_head = NULL;
  d->connector_tail = NULL;
  d->ready_connector_head = NULL;
  d->ready_connector_tail = NULL;
  d->listener_count = 0;
  d->connector_count = 0;
  d->ctrl[0] = 0;
  d->ctrl[1] = 0;
  d->trace = ((pn_env_bool("PN_TRACE_RAW") ? PN_TRACE_RAW : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_FRM") ? PN_TRACE_FRM : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_DRV") ? PN_TRACE_DRV : PN_TRACE_OFF));

  // XXX
  if (pn_pipe(d->io, d->ctrl)) {
    perror("Can't create control pipe");
    free(d);
    return NULL;
  }
  d->ctrl_selectable = create_ctrl_selectable(d->ctrl[0]);
  pn_selector_add(d->selector, d->ctrl_selectable);

  return d;
}

int pn_driver_errno(pn_driver_t *d)
{
  return d ? pn_error_code(d->error) : PN_ARG_ERR;
}

const char *pn_driver_error(pn_driver_t *d)
{
  return d ? pn_error_text(d->error) : NULL;
}

void pn_driver_trace(pn_driver_t *d, pn_trace_t trace)
{
  d->trace = trace;
}

void pn_driver_free(pn_driver_t *d)
{
  if (!d) return;

  pn_selectable_free(d->ctrl_selectable);
  pn_close(d->io, d->ctrl[0]);
  pn_close(d->io, d->ctrl[1]);
  while (d->connector_head)
    pn_connector_free(d->connector_head);
  while (d->listener_head)
    pn_listener_free(d->listener_head);
  pn_error_free(d->error);
  pn_io_free(d->io);
  free(d);
}

int pn_driver_wakeup(pn_driver_t *d)
{
  if (d) {
    ssize_t count = pn_write(d->io, d->ctrl[1], "x", 1);
    if (count <= 0) {
      return count;
    } else {
      return 0;
    }
  } else {
    return PN_ARG_ERR;
  }
}

void pn_driver_wait_1(pn_driver_t *d)
{
}

int pn_driver_wait_2(pn_driver_t *d, int timeout)
{
  // These lists will normally be empty
  while (d->ready_listener_head)
    ready_listener_list_remove(d, d->ready_listener_head);
  while (d->ready_connector_head)
    ready_connector_list_remove(d, d->ready_connector_head);
  pn_connector_t *c = d->connector_head;
  for (unsigned i = 0; i < d->connector_count; i++)
  {
    // Optimistically use a snapshot of the non-threadsafe vars.
    // If they are in flux, the app will guarantee progress with a pn_driver_wakeup().
    int current_status = c->status;
    pn_timestamp_t current_wakeup = c->wakeup;
    if (c->posted_status != current_status || c->posted_wakeup != current_wakeup) {
      c->posted_status = current_status;
      c->posted_wakeup = current_wakeup;
      pn_selector_update(c->driver->selector, c->selectable);
    }
    if (c->closed) {
      c->pending_read = false;
      c->pending_write = false;
      c->pending_tick = false;
      LL_ADD(d, ready_connector, c);
    }
    c = c->connector_next;
  }

  if (d->ready_connector_head)
    timeout = 0;   // We found closed connections

  int code = pn_selector_select(d->selector, timeout);
  if (code) {
    pn_error_set(d->error, code, "select");
    return -1;
  }
  get_new_events(d);
  return 0;
}

int pn_driver_wait_3(pn_driver_t *d)
{
  //  no-op with new selector/selectables
  return 0;
}


// XXX - pn_driver_wait has been divided into three internal functions as a
//       temporary workaround for a multi-threading problem.  A multi-threaded
//       application must hold a lock on parts 1 and 3, but not on part 2.
//       This temporary change, which is not reflected in the driver's API, allows
//       a multi-threaded application to use the three parts separately.
//
//       This workaround will eventually be replaced by a more elegant solution
//       to the problem.
//
int pn_driver_wait(pn_driver_t *d, int timeout)
{
    pn_driver_wait_1(d);
    int result = pn_driver_wait_2(d, timeout);
    if (result == -1)
        return pn_error_code(d->error);
    return pn_driver_wait_3(d);
}

static void get_new_events(pn_driver_t *d)
{
  bool woken = false;
  int events;
  pn_selectable_t *sel;
  while ((sel = pn_selector_next(d->selector, &events)) != NULL) {
    if (sel == d->ctrl_selectable) {
      woken = true;
      //clear the pipe
      char buffer[512];
      while (pn_read(d->io, d->ctrl[0], buffer, 512) == 512);
      continue;
    }

    void *ctx = pni_selectable_get_context(sel);
    sel_type_t *type = (sel_type_t *) ctx;
    if (*type == CONNECTOR) {
      pn_connector_t *c = (pn_connector_t *) ctx;
      if (!c->closed) {
        LL_ADD(d, ready_connector, c);
        c->pending_read = events & PN_READABLE;
        c->pending_write = events & PN_WRITABLE;
        c->pending_tick = events & PN_EXPIRED;
      }
    } else {
      pn_listener_t *l = (pn_listener_t *) ctx;
      LL_ADD(d, ready_listener, l);
      l->pending = events & PN_READABLE;
    }
  }
}

pn_listener_t *pn_driver_listener(pn_driver_t *d) {
  if (!d) return NULL;

  pn_listener_t *l = d->ready_listener_head;
  while (l) {
    ready_listener_list_remove(d, l);
    if (l->pending)
      return l;
    l = d->ready_listener_head;
  }
  return NULL;
}

pn_connector_t *pn_driver_connector(pn_driver_t *d) {
  if (!d) return NULL;

  pn_connector_t *c = d->ready_connector_head;
  while (c) {
    ready_connector_list_remove(d, c);
    if (c->closed || c->pending_read || c->pending_write || c->pending_tick) {
      return c;
    }
    c = d->ready_connector_head;
  }
  return NULL;
}

static pn_selectable_t *create_ctrl_selectable(pn_socket_t fd)
{
  // ctrl input only needs to know about read events, just like a listener.
  pn_selectable_t *sel = pni_selectable(driver_listener_capacity,
                                        driver_listener_pending,
                                        driver_listener_deadline,
                                        driver_listener_readable,
                                        driver_listener_writable,
                                        driver_listener_expired,
                                        driver_listener_finalize);
  pni_selectable_set_fd(sel, fd);
  return sel;
}
