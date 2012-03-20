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
  size_t size;
  int ctrl[2]; //pipe for updating selectable status
  bool stopping;
  pn_trace_t trace;
};

#define IO_BUF_SIZE (4*1024)

struct pn_selectable_t {
  pn_driver_t *driver;
  pn_selectable_t *next;
  pn_selectable_t *prev;
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
  return sel->sasl;
}

pn_connection_t *pn_selectable_connection(pn_selectable_t *sel)
{
  return sel->connection;
}

void *pn_selectable_context(pn_selectable_t *sel)
{
  return sel->context;
}

void pn_selectable_destroy(pn_selectable_t *sel)
{
  if (sel->driver) pn_driver_remove(sel->driver, sel);
  if (sel->connection) pn_destroy((pn_endpoint_t *) sel->connection);
  if (sel->sasl) pn_sasl_destroy(sel->sasl);
  free(sel);
}

static void pn_selectable_close(pn_selectable_t *sel)
{
  // XXX: should probably signal engine and callback here
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
  pn_open((pn_endpoint_t *) sel->transport);
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
  d->size = 0;
  d->ctrl[0] = 0;
  d->ctrl[1] = 0;
  d->stopping = false;
  d->trace = ((pn_env_bool("PN_TRACE_RAW") ? PN_TRACE_RAW : PN_TRACE_OFF) |
              (pn_env_bool("PN_TRACE_FRM") ? PN_TRACE_FRM : PN_TRACE_OFF));
  return d;
}

void pn_driver_trace(pn_driver_t *d, pn_trace_t trace)
{
  d->trace = trace;
}

void pn_driver_destroy(pn_driver_t *d)
{
  while (d->head)
    pn_selectable_destroy(d->head);
  free(d);
}

void pn_driver_run(pn_driver_t *d)
{
  int i, nfds = 0;
  struct pollfd *fds = NULL;

  if (pipe(d->ctrl)) {
      perror("Can't create control pipe");
  }
  while (!d->stopping)
  {
    int n = d->size;
    if (n == 0) break;
    if (n > nfds) {
      fds = realloc(fds, (n+1)*sizeof(struct pollfd));
      nfds = n;
    }

    pn_selectable_t *s = d->head;
    for (i = 0; i < n; i++)
    {
      fds[i].fd = s->fd;
      fds[i].events = (s->status & PN_SEL_RD ? POLLIN : 0) |
        (s->status & PN_SEL_WR ? POLLOUT : 0);
      fds[i].revents = 0;
      // XXX
      s->tick(s, 0);
      s = s->next;
    }
    fds[n].fd = d->ctrl[0];
    fds[n].events = POLLIN;
    fds[n].revents = 0;

    DIE_IFE(poll(fds, n+1, -1));

    s = d->head;
    for (i = 0; i < n; i++)
    {
      // XXX: this is necessary because read or write might close the
      // selectable, should probably fix this by making them mark it
      // as closed and closing from this loop
      pn_selectable_t *next = s->next;
      if (fds[i].revents & POLLIN)
        s->read(s);
      if (fds[i].revents & POLLOUT)
        s->write(s);
      s = next;
    }

    if (fds[n].revents & POLLIN) {
      //clear the pipe
      char buffer[512];
      while (read(d->ctrl[0], buffer, 512) == 512);
    }
  }

  close(d->ctrl[0]);
  close(d->ctrl[1]);
  free(fds);
}

void pn_driver_stop(pn_driver_t *d)
{
  d->stopping = true;
  write(d->ctrl[1], "x", 1);
}

pn_selectable_t *pn_connector(pn_driver_t *driver, char *host, char *port, pn_callback_t *callback, void *context)
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

pn_selectable_t *pn_acceptor(pn_driver_t *driver, char *host, char *port, pn_callback_t *callback, void* context)
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
