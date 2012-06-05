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

#define _POSIX_C_SOURCE 1

#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <proton/driver.h>
#include <proton/sasl.h>
#include "util.h"


/* Decls */

#define PN_SEL_RD (0x0001)
#define PN_SEL_WR (0x0002)

struct pn_driver_t {
  pn_listener_t *listener_head;
  pn_listener_t *listener_tail;
  pn_listener_t *listener_next;
  pn_connector_t *connector_head;
  pn_connector_t *connector_tail;
  pn_connector_t *connector_next;
  size_t listener_count;
  size_t connector_count;
  size_t closed_count;
  size_t capacity;
  struct pollfd *fds;
  size_t nfds;
  int ctrl[2]; //pipe for updating selectable status
  pn_trace_t trace;
};

struct pn_listener_t {
  pn_driver_t *driver;
  pn_listener_t *next;
  pn_listener_t *prev;
  int idx;
  bool pending;
  int fd;
  void *context;
};

#define IO_BUF_SIZE (4*1024)

struct pn_connector_t {
  pn_driver_t *driver;
  pn_connector_t *next;
  pn_connector_t *prev;
  int idx;
  bool pending_tick;
  bool pending_read;
  bool pending_write;
  int fd;
  int status;
  pn_trace_t trace;
  bool closed;
  time_t wakeup;
  void (*read)(pn_connector_t *);
  void (*write) (pn_connector_t *);
  time_t (*tick)(pn_connector_t *sel, time_t now);
  size_t input_size;
  char input[IO_BUF_SIZE];
  bool input_eos;
  size_t output_size;
  char output[IO_BUF_SIZE];
  pn_sasl_t *sasl;
  pn_connection_t *connection;
  pn_transport_t *transport;
  ssize_t (*process_input)(pn_connector_t *);
  ssize_t (*process_output)(pn_connector_t *);
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
  LL_ADD(d->listener_head, d->listener_tail, l);
  l->driver = d;
  d->listener_count++;
}

static void pn_driver_remove_listener(pn_driver_t *d, pn_listener_t *l)
{
  if (!l->driver) return;

  if (l == d->listener_next) {
    d->listener_next = l->next;
  }

  LL_REMOVE(d->listener_head, d->listener_tail, l);
  l->driver = NULL;
  d->listener_count--;
}

pn_listener_t *pn_listener(pn_driver_t *driver, const char *host,
                           const char *port, void* context)
{
  if (!driver) return NULL;

  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1) {
    perror("socket");
    return NULL;
  }

  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
    perror("setsockopt");
    return NULL;
  }

  if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    perror("bind");
    freeaddrinfo(addr);
    return NULL;
  }

  freeaddrinfo(addr);

  if (listen(sock, 50) == -1) {
    perror("listen");
    return NULL;
  }

  pn_listener_t *l = pn_listener_fd(driver, sock, context);

  printf("Listening on %s:%s\n", host, port);
  return l;
}

pn_listener_t *pn_listener_fd(pn_driver_t *driver, int fd, void *context)
{
  if (!driver) return NULL;

  pn_listener_t *l = malloc(sizeof(pn_listener_t));
  if (!l) return NULL;
  l->driver = driver;
  l->next = NULL;
  l->prev = NULL;
  l->idx = 0;
  l->pending = false;
  l->fd = fd;
  l->context = context;

  pn_driver_add_listener(driver, l);
  return l;
}

void pn_listener_trace(pn_listener_t *l, pn_trace_t trace) {
  // XXX
}

void *pn_listener_context(pn_listener_t *l) {
  return l ? l->context : NULL;
}

static void pn_configure_sock(int sock) {
  // this would be nice, but doesn't appear to exist on linux
  /*
  int set = 1;
  if (!setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int))) {
    perror("setsockopt");
  };
  */
}

