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

/*
 * Copy of posix poll-based driver with minimal changes to use
 * select().  TODO: fully native implementaton with I/O completion
 * ports.
 *
 * This implementation comments out the posix max_fds arg to select
 * which has no meaning on windows.  The number of fd_set slots are
 * configured at compile time via FD_SETSIZE, chosen "large enough"
 * for the limited scalability of select() at the expense of
 * 2*N*sizeof(unsigned int) bytes per driver instance.  select (and
 * associated macros like FD_ZERO) are otherwise unaffected
 * performance-wise by increasing FD_SETSIZE.
 */

#define FD_SETSIZE 2048
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#define PN_WINAPI

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../platform.h"
#include <proton/io.h>
#include <proton/driver.h>
#include <proton/driver_extras.h>
#include <proton/error.h>
#include <proton/sasl.h>
#include <proton/ssl.h>
#include <proton/util.h>
#include "../util.h"
#include "../ssl/ssl-internal.h"

#include <proton/types.h>

/* Posix compatibility helpers */

static int pn_socket_pair(SOCKET sv[2]);
#define close(sock) closesocket(sock)
static int pn_i_error_from_errno_wrap(pn_error_t *error, const char *msg) {
  errno = WSAGetLastError();
  return pn_i_error_from_errno(error, msg);
}
#define pn_i_error_from_errno(e,m) pn_i_error_from_errno_wrap(e,m)

/* Decls */

#define PN_SEL_RD (0x0001)
#define PN_SEL_WR (0x0002)

struct pn_driver_t {
  pn_error_t *error;
  pn_io_t *io;
  pn_listener_t *listener_head;
  pn_listener_t *listener_tail;
  pn_listener_t *listener_next;
  pn_connector_t *connector_head;
  pn_connector_t *connector_tail;
  pn_connector_t *connector_next;
  size_t listener_count;
  size_t connector_count;
  size_t closed_count;
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  // int max_fds;
  bool overflow;
  pn_socket_t ctrl[2]; //pipe for updating selectable status

  pn_trace_t trace;
  pn_timestamp_t wakeup;
};

struct pn_listener_t {
  pn_driver_t *driver;
  pn_listener_t *listener_next;
  pn_listener_t *listener_prev;
  int idx;
  bool pending;
  pn_socket_t fd;
  bool closed;
  void *context;
};

#define PN_NAME_MAX (256)

struct pn_connector_t {
  pn_driver_t *driver;
  pn_connector_t *connector_next;
  pn_connector_t *connector_prev;
  char name[PN_NAME_MAX];
  int idx;
  bool pending_tick;
  bool pending_read;
  bool pending_write;
  pn_socket_t fd;
  int status;
  pn_trace_t trace;
  bool closed;
  pn_timestamp_t wakeup;
  pn_connection_t *connection;
  pn_transport_t *transport;
  pn_sasl_t *sasl;
  bool input_done;
  bool output_done;
  pn_listener_t *listener;
  void *context;
};

/* Impls */

// listener

static void pn_driver_add_listener(pn_driver_t *d, pn_listener_t *l)
{
  if (!l->driver) return;
  LL_ADD(d, listener, l);
  l->driver = d;
  d->listener_count++;
}

static void pn_driver_remove_listener(pn_driver_t *d, pn_listener_t *l)
{
  if (!l->driver) return;

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

  if (sock == INVALID_SOCKET) {
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
  l->driver = driver;
  l->listener_next = NULL;
  l->listener_prev = NULL;
  l->idx = 0;
  l->pending = false;
  l->fd = fd;
  l->closed = false;
  l->context = context;

  pn_driver_add_listener(driver, l);
  return l;
}

pn_socket_t pn_listener_get_fd(pn_listener_t *listener)
{
  assert(listener);
  return listener->fd;
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

  pn_socket_t sock = pn_accept(l->driver->io, l->fd, name, PN_NAME_MAX);
  if (sock == INVALID_SOCKET) {
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

  if (close(l->fd) == -1)
    perror("close");
  l->closed = true;
}

void pn_listener_free(pn_listener_t *l)
{
  if (!l) return;

  if (l->driver) pn_driver_remove_listener(l->driver, l);
  free(l);
}

// connector

static void pn_driver_add_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;
  LL_ADD(d, connector, c);
  c->driver = d;
  d->connector_count++;
}

static void pn_driver_remove_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;

  if (c == d->connector_next) {
    d->connector_next = c->connector_next;
  }

  LL_REMOVE(d, connector, c);
  c->driver = NULL;
  d->connector_count--;
  if (c->closed) {
    d->closed_count--;
  }
}

