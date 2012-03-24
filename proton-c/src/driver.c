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

struct pn_driver_t {
  pn_selectable_t *head;
  pn_selectable_t *tail;
  pn_selectable_t *current;
  size_t size;
  size_t capacity;
  struct pollfd *fds;
  int ctrl[2]; //pipe for updating selectable status
  bool stopping;
  pn_trace_t trace;
};

#define IO_BUF_SIZE (4*1024)

struct pn_selectable_t {
  pn_driver_t *driver;
  pn_selectable_t *next;
  pn_selectable_t *prev;
  int idx;
  int fd;
  int status;
  time_t wakeup;
  pn_callback_t *read;
  pn_callback_t *write;
  time_t (*tick)(pn_selectable_t *sel, time_t now);
  size_t input_size;
  char input[IO_BUF_SIZE];
  size_t output_size;
  char output[IO_BUF_SIZE];
  pn_sasl_t *sasl;
  pn_connection_t *connection;
  pn_transport_t *transport;
  ssize_t (*process_input)(pn_selectable_t *sel);
  ssize_t (*process_output)(pn_selectable_t *sel);
  pn_callback_t *callback;
  void *context;
};

/* Impls */

static void pn_driver_add(pn_driver_t *d, pn_selectable_t *s)
{
  LL_ADD(d->head, d->tail, s);
  s->driver = d;
  d->size++;
}

static void pn_driver_remove(pn_driver_t *d, pn_selectable_t *s)
{
  if (s == d->current) {
    d->current = s->next;
  }

  LL_REMOVE(d->head, d->tail, s);
  s->driver = NULL;
  d->size--;
}

static void pn_selectable_read(pn_selectable_t *sel);
static void pn_selectable_write(pn_selectable_t *sel);
static time_t pn_selectable_tick(pn_selectable_t *sel, time_t now);

static ssize_t pn_selectable_read_sasl_header(pn_selectable_t *sel);
static ssize_t pn_selectable_read_sasl(pn_selectable_t *sel);
static ssize_t pn_selectable_read_amqp_header(pn_selectable_t *sel);
static ssize_t pn_selectable_read_amqp(pn_selectable_t *sel);
static ssize_t pn_selectable_write_sasl_header(pn_selectable_t *sel);
static ssize_t pn_selectable_write_sasl(pn_selectable_t *sel);
static ssize_t pn_selectable_write_amqp_header(pn_selectable_t *sel);
static ssize_t pn_selectable_write_amqp(pn_selectable_t *sel);

pn_selectable_t *pn_selectable(pn_driver_t *driver, int fd, pn_callback_t *callback, void *context)
{
  pn_selectable_t *s = malloc(sizeof(pn_selectable_t));
  if (!s) return NULL;
  s->driver = driver;
  s->next = NULL;
  s->prev = NULL;
  s->fd = fd;
  s->status = 0;
  s->wakeup = 0;
  s->read = pn_selectable_read;
  s->write = pn_selectable_write;
  s->tick = pn_selectable_tick;
  s->input_size = 0;
  s->output_size = 0;
  s->sasl = pn_sasl();
  s->connection = pn_connection();
  s->transport = pn_transport(s->connection);
  s->process_input = pn_selectable_read_sasl_header;
  s->process_output = pn_selectable_write_sasl_header;
  s->callback = callback;
  s->context = context;
  s->idx = 0;

  pn_selectable_trace(s, driver->trace);

  pn_driver_add(driver, s);

  return s;
}

void pn_selectable_trace(pn_selectable_t *sel, pn_trace_t trace)
{
  pn_sasl_trace(sel->sasl, trace);
  pn_trace(sel->transport, trace);
}

pn_sasl_t *pn_selectable_sasl(pn_selectable_t *sel)
{
  return sel ? sel->sasl : NULL;
}

pn_connection_t *pn_selectable_connection(pn_selectable_t *sel)
{
  return sel ? sel->connection : NULL;
}

void *pn_selectable_context(pn_selectable_t *sel)
{
  return sel ? sel->context : NULL;
}

void pn_selectable_destroy(pn_selectable_t *sel)
{
  if (!sel) return;

  if (sel->driver) pn_driver_remove(sel->driver, sel);
  pn_connection_destroy(sel->connection);
  pn_sasl_destroy(sel->sasl);
  free(sel);
}

void pn_selectable_close(pn_selectable_t *sel)
{
  // XXX: should probably signal engine and callback here
  if (!sel) return;

  sel->status = 0;
  if (close(sel->fd) == -1)
    perror("close");
}

static void pn_selectable_consume(pn_selectable_t *sel, int n)
{
  sel->input_size -= n;
  memmove(sel->input, sel->input + n, sel->input_size);
}

