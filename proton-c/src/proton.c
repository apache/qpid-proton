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

#if defined(_WIN32) && ! defined(__CYGWIN__)
#define NOGDI
#include <winsock2.h>
#include "pncompat/misc_funcs.inc"
#else
#include <unistd.h>
#include <libgen.h>
#endif

#include <stdio.h>
#include <string.h>
#include <proton/driver.h>
#include <proton/message.h>
#include <proton/util.h>
#include "util.h"
#include <proton/version.h>
#include <proton/codec.h>
#include <proton/buffer.h>
#include <proton/parser.h>
#include "platform_fmt.h"
#include "protocol.h"

typedef struct {
  char *buf;
  size_t capacity;
} heap_buffer;

heap_buffer client_msg, client_data, server_data, server_iresp;
void free_heap_buffers(void) {
  free(client_msg.buf);
  free(client_data.buf);
  free(server_data.buf);
  free(server_iresp.buf);
}

int buffer(int argc, char **argv)
{
  pn_buffer_t *buf = pn_buffer(16);

  pn_buffer_append(buf, "abcd", 4);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_prepend(buf, "012", 3);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_prepend(buf, "z", 1);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_append(buf, "efg", 3);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_append(buf, "hijklm", 6);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_defrag(buf);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_trim(buf, 1, 1);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_trim(buf, 4, 0);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_clear(buf);
  pn_buffer_print(buf); printf("\n");
  pn_buffer_free(buf);

  pn_data_t *data = pn_data(16);
  int err = pn_data_fill(data, "Ds[iSi]", "desc", 1, "two", 3);
  if (err) {
    printf("%s\n", pn_code(err));
  }
  pn_data_print(data); printf("\n");
  pn_bytes_t str;
  err = pn_data_scan(data, "D.[.S.]", &str);
  if (err) {
    printf("%s\n", pn_code(err));
  } else {
    printf("%.*s\n", (int) str.size, str.start);
  }

  pn_data_clear(data);
  pn_data_fill(data, "DL[SIonn?DL[S]?DL[S]nnI]", ATTACH, "asdf", 1, true,
               true, SOURCE, "queue",
               true, TARGET, "queue",
               0);

  pn_data_print(data); printf("\n");


  pn_data_free(data);

  return 0;
}

struct server_context {
  int count;
  bool quiet;
  int size;
};