pn_connector_t *pn_connector(pn_driver_t *driver, const char *hostarg,
                             const char *port, void *context)
{
  if (!driver) return NULL;

  // convert "0.0.0.0" to "127.0.0.1" on Windows for outgoing sockets
  const char *host = strcmp("0.0.0.0", hostarg) ? hostarg : "127.0.0.1";

  pn_socket_t sock = pn_connect(driver->io, host, port);

  pn_connector_t *c = pn_connector_fd(driver, sock, context);
  snprintf(c->name, PN_NAME_MAX, "%s:%s", host, port);
  if (driver->trace & (PN_TRACE_FRM | PN_TRACE_RAW | PN_TRACE_DRV))
    fprintf(stderr, "Connected to %s\n", c->name);
  return c;
}

static void pn_connector_read(pn_connector_t *ctor);
static void pn_connector_write(pn_connector_t *ctor);

pn_connector_t *pn_connector_fd(pn_driver_t *driver, pn_socket_t fd, void *context)
{
  if (!driver) return NULL;

  pn_connector_t *c = (pn_connector_t *) malloc(sizeof(pn_connector_t));
  if (!c) return NULL;
  c->driver = driver;
  c->connector_next = NULL;
  c->connector_prev = NULL;
  c->pending_tick = false;
  c->pending_read = false;
  c->pending_write = false;
  c->name[0] = '\0';
  c->idx = 0;
  c->fd = fd;
  c->status = PN_SEL_RD | PN_SEL_WR;
  c->trace = driver->trace;
  c->closed = false;
  c->wakeup = 0;
  c->connection = NULL;
  c->transport = pn_transport();
  c->sasl = pn_sasl(c->transport);
  c->input_done = false;
  c->output_done = false;
  c->context = context;
  c->listener = NULL;

  pn_connector_trace(c, driver->trace);

  pn_driver_add_connector(driver, c);
  return c;
}

pn_socket_t pn_connector_get_fd(pn_connector_t *connector)
{
  assert(connector);
  return connector->fd;
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
  ctor->connection = connection;
  pn_transport_bind(ctor->transport, connection);
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
  if (close(ctor->fd) == -1)
    perror("close");
  ctor->closed = true;
  ctor->driver->closed_count++;
}

bool pn_connector_closed(pn_connector_t *ctor)
{
  return ctor ? ctor->closed : true;
}