pn_connector_t *pn_listener_accept(pn_listener_t *l)
{
  if (!l || !l->pending) return NULL;

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen = sizeof(addr);
  int sock = accept(l->fd, (struct sockaddr *) &addr, &addrlen);
  if (sock == -1) {
    perror("accept");
    return NULL;
  } else {
    char host[1024], serv[64];
    int code;
    if ((code = getnameinfo((struct sockaddr *) &addr, addrlen, host, 1024, serv, 64, 0))) {
      fprintf(stderr, "getnameinfo: %s\n", gai_strerror(code));
      if (close(sock) == -1)
        perror("close");
      return NULL;
    } else {
      pn_configure_sock(sock);
      if (l->driver->trace & (PN_TRACE_FRM | PN_TRACE_RAW))
        printf("accepted from %s:%s\n", host, serv);
      pn_connector_t *c = pn_connector_fd(l->driver, sock, NULL);
      c->listener = l;
      return c;
    }
  }
}

void pn_listener_close(pn_listener_t *l)
{
  if (!l) return;

  if (close(l->fd) == -1)
    perror("close");
}

void pn_listener_destroy(pn_listener_t *l)
{
  if (!l) return;

  if (l->driver) pn_driver_remove_listener(l->driver, l);
  free(l);
}

// connector

static void pn_driver_add_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;
  LL_ADD(d->connector_head, d->connector_tail, c);
  c->driver = d;
  d->connector_count++;
}

static void pn_driver_remove_connector(pn_driver_t *d, pn_connector_t *c)
{
  if (!c->driver) return;

  if (c == d->connector_next) {
    d->connector_next = c->next;
  }

  LL_REMOVE(d->connector_head, d->connector_tail, c);
  c->driver = NULL;
  d->connector_count--;
  if (c->closed) {
    d->closed_count--;
  }
}

pn_connector_t *pn_connector(pn_driver_t *driver, const char *host,
                             const char *port, void *context)
{
  if (!driver) return NULL;

  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  pn_configure_sock(sock);

  if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    freeaddrinfo(addr);
    return NULL;
  }

  freeaddrinfo(addr);

  pn_connector_t *c = pn_connector_fd(driver, sock, context);
  printf("Connected to %s:%s\n", host, port);
  return c;
}

static void pn_connector_read(pn_connector_t *ctor);
static void pn_connector_write(pn_connector_t *ctor);
static time_t pn_connector_tick(pn_connector_t *ctor, time_t now);

static ssize_t pn_connector_read_sasl_header(pn_connector_t *ctor);
static ssize_t pn_connector_read_sasl(pn_connector_t *ctor);
static ssize_t pn_connector_read_amqp_header(pn_connector_t *ctor);
static ssize_t pn_connector_read_amqp(pn_connector_t *ctor);
static ssize_t pn_connector_write_sasl_header(pn_connector_t *ctor);
static ssize_t pn_connector_write_sasl(pn_connector_t *ctor);
static ssize_t pn_connector_write_amqp_header(pn_connector_t *ctor);
static ssize_t pn_connector_write_amqp(pn_connector_t *ctor);

pn_connector_t *pn_connector_fd(pn_driver_t *driver, int fd, void *context)
{
  if (!driver) return NULL;

  pn_connector_t *c = malloc(sizeof(pn_connector_t));
  if (!c) return NULL;
  c->driver = driver;
  c->next = NULL;
  c->prev = NULL;
  c->pending_tick = false;
  c->pending_read = false;
  c->pending_write = false;
  c->idx = 0;
  c->fd = fd;
  c->status = PN_SEL_RD | PN_SEL_WR;
  c->trace = driver->trace;
  c->closed = false;
  c->wakeup = 0;
  c->read = pn_connector_read;
  c->write = pn_connector_write;
  c->tick = pn_connector_tick;
  c->input_size = 0;
  c->input_eos = false;
  c->output_size = 0;
  c->sasl = pn_sasl();
  c->connection = NULL;
  c->transport = NULL;
  c->process_input = pn_connector_read_sasl_header;
  c->process_output = pn_connector_write_sasl_header;
  c->input_done = false;
  c->output_done = false;
  c->context = context;
  c->listener = NULL;

  pn_connector_trace(c, driver->trace);

  pn_driver_add_connector(driver, c);
  return c;
}

void pn_connector_trace(pn_connector_t *ctor, pn_trace_t trace)
{
  if (!ctor) return;
  ctor->trace = trace;
  if (ctor->sasl) pn_sasl_trace(ctor->sasl, trace);
  if (ctor->transport) pn_trace(ctor->transport, trace);
}