static void pn_selectable_read(pn_selectable_t *sel)
{
  ssize_t n = recv(sel->fd, sel->input + sel->input_size, IO_BUF_SIZE - sel->input_size, 0);

  if (n <= 0) {
    printf("disconnected: %zi\n", n);
    pn_selectable_close(sel);
    pn_selectable_destroy(sel);
    return;
  } else {
    sel->input_size += n;
  }

  while (sel->input_size > 0) {
    n = sel->process_input(sel);
    if (n > 0) {
      pn_selectable_consume(sel, n);
    } else if (n == 0) {
      return;
    } else {
      if (n != PN_EOS) printf("error in process_input: %zi\n", n);
      pn_selectable_close(sel);
      pn_selectable_destroy(sel);
      return;
    }
  }
}

static ssize_t pn_selectable_read_sasl_header(pn_selectable_t *sel)
{
  if (sel->input_size >= 8) {
    if (memcmp(sel->input, "AMQP\x03\x01\x00\x00", 8)) {
      fprintf(stderr, "sasl header missmatch\n");
      return PN_ERR;
    } else {
      fprintf(stderr, "    <- AMQP SASL 1.0\n");
      sel->process_input = pn_selectable_read_sasl;
      return 8;
    }
  }

  return 0;
}

static ssize_t pn_selectable_read_sasl(pn_selectable_t *sel)
{
  pn_sasl_t *sasl = sel->sasl;
  ssize_t n = pn_sasl_input(sasl, sel->input, sel->input_size);
  if (n == PN_EOS) {
    sel->process_input = pn_selectable_read_amqp_header;
    return sel->process_input(sel);
  } else {
    return n;
  }
}

static ssize_t pn_selectable_read_amqp_header(pn_selectable_t *sel)
{
  if (sel->input_size >= 8) {
    if (memcmp(sel->input, "AMQP\x00\x01\x00\x00", 8)) {
      fprintf(stderr, "amqp header missmatch\n");
      return PN_ERR;
    } else {
      fprintf(stderr, "    <- AMQP 1.0\n");
      sel->process_input = pn_selectable_read_amqp;
      return 8;
    }
  }

  return 0;
}

static ssize_t pn_selectable_read_amqp(pn_selectable_t *sel)
{
  pn_transport_t *transport = sel->transport;
  return pn_input(transport, sel->input, sel->input_size);
}

static char *pn_selectable_output(pn_selectable_t *sel)
{
  return sel->output + sel->output_size;
}

static size_t pn_selectable_available(pn_selectable_t *sel)
{
  return IO_BUF_SIZE - sel->output_size;
}

static void pn_selectable_write(pn_selectable_t *sel)
{
  while (pn_selectable_available(sel) > 0) {
    ssize_t n = sel->process_output(sel);
    if (n > 0) {
      sel->output_size += n;
    } else if (n == 0) {
      break;
    } else {
      if (n != PN_EOS) fprintf(stderr, "error in process_output: %zi", n);
      pn_selectable_close(sel);
      pn_selectable_destroy(sel);
      return;
    }
  }

  if (sel->output_size > 0) {
    ssize_t n = send(sel->fd, sel->output, sel->output_size, 0);
    if (n < 0) {
      // XXX
      perror("send");
      pn_selectable_close(sel);
      pn_selectable_destroy(sel);
      return;
    } else {
      sel->output_size -= n;
      memmove(sel->output, sel->output + n, sel->output_size);
    }

    if (sel->output_size)
      sel->status |= PN_SEL_WR;
    else
      sel->status &= ~PN_SEL_WR;
  }
}

static ssize_t pn_selectable_write_sasl_header(pn_selectable_t *sel)
{
  fprintf(stderr, "    -> AMQP SASL 1.0\n");
  memmove(pn_selectable_output(sel), "AMQP\x03\x01\x00\x00", 8);
  sel->process_output = pn_selectable_write_sasl;
  return 8;
}

static ssize_t pn_selectable_write_sasl(pn_selectable_t *sel)
{
  pn_sasl_t *sasl = sel->sasl;
  ssize_t n = pn_sasl_output(sasl, pn_selectable_output(sel), pn_selectable_available(sel));
  if (n == PN_EOS) {
    sel->process_output = pn_selectable_write_amqp_header;
    return sel->process_output(sel);
  } else {
    return n;
  }
}

static ssize_t pn_selectable_write_amqp_header(pn_selectable_t *sel)
{
  fprintf(stderr, "    -> AMQP 1.0\n");
  memmove(pn_selectable_output(sel), "AMQP\x00\x01\x00\x00", 8);
  sel->process_output = pn_selectable_write_amqp;
  pn_transport_open(sel->transport);
  return 8;
}

static ssize_t pn_selectable_write_amqp(pn_selectable_t *sel)
{
  pn_transport_t *transport = sel->transport;
  return pn_output(transport, pn_selectable_output(sel), pn_selectable_available(sel));
}

static time_t pn_selectable_tick(pn_selectable_t *sel, time_t now)
{
  // XXX: should probably have a function pointer for this and switch it with different layers
  time_t result = pn_tick(sel->transport, now);
  if (sel->callback) sel->callback(sel);
  pn_selectable_write(sel);
  return result;
}