void server_callback(pn_connector_t *ctor)
{
  pn_sasl_t *sasl = pn_connector_sasl(ctor);

  while (pn_sasl_state(sasl) != PN_SASL_PASS) {
    switch (pn_sasl_state(sasl)) {
    case PN_SASL_IDLE:
      return;
    case PN_SASL_CONF:
      pn_sasl_mechanisms(sasl, "PLAIN ANONYMOUS");
      pn_sasl_server(sasl);
      break;
    case PN_SASL_STEP:
      {
        size_t n = pn_sasl_pending(sasl);
        PN_ENSURE(server_iresp.buf, server_iresp.capacity, n, char);
        char *iresp = server_iresp.buf;
        pn_sasl_recv(sasl, iresp, n);
        printf("%s", pn_sasl_remote_mechanisms(sasl));
        printf(" response = ");
        pn_print_data(iresp, n);
        printf("\n");
        pn_sasl_done(sasl, PN_SASL_OK);
        pn_connector_set_connection(ctor, pn_connection());
      }
      break;
    case PN_SASL_PASS:
      break;
    case PN_SASL_FAIL:
      return;
    }
  }

  pn_connection_t *conn = pn_connector_connection(ctor);
  struct server_context *ctx = (struct server_context *) pn_connector_context(ctor);
  char tagstr[1024];
  char msg[10*1024];
  PN_ENSURE(server_data.buf, server_data.capacity, (size_t) ctx->size + 16, char);
  char *data = server_data.buf;
  for (int i = 0; i < ctx->size; i++) {
    msg[i] = 'x';
  }
  size_t ndata = pn_message_data(data, ctx->size + 16, msg, ctx->size);

  if (pn_connection_state(conn) == (PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)) {
    pn_connection_open(conn);
  }

  pn_session_t *ssn = pn_session_head(conn, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE);
  while (ssn) {
    pn_session_open(ssn);
    ssn = pn_session_next(ssn, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE);
  }

  pn_link_t *link = pn_link_head(conn, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE);
  while (link) {
    printf("%s, %s\n", pn_terminus_get_address(pn_link_remote_source(link)),
           pn_terminus_get_address(pn_link_remote_target(link)));
    pn_terminus_copy(pn_link_source(link), pn_link_remote_source(link));
    pn_terminus_copy(pn_link_target(link), pn_link_remote_target(link));
    pn_link_open(link);
    if (pn_link_is_receiver(link)) {
      pn_link_flow(link, 100);
    } else {
      pn_delivery(link, pn_dtag("blah", 4));
    }

    link = pn_link_next(link, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE);
  }

  pn_delivery_t *delivery = pn_work_head(conn);
  while (delivery)
  {
    pn_delivery_tag_t tag = pn_delivery_tag(delivery);
    pn_quote_data(tagstr, 1024, tag.bytes, tag.size);
    pn_link_t *link = pn_delivery_link(delivery);
    if (pn_delivery_readable(delivery)) {
      if (!ctx->quiet) {
        printf("received delivery: %s\n", tagstr);
        printf("  payload = \"");
      }
      while (true) {
        ssize_t n = pn_link_recv(link, msg, 48);
        if (n == PN_EOS) {
          pn_link_advance(link);
          pn_delivery_update(delivery, PN_ACCEPTED);
          break;
        } else if (!ctx->quiet) {
          pn_print_data(msg, n);
        }
      }
      if (!ctx->quiet) printf("\"\n");
      if (pn_link_credit(link) < 50) pn_link_flow(link, 100);
    } else if (pn_delivery_writable(delivery)) {
      pn_link_send(link, data, ndata);
      if (pn_link_advance(link)) {
        if (!ctx->quiet) printf("sent delivery: %s\n", tagstr);
        char tagbuf[16];
        sprintf(tagbuf, "%i", ctx->count++);
        pn_delivery(link, pn_dtag(tagbuf, strlen(tagbuf)));
      }
    }

    if (pn_delivery_updated(delivery)) {
      if (!ctx->quiet) printf("disposition for %s: %" PRIu64 "\n", tagstr, pn_delivery_remote_state(delivery));
      pn_delivery_settle(delivery);
    }

    delivery = pn_work_next(delivery);
  }

  if (pn_connection_state(conn) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)) {
    pn_connection_close(conn);
  }

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
}

struct client_context {
  bool init;
  bool done;
  int recv_count;
  int send_count;
  pn_driver_t *driver;
  bool quiet;
  int size;
  int high;
  int low;
  const char *mechanism;
  const char *username;
  const char *password;
  const char *hostname;
  const char *address;
};