void pn_connector_free(pn_connector_t *ctor)
{
  if (!ctor) return;

  if (ctor->driver) pn_driver_remove_connector(ctor->driver, ctor);
  ctor->connection = NULL;
  pn_transport_free(ctor->transport);
  ctor->transport = NULL;
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

    ///
    /// Socket read
    ///
    if (!c->input_done) {
      ssize_t capacity = pn_transport_capacity(transport);
      if (capacity > 0) {
        c->status |= PN_SEL_RD;
        if (c->pending_read) {
          c->pending_read = false;
          ssize_t n =  pn_recv(c->driver->io, c->fd, pn_transport_tail(transport), capacity);
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
          ssize_t n = pn_send(c->driver->io, c->fd, pn_transport_head(transport), pending);
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

pn_driver_t *pn_driver()
{
  pn_driver_t *d = (pn_driver_t *) malloc(sizeof(pn_driver_t));
  if (!d) return NULL;
  d->error = pn_error();
  d->io = pn_io();
  d->listener_head = NULL;
  d->listener_tail = NULL;
  d->listener_next = NULL;
  d->connector_head = NULL;
  d->connector_tail = NULL;
  d->connector_next = NULL;
  d->listener_count = 0;
  d->connector_count = 0;
  d->closed_count = 0;
  // d->max_fds = 0;
  d->ctrl[0] = 0;
  d->ctrl[1] = 0;
  d->trace = ((pn_env_bool("PN_TRACE_RAW") ? PN_TRACE_RAW : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_FRM") ? PN_TRACE_FRM : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_DRV") ? PN_TRACE_DRV : PN_TRACE_OFF));
  d->wakeup = 0;

  // XXX
  if (pn_socket_pair(d->ctrl)) {
    perror("Can't create control pipe");
    free(d);
    return NULL;
  }

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

  close(d->ctrl[0]);
  close(d->ctrl[1]);
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
      ssize_t count = send(d->ctrl[1], "x", 1, 0);
    if (count <= 0) {
      return count;
    } else {
      return 0;
    }
  } else {
    return PN_ARG_ERR;
  }
}

static void pn_driver_rebuild(pn_driver_t *d)
{
  d->wakeup = 0;
  d->overflow = false;
  int r_avail = FD_SETSIZE;
  int w_avail = FD_SETSIZE;
  // d->max_fds = -1;
  FD_ZERO(&d->readfds);
  FD_ZERO(&d->writefds);
  FD_ZERO(&d->exceptfds);

  FD_SET(d->ctrl[0], &d->readfds);
  // if (d->ctrl[0] > d->max_fds) d->max_fds = d->ctrl[0];

  pn_listener_t *l = d->listener_head;
  for (unsigned i = 0; i < d->listener_count; i++) {
    if (r_avail) {
      FD_SET(l->fd, &d->readfds);
      // if (l->fd > d->max_fds) d->max_fds = l->fd;
      r_avail--;
      l = l->listener_next;
    }
    else {
      d->overflow = true;
      break;
    }
  }

  pn_connector_t *c = d->connector_head;
  for (unsigned i = 0; i < d->connector_count; i++)
  {
    if (!c->closed) {
      FD_SET(c->fd, &d->exceptfds);
      d->wakeup = pn_timestamp_min(d->wakeup, c->wakeup);
      if (c->status & PN_SEL_RD) {
        if (r_avail) {
          FD_SET(c->fd, &d->readfds);
          r_avail--;
        }
        else {
          d->overflow = true;
          break;
        }
      }
      if (c->status & PN_SEL_WR) {
        if (w_avail) {
          FD_SET(c->fd, &d->writefds);
          w_avail--;
        }
        else {
          d->overflow = true;
          break;
        }
      }
      // if (c->fd > d->max_fds) d->max_fds = c->fd;
    }
    c = c->connector_next;
  }
}

void pn_driver_wait_1(pn_driver_t *d)
{
  pn_driver_rebuild(d);
}

int pn_driver_wait_2(pn_driver_t *d, int timeout)
{
  if (d->overflow)
      return pn_error_set(d->error, PN_ERR, "maximum driver sockets exceeded");
  if (d->wakeup) {
    pn_timestamp_t now = pn_i_now();
    if (now >= d->wakeup)
      timeout = 0;
    else
      timeout = (timeout < 0) ? d->wakeup-now : pn_min(timeout, d->wakeup - now);
  }

  struct timeval to = {0};
  struct timeval *to_arg = &to;
  // block only if (timeout == 0) and (closed_count == 0)
  if (d->closed_count == 0) {
    if (timeout > 0) {
      // convert millisecs to sec and usec:
      to.tv_sec = timeout/1000;
      to.tv_usec = (timeout - (to.tv_sec * 1000)) * 1000;
    }
    else if (timeout < 0) {
        to_arg = NULL;
    }
  }
  int nfds = select(/* d->max_fds */ 0, &d->readfds, &d->writefds, &d->exceptfds, to_arg);
  if (nfds == SOCKET_ERROR) {
    errno = WSAGetLastError();
    pn_i_error_from_errno(d->error, "select");
    return -1;
  }
  return 0;
}

int pn_driver_wait_3(pn_driver_t *d)
{
  bool woken = false;
  if (FD_ISSET(d->ctrl[0], &d->readfds)) {
    woken = true;
    //clear the pipe
    char buffer[512];
    while (recv(d->ctrl[0], buffer, 512, 0) == 512);
  }

  pn_listener_t *l = d->listener_head;
  while (l) {
    l->pending = (FD_ISSET(l->fd, &d->readfds));
    l = l->listener_next;
  }

  pn_timestamp_t now = pn_i_now();
  pn_connector_t *c = d->connector_head;
  while (c) {
    if (c->closed) {
      c->pending_read = false;
      c->pending_write = false;
      c->pending_tick = false;
    } else {
      c->pending_read = FD_ISSET(c->fd, &d->readfds);
      c->pending_write = FD_ISSET(c->fd, &d->writefds);
      c->pending_tick = (c->wakeup &&  c->wakeup <= now);
// Unlike Posix no distinction of POLLERR and POLLHUP
//      if (idx && d->fds[idx].revents & POLLERR)
//          pn_connector_close(c);
//      else if (idx && (d->fds[idx].revents & POLLHUP)) {
//          [...]
// Strategy, defer error to a recv or send if read or write pending.
// Otherwise proclaim the connection dead.
      if (!c->pending_read && !c->pending_write) {
        if (FD_ISSET(c->fd, &d->exceptfds)) {
          // can't defer error to a read or write, close now.
          // How to get WSAlastError() equivalent info?
          fprintf(stderr, "connector cleanup on unknown error %s\n", c->name);
          pn_connector_close(c);
        }
      }
    }
    c = c->connector_next;
  }

  d->listener_next = d->listener_head;
  d->connector_next = d->connector_head;

  return woken ? PN_INTR : 0;
}

//
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

pn_listener_t *pn_driver_listener(pn_driver_t *d) {
  if (!d) return NULL;

  while (d->listener_next) {
    pn_listener_t *l = d->listener_next;
    d->listener_next = l->listener_next;

    if (l->pending) {
      return l;
    }
  }

  return NULL;
}

pn_connector_t *pn_driver_connector(pn_driver_t *d) {
  if (!d) return NULL;

  while (d->connector_next) {
    pn_connector_t *c = d->connector_next;
    d->connector_next = c->connector_next;

    if (c->closed || c->pending_read || c->pending_write || c->pending_tick) {
      return c;
    }
  }

  return NULL;
}

static int pn_socket_pair (SOCKET sv[2]) {
  // no socketpair on windows.  provide pipe() semantics using sockets

  SOCKET sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == INVALID_SOCKET) {
    perror("socket");
    return -1;
  }

  BOOL b = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &b, sizeof(b)) == -1) {
    perror("setsockopt");
    closesocket(sock);
    return -1;
  }
  else {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      perror("bind");
      closesocket(sock);
      return -1;
    }
  }

  if (listen(sock, 50) == -1) {
    perror("listen");
    closesocket(sock);
    return -1;
  }

  if ((sv[1] = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto)) == INVALID_SOCKET) {
    perror("sock1");
    closesocket(sock);
    return -1;
  }
  else {
    struct sockaddr addr = {0};
    int l = sizeof(addr);
    if (getsockname(sock, &addr, &l) == -1) {
      perror("getsockname");
      closesocket(sock);
      return -1;
    }

    if (connect(sv[1], &addr, sizeof(addr)) == -1) {
      int err = WSAGetLastError();
      fprintf(stderr, "connect wsaerrr %d\n", err);
      closesocket(sock);
      closesocket(sv[1]);
      return -1;
    }

    if ((sv[0] = accept(sock, &addr, &l)) == INVALID_SOCKET) {
      perror("accept");
      closesocket(sock);
      closesocket(sv[1]);
      return -1;
    }
  }

  u_long v = 1;
  ioctlsocket (sv[0], FIONBIO, &v);
  ioctlsocket (sv[1], FIONBIO, &v);
  closesocket(sock);
  return 0;
}