pn_driver_t *pn_driver()
{
  pn_driver_t *d = malloc(sizeof(pn_driver_t));
  if (!d) return NULL;
  d->head = NULL;
  d->tail = NULL;
  d->current = NULL;
  d->size = 0;
  d->capacity = 0;
  d->fds = NULL;
  d->ctrl[0] = 0;
  d->ctrl[1] = 0;
  d->stopping = false;
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
  while (d->head)
    pn_selectable_destroy(d->head);
  free(d->fds);
  free(d);
}

void pn_driver_wakeup(pn_driver_t *d)
{
  write(d->ctrl[1], "x", 1);
}

static void pn_driver_rebuild(pn_driver_t *d)
{
  if (d->size == 0) return;
  while (d->capacity < d->size + 1) {
    d->capacity = d->capacity ? 2*d->capacity : 16;
    d->fds = realloc(d->fds, d->capacity*sizeof(struct pollfd));
  }

  d->fds[0].fd = d->ctrl[0];
  d->fds[0].events = POLLIN;
  d->fds[0].revents = 0;

  pn_selectable_t *s = d->head;
  for (int i = 1; i <= d->size; i++)
  {
    d->fds[i].fd = s->fd;
    d->fds[i].events = (s->status & PN_SEL_RD ? POLLIN : 0) |
      (s->status & PN_SEL_WR ? POLLOUT : 0);
    d->fds[i].revents = 0;
    s->idx = i;
    s = s->next;
  }

}

void pn_driver_wait(pn_driver_t *d) {
  pn_driver_rebuild(d);

  pn_selectable_t *s = d->head;
  while (s) {
    // XXX
    s->tick(s, 0);
    s = s->next;
  }

  DIE_IFE(poll(d->fds, d->size+1, -1));

  if (d->fds[0].revents & POLLIN) {
    //clear the pipe
    char buffer[512];
    while (read(d->ctrl[0], buffer, 512) == 512);
  }

  d->current = d->head;
}

void pn_selectable_work(pn_selectable_t *s) {
  // XXX: this is necessary because read or write might close the
  // selectable, should probably fix this by making them mark it
  // and keeping close/destroy/etc entirely outside the driver
  int idx = s->idx;
  pn_driver_t *d = s->driver;
  if (d->fds[idx].revents & POLLIN)
    s->read(s);
  if (d->fds[idx].revents & POLLOUT)
    s->write(s);
}

pn_selectable_t *pn_driver_next(pn_driver_t *d) {
  pn_selectable_t *s = d->current;
  if (s) {
    d->current = s->next;
    pn_selectable_work(s);
  }
  return s;
}

void pn_driver_run(pn_driver_t *d)
{
  while (!d->stopping)
  {
    pn_driver_wait(d);
    while (pn_driver_next(d));
  }
}

void pn_driver_stop(pn_driver_t *d)
{
  d->stopping = true;
  pn_driver_wakeup(d);
}

pn_selectable_t *pn_connector(pn_driver_t *driver, const char *host, const char *port,
                              pn_callback_t *callback, void *context)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    freeaddrinfo(addr);
    return NULL;
  }

  freeaddrinfo(addr);

  pn_selectable_t *s = pn_selectable(driver, sock, callback, context);
  s->status = PN_SEL_RD | PN_SEL_WR;

  printf("Connected to %s:%s\n", host, port);
  return s;
}

static void do_accept(pn_selectable_t *s)
{
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen = sizeof(addr);
  int sock = accept(s->fd, (struct sockaddr *) &addr, &addrlen);
  if (sock == -1) {
    perror("accept");
  } else {
    char host[1024], serv[64];
    int code;
    if ((code = getnameinfo((struct sockaddr *) &addr, addrlen, host, 1024, serv, 64, 0))) {
      fprintf(stderr, "getnameinfo: %s\n", gai_strerror(code));
      if (close(sock) == -1)
        perror("close");
    } else {
      printf("accepted from %s:%s\n", host, serv);
      pn_selectable_t *a = pn_selectable(s->driver, sock, s->callback, s->context);
      a->status = PN_SEL_RD | PN_SEL_WR;
    }
  }
}

static void do_nothing(pn_selectable_t *s) {}
static time_t never_tick(pn_selectable_t *s, time_t now) { return 0; }

pn_selectable_t *pn_acceptor(pn_driver_t *driver, const char *host, const char *port,
                             pn_callback_t *callback, void* context)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    return NULL;

  if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
    freeaddrinfo(addr);
    return NULL;
  }

  freeaddrinfo(addr);

  if (listen(sock, 50) == -1)
    return NULL;

  // XXX: should factor into pure selectable and separate subclass
  pn_selectable_t *s = pn_selectable(driver, sock, callback, context);
  s->read = do_accept;
  s->write = do_nothing;
  s->tick = never_tick;
  s->status = PN_SEL_RD;

  printf("Listening on %s:%s\n", host, port);
  return s;
}