pn_sasl_t *pn_connector_sasl(pn_connector_t *ctor)
{
  return ctor ? ctor->sasl : NULL;
}

void pn_connector_set_connection(pn_connector_t *ctor, pn_connection_t *connection)
{
  if (!ctor) return;
  ctor->connection = connection;
  ctor->transport = pn_transport(connection);
  if (ctor->transport) pn_trace(ctor->transport, ctor->trace);
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

void pn_connector_destroy(pn_connector_t *ctor)
{
  if (!ctor) return;

  if (ctor->driver) pn_driver_remove_connector(ctor->driver, ctor);
  ctor->connection = NULL;
  ctor->transport = NULL;
  pn_sasl_destroy(ctor->sasl);
  free(ctor);
}

static void pn_connector_read(pn_connector_t *ctor)
{
  ssize_t n = recv(ctor->fd, ctor->input + ctor->input_size, IO_BUF_SIZE - ctor->input_size, 0);
  if (n <= 0) {
    if (n < 0) perror("read");
    ctor->status &= ~PN_SEL_RD;
    ctor->input_eos = true;
  } else {
    ctor->input_size += n;
  }
}

static void pn_connector_consume(pn_connector_t *ctor, int n)
{
  ctor->input_size -= n;
  memmove(ctor->input, ctor->input + n, ctor->input_size);
}

static void pn_connector_process_input(pn_connector_t *ctor)
{
  while (!ctor->input_done && (ctor->input_size > 0 || ctor->input_eos)) {
    ssize_t n = ctor->process_input(ctor);
    if (n > 0) {
      pn_connector_consume(ctor, n);
    } else if (n == 0) {
      break;
    } else {
      if (n == PN_EOS) {
        pn_connector_consume(ctor, ctor->input_size);
      } else {
        printf("error in process_input: %s\n", pn_error(n));
      }
      ctor->input_done = true;
      break;
    }
  }
}

static ssize_t pn_connector_read_sasl_header(pn_connector_t *ctor)
{
  if (ctor->input_size >= 8) {
    if (memcmp(ctor->input, "AMQP\x03\x01\x00\x00", 8)) {
      fprintf(stderr, "sasl header missmatch: ");
      pn_fprint_data(stderr, ctor->input, ctor->input_size);
      fprintf(stderr, "\n");
      ctor->output_done = true;
      return PN_ERR;
    } else {
      if (ctor->trace & PN_TRACE_FRM)
        fprintf(stderr, "    <- AMQP SASL 1.0\n");
      ctor->process_input = pn_connector_read_sasl;
      return 8;
    }
  } else if (ctor->input_eos) {
    fprintf(stderr, "sasl header missmatch: ");
    pn_fprint_data(stderr, ctor->input, ctor->input_size);
    fprintf(stderr, "\n");
    ctor->output_done = true;
    return PN_ERR;
  }

  return 0;
}

static ssize_t pn_connector_read_sasl(pn_connector_t *ctor)
{
  pn_sasl_t *sasl = ctor->sasl;
  ssize_t n = pn_sasl_input(sasl, ctor->input, ctor->input_size);
  if (n == PN_EOS) {
    ctor->process_input = pn_connector_read_amqp_header;
    return ctor->process_input(ctor);
  } else {
    return n;
  }
}

static ssize_t pn_connector_read_amqp_header(pn_connector_t *ctor)
{
  if (ctor->input_size >= 8) {
    if (memcmp(ctor->input, "AMQP\x00\x01\x00\x00", 8)) {
      fprintf(stderr, "amqp header missmatch: ");
      pn_fprint_data(stderr, ctor->input, ctor->input_size);
      fprintf(stderr, "\n");
      ctor->output_done = true;
      return PN_ERR;
    } else {
      if (ctor->trace & PN_TRACE_FRM)
        fprintf(stderr, "    <- AMQP 1.0\n");
      ctor->process_input = pn_connector_read_amqp;
      return 8;
    }
  } else if (ctor->input_eos) {
    fprintf(stderr, "amqp header missmatch: ");
    pn_fprint_data(stderr, ctor->input, ctor->input_size);
    fprintf(stderr, "\n");
    ctor->output_done = true;
    return PN_ERR;
  }

  return 0;
}

static ssize_t pn_connector_read_amqp(pn_connector_t *ctor)
{
  if (!ctor->transport) return 0;
  pn_transport_t *transport = ctor->transport;
  return pn_input(transport, ctor->input, ctor->input_size);
}

static char *pn_connector_output(pn_connector_t *ctor)
{
  return ctor->output + ctor->output_size;
}

static size_t pn_connector_available(pn_connector_t *ctor)
{
  return IO_BUF_SIZE - ctor->output_size;
}

static void pn_connector_process_output(pn_connector_t *ctor)
{
  while (!ctor->output_done && pn_connector_available(ctor) > 0) {
    ssize_t n = ctor->process_output(ctor);
    if (n > 0) {
      ctor->output_size += n;
    } else if (n == 0) {
      break;
    } else {
      if (n != PN_EOS) {
        fprintf(stderr, "error in process_output: %s\n", pn_error(n));
      }
      ctor->output_done = true;
      break;
    }
  }

  if (ctor->output_size) {
    ctor->status |= PN_SEL_WR;
  }
}

static void pn_connector_write(pn_connector_t *ctor)
{
  if (ctor->output_size > 0) {
    ssize_t n = send(ctor->fd, ctor->output, ctor->output_size, MSG_NOSIGNAL);
    if (n < 0) {
      // XXX
      perror("send");
      ctor->output_size = 0;
      ctor->output_done = true;
    } else {
      ctor->output_size -= n;
      memmove(ctor->output, ctor->output + n, ctor->output_size);
    }
  }

  if (!ctor->output_size)
    ctor->status &= ~PN_SEL_WR;
}

static ssize_t pn_connector_write_sasl_header(pn_connector_t *ctor)
{
  if (ctor->trace & PN_TRACE_FRM)
    fprintf(stderr, "    -> AMQP SASL 1.0\n");
  memmove(pn_connector_output(ctor), "AMQP\x03\x01\x00\x00", 8);
  ctor->process_output = pn_connector_write_sasl;
  return 8;
}

static ssize_t pn_connector_write_sasl(pn_connector_t *ctor)
{
  pn_sasl_t *sasl = ctor->sasl;
  ssize_t n = pn_sasl_output(sasl, pn_connector_output(ctor), pn_connector_available(ctor));
  if (n == PN_EOS) {
    ctor->process_output = pn_connector_write_amqp_header;
    return ctor->process_output(ctor);
  } else {
    return n;
  }
}

static ssize_t pn_connector_write_amqp_header(pn_connector_t *ctor)
{
  if (ctor->trace & PN_TRACE_FRM)
    fprintf(stderr, "    -> AMQP 1.0\n");
  memmove(pn_connector_output(ctor), "AMQP\x00\x01\x00\x00", 8);
  ctor->process_output = pn_connector_write_amqp;
  return 8;
}

static ssize_t pn_connector_write_amqp(pn_connector_t *ctor)
{
  if (!ctor->transport) return 0;
  pn_transport_t *transport = ctor->transport;
  return pn_output(transport, pn_connector_output(ctor), pn_connector_available(ctor));
}

static time_t pn_connector_tick(pn_connector_t *ctor, time_t now)
{
  if (!ctor->transport) return 0;
  // XXX: should probably have a function pointer for this and switch it with different layers
  time_t result = pn_tick(ctor->transport, now);
  pn_connector_process_input(ctor);
  pn_connector_process_output(ctor);
  return result;
}

void pn_connector_process(pn_connector_t *c) {
  if (c) {
    if (c->closed) return;

    if (c->pending_tick) {
      // XXX: should handle timing also
      c->tick(c, 0);
      c->pending_tick = false;
    }

    if (c->pending_read) {
      c->read(c);
      c->pending_read = false;
    }
    pn_connector_process_input(c);
    pn_connector_process_output(c);
    if (c->pending_write) {
      c->write(c);
      c->pending_write = false;
    }
    if (c->output_size == 0 && c->input_done && c->output_done) {
      if (c->trace & (PN_TRACE_FRM | PN_TRACE_RAW))
        fprintf(stderr, "closed\n");
      pn_connector_close(c);
    }
  }
}

// driver

pn_driver_t *pn_driver()
{
  pn_driver_t *d = malloc(sizeof(pn_driver_t));
  if (!d) return NULL;
  d->listener_head = NULL;
  d->listener_tail = NULL;
  d->listener_next = NULL;
  d->connector_head = NULL;
  d->connector_tail = NULL;
  d->connector_next = NULL;
  d->listener_count = 0;
  d->connector_count = 0;
  d->closed_count = 0;
  d->capacity = 0;
  d->fds = NULL;
  d->nfds = 0;
  d->ctrl[0] = 0;
  d->ctrl[1] = 0;
  d->trace = ((pn_env_bool("PN_TRACE_RAW") ? PN_TRACE_RAW : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_FRM") ? PN_TRACE_FRM : PN_TRACE_OFF));

  // XXX
  if (pipe(d->ctrl)) {
    perror("Can't create control pipe");
  }

  return d;
}

void pn_driver_trace(pn_driver_t *d, pn_trace_t trace)
{
  d->trace = trace;
}

void pn_driver_destroy(pn_driver_t *d)
{
  if (!d) return;

  close(d->ctrl[0]);
  close(d->ctrl[1]);
  while (d->connector_head)
    pn_connector_destroy(d->connector_head);
  while (d->listener_head)
    pn_listener_destroy(d->listener_head);
  free(d->fds);
  free(d);
}

void pn_driver_wakeup(pn_driver_t *d)
{
  if (d) {
    write(d->ctrl[1], "x", 1);
  }
}

static void pn_driver_rebuild(pn_driver_t *d)
{
  size_t size = d->listener_count + d->connector_count;
  while (d->capacity < size + 1) {
    d->capacity = d->capacity ? 2*d->capacity : 16;
    d->fds = realloc(d->fds, d->capacity*sizeof(struct pollfd));
  }

  d->nfds = 0;

  d->fds[d->nfds].fd = d->ctrl[0];
  d->fds[d->nfds].events = POLLIN;
  d->fds[d->nfds].revents = 0;
  d->nfds++;

  pn_listener_t *l = d->listener_head;
  for (int i = 0; i < d->listener_count; i++) {
    d->fds[d->nfds].fd = l->fd;
    d->fds[d->nfds].events = POLLIN;
    d->fds[d->nfds].revents = 0;
    l->idx = d->nfds;
    d->nfds++;
    l = l->next;
  }

  pn_connector_t *c = d->connector_head;
  for (int i = 0; i < d->connector_count; i++)
  {
    if (!c->closed) {
      d->fds[d->nfds].fd = c->fd;
      d->fds[d->nfds].events = (c->status & PN_SEL_RD ? POLLIN : 0) |
        (c->status & PN_SEL_WR ? POLLOUT : 0);
      d->fds[d->nfds].revents = 0;
      c->idx = d->nfds;
      d->nfds++;
    }
    c = c->next;
  }
}

void pn_driver_wait(pn_driver_t *d, int timeout) {
  pn_driver_rebuild(d);

  DIE_IFE(poll(d->fds, d->nfds, d->closed_count > 0 ? 0 : timeout));

  if (d->fds[0].revents & POLLIN) {
    //clear the pipe
    char buffer[512];
    while (read(d->ctrl[0], buffer, 512) == 512);
  }

  pn_listener_t *l = d->listener_head;
  while (l) {
    l->pending = (l->idx && d->fds[l->idx].revents & POLLIN);
    l = l->next;
  }

  pn_connector_t *c = d->connector_head;
  while (c) {
    if (c->closed) {
      c->pending_read = false;
      c->pending_write = false;
      c->pending_tick = false;
    } else {
      int idx = c->idx;
      c->pending_read = (idx && d->fds[idx].revents & POLLIN);
      c->pending_write = (idx && d->fds[idx].revents & POLLOUT);
    }
    c = c->next;
  }

  d->listener_next = d->listener_head;
  d->connector_next = d->connector_head;
}

pn_listener_t *pn_driver_listener(pn_driver_t *d) {
  if (!d) return NULL;

  while (d->listener_next) {
    pn_listener_t *l = d->listener_next;
    d->listener_next = l->next;

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
    d->connector_next = c->next;

    if (c->closed || c->pending_read || c->pending_write || c->pending_tick) {
      return c;
    }
  }

  return NULL;
}