void client_callback(pn_connector_t *ctor)
{
  struct client_context *ctx = (struct client_context *) pn_connector_context(ctor);
  if (pn_connector_closed(ctor)) {
    ctx->done = true;
  }

  pn_sasl_t *sasl = pn_connector_sasl(ctor);

  while (pn_sasl_state(sasl) != PN_SASL_PASS) {
    pn_sasl_state_t st = pn_sasl_state(sasl);
    switch (st) {
    case PN_SASL_IDLE:
      return;
    case PN_SASL_CONF:
      if (ctx->mechanism && !strcmp(ctx->mechanism, "PLAIN")) {
        pn_sasl_plain(sasl, ctx->username, ctx->password);
      } else {
        pn_sasl_mechanisms(sasl, ctx->mechanism);
        pn_sasl_client(sasl);
      }
      break;
    case PN_SASL_STEP:
      if (pn_sasl_pending(sasl)) {
        fprintf(stderr, "challenge failed\n");
        ctx->done = true;
      }
      return;
    case PN_SASL_FAIL:
      fprintf(stderr, "authentication failed\n");
      ctx->done = true;
      return;
    case PN_SASL_PASS:
      break;
    }
  }

  pn_connection_t *connection = pn_connector_connection(ctor);
  char tagstr[1024];
  PN_ENSURE(client_msg.buf, client_msg.capacity, (size_t) ctx->size + 16, char);
  char *msg = client_msg.buf;
  PN_ENSURE(client_data.buf, client_data.capacity, (size_t) ctx->size + 16, char);
  char *data = client_data.buf;
  for (int i = 0; i < ctx->size; i++) {
    msg[i] = 'x';
  }
  size_t ndata = pn_message_data(data, ctx->size + 16, msg, ctx->size);

  if (!ctx->init) {
    ctx->init = true;

    char container[1024];
    if (gethostname(container, 1024)) pn_fatal("hostname lookup failed");

    pn_connection_set_container(connection, container);
    pn_connection_set_hostname(connection, ctx->hostname);

    pn_session_t *ssn = pn_session(connection);
    pn_connection_open(connection);
    pn_session_open(ssn);

    if (ctx->send_count) {
      pn_link_t *snd = pn_sender(ssn, "sender");
      pn_terminus_set_address(pn_link_target(snd), ctx->address);
      pn_link_open(snd);

      char buf[16];
      for (int i = 0; i < ctx->send_count; i++) {
        sprintf(buf, "%x", i);
        pn_delivery(snd, pn_dtag(buf, strlen(buf)));
      }
    }

    if (ctx->recv_count) {
      pn_link_t *rcv = pn_receiver(ssn, "receiver");
      pn_terminus_set_address(pn_link_source(rcv), ctx->address);
      pn_link_open(rcv);
      pn_link_flow(rcv, ctx->recv_count < ctx->high ? ctx->recv_count : ctx->high);
    }
  }

  pn_delivery_t *delivery = pn_work_head(connection);
  while (delivery)
  {
    pn_delivery_tag_t tag = pn_delivery_tag(delivery);
    pn_quote_data(tagstr, 1024, tag.bytes, tag.size);
    pn_link_t *link = pn_delivery_link(delivery);
    if (pn_delivery_writable(delivery)) {
      pn_link_send(link, data, ndata);
      if (pn_link_advance(link)) {
        if (!ctx->quiet) printf("sent delivery: %s\n", tagstr);
      }
    } else if (pn_delivery_readable(delivery)) {
      if (!ctx->quiet) {
        printf("received delivery: %s\n", tagstr);
        printf("  payload = \"");
      }
      while (true) {
        ssize_t n = pn_link_recv(link, msg, 1024);
        if (n == PN_EOS) {
          pn_link_advance(link);
          pn_delivery_update(delivery, PN_ACCEPTED);
          pn_delivery_settle(delivery);
          if (!--ctx->recv_count) {
            pn_link_close(link);
          }
          break;
        } else if (!ctx->quiet) {
          pn_print_data(msg, n);
        }
      }
      if (!ctx->quiet) printf("\"\n");

      if (pn_link_credit(link) < ctx->low && pn_link_credit(link) < ctx->recv_count) {
        pn_link_flow(link, (ctx->recv_count < ctx->high ? ctx->recv_count : ctx->high)
                     - pn_link_credit(link));
      }
    }

    if (pn_delivery_updated(delivery)) {
      if (!ctx->quiet) printf("disposition for %s: %" PRIu64 "\n", tagstr, pn_delivery_remote_state(delivery));
      pn_delivery_clear(delivery);
      pn_delivery_settle(delivery);
      if (!--ctx->send_count) {
        pn_link_close(link);
      }
    }

    delivery = pn_work_next(delivery);
  }

  if (!ctx->send_count && !ctx->recv_count) {
    printf("closing\n");
    // XXX: how do we close the session?
    //pn_close((pn_endpoint_t *) ssn);
    pn_connection_close(connection);
  }

  if (pn_connection_state(connection) == (PN_LOCAL_CLOSED | PN_REMOTE_CLOSED)) {
    ctx->done = true;
  }
}

#include <ctype.h>

int main(int argc, char **argv)
{
  char *url = NULL;
  char *address = (char *) "queue";
  char *mechanism = (char *) "ANONYMOUS";
  int count = 1;
  bool quiet = false;
  int high = 100;
  int low = 50;
  int size = 32;

  int opt;
  while ((opt = getopt(argc, argv, "c:a:m:n:s:u:l:qhVXY")) != -1)
  {
    switch (opt) {
    case 'c':
      if (url) pn_fatal("multiple connect urls not allowed\n");
      url = optarg;
      break;
    case 'a':
      address = optarg;
      break;
    case 'm':
      mechanism = optarg;
      break;
    case 'n':
      count = atoi(optarg);
      break;
    case 's':
      size = atoi(optarg);
      break;
    case 'u':
      high = atoi(optarg);
      break;
    case 'l':
      low = atoi(optarg);
      break;
    case 'q':
      quiet = true;
      break;
    case 'V':
      printf("proton version %i.%i\n", PN_VERSION_MAJOR, PN_VERSION_MINOR);
      exit(EXIT_SUCCESS);
    case 'Y':
      buffer(argc, argv);
      exit(EXIT_SUCCESS);
    case 'h':
      printf("Usage: proton [-h] [-c [user[:password]@]host[:port]] [-a <address>] [-m <sasl-mech>]\n");
      printf("\n");
      printf("    -c    The connect url.\n");
      printf("    -a    The AMQP address.\n");
      printf("    -m    The SASL mechanism.\n");
      printf("    -n    The number of messages.\n");
      printf("    -s    Message size.\n");
      printf("    -u    Upper flow threshold.\n");
      printf("    -l    Lower flow threshold.\n");
      printf("    -q    Supress printouts.\n");
      printf("    -h    Print this help.\n");
      exit(EXIT_SUCCESS);
    default: /* '?' */
      pn_fatal("Usage: %s -h\n", argv[0]);
    }
  }

  char *scheme = NULL;
  char *user = NULL;
  char *pass = NULL;
  char *host = (char *) "0.0.0.0";
  char *port = (char *) "5672";
  char *path = NULL;

  pni_parse_url(url, &scheme, &user, &pass, &host, &port, &path);

  pn_driver_t *drv = pn_driver();
  if (url) {
    struct client_context ctx = {false, false, count, count, drv, quiet, size, high, low};
    ctx.username = user;
    ctx.password = pass;
    ctx.mechanism = mechanism;
    ctx.hostname = host;
    ctx.address = address;
    pn_connector_t *ctor = pn_connector(drv, host, port, &ctx);
    if (!ctor) pn_fatal("connector failed\n");
    pn_connector_set_connection(ctor, pn_connection());
    while (!ctx.done) {
      pn_driver_wait(drv, -1);
      pn_connector_t *c;
      while ((c = pn_driver_connector(drv))) {
        pn_connector_process(c);
        client_callback(c);
        if (pn_connector_closed(c)) {
	  pn_connection_free(pn_connector_connection(c));
          pn_connector_free(c);
        } else {
          pn_connector_process(c);
        }
      }
    }
  } else {
    struct server_context ctx = {0, quiet, size};
    if (!pn_listener(drv, host, port, &ctx)) pn_fatal("listener failed\n");
    while (true) {
      pn_driver_wait(drv, -1);
      pn_listener_t *l;
      pn_connector_t *c;

      while ((l = pn_driver_listener(drv))) {
        c = pn_listener_accept(l);
        pn_connector_set_context(c, &ctx);
      }

      while ((c = pn_driver_connector(drv))) {
        pn_connector_process(c);
        server_callback(c);
        if (pn_connector_closed(c)) {
	  pn_connection_free(pn_connector_connection(c));
          pn_connector_free(c);
        } else {
          pn_connector_process(c);
        }
      }
    }
  }

  pn_driver_free(drv);
  free_heap_buffers();

  return 0;
}
