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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <proton/driver.h>
#include <proton/message.h>
#include <proton/value.h>
#include <proton/util.h>
#include <unistd.h>
#include "util.h"
#include "pn_config.h"

void print(pn_value_t value)
{
  char buf[1024];
  char *pos = buf;
  pn_format_value(&pos, buf + 1024, &value, 1);
  *pos = '\0';
  printf("%s\n", buf);
}

int value(int argc, char **argv)
{
  pn_value_t v[1024];
  int count = pn_scan(v, "niIlLISz[iii{SSSi}]i([iiiiz])@i[iii]{SiSiSi}", -3, 3, -123456789101112, 123456719101112, 3,
                       L"this is a string", (size_t) 16, "this is binary\x00\x01",
                       1, 2, 3, L"key", L"value", L"one", 1,
                       1, 2, 3, 4, 5, 7, "binary",
                       1, 2, 3,
                       L"one", 1, L"two", 2, L"three", 3);

  pn_list_t *list = pn_to_list(v[8]);
  pn_map_t *map = pn_to_map(pn_list_get(list, 3));
  print(pn_list_get(list, 3));
  printf("POP: ");
  print(pn_map_pop(map, pn_value("S", L"key")));

  printf("scanned %i values\n", count);
  for (int i = 0; i < count; i++) {
    printf("value %.2i [%zi]: ", i, pn_encode_sizeof(v[i])); print(v[i]);
  }

  pn_list_t *l = pn_list(1024);
  pn_list_extend(l, "SIi[iii]", L"One", 2, -3, 4, 5, 6);
  printf("list [%zi]: ", pn_encode_sizeof_list(l)); print(pn_from_list(l));

  for (int i = 0; i < count; i++)
  {
    char buf[pn_encode_sizeof(v[i])];
    size_t size = pn_encode(v[i], buf);
    pn_value_t value;
    size_t read = pn_decode(&value, buf, size);
    printf("read=%zi: ", read); print(value);
  }

  return 0;
}

struct server_context {
  int count;
};

void server_callback(pn_selectable_t *sel)
{
  pn_sasl_t *sasl = pn_selectable_sasl(sel);

  if (!pn_sasl_init(sasl)) {
    pn_sasl_server(sasl);
  }

  switch (pn_sasl_outcome(sasl)) {
  case SASL_NONE:
    {
      const char *mech = pn_sasl_mechanism(sasl);
      if (mech && (!strcmp(mech, "PLAIN") || !strcmp(mech, "ANONYMOUS"))) {
        pn_binary_t *response = pn_sasl_response(sasl);
        char buf[1024];
        pn_format(buf, 1024, pn_from_binary(response));
        printf("response = %s\n", buf);
        pn_sasl_auth(sasl, SASL_OK);
        break;
      } else {
        return;
      }
    }
  case SASL_OK:
    break;
  default:
    return;
  }

  pn_connection_t *conn = pn_selectable_connection(sel);
  struct server_context *ctx = pn_selectable_context(sel);
  char tagstr[1024];
  char msg[1024];
  char data[1024];

  pn_endpoint_t *endpoint = pn_endpoint_head(conn, UNINIT, ACTIVE);
  while (endpoint)
  {
    switch (pn_endpoint_type(endpoint))
    {
    case CONNECTION:
    case SESSION:
      if (pn_remote_state(endpoint) != UNINIT)
        pn_open(endpoint);
      break;
    case SENDER:
    case RECEIVER:
      {
        pn_link_t *link = (pn_link_t *) endpoint;
        if (pn_remote_state(endpoint) != UNINIT) {
          printf("%ls, %ls\n", pn_remote_source(link), pn_remote_target(link));
          pn_set_source(link, pn_remote_source(link));
          pn_set_target(link, pn_remote_target(link));
          pn_open(endpoint);
          if (pn_endpoint_type(endpoint) == RECEIVER) {
            pn_flow((pn_receiver_t *) endpoint, 100);
          } else {
            pn_binary_t *tag = pn_binary("blah", 4);
            pn_delivery(link, tag);
            pn_free_binary(tag);
          }
        }
      }
      break;
    case TRANSPORT:
      break;
    }

    endpoint = pn_endpoint_next(endpoint, UNINIT, ACTIVE);
  }

  pn_delivery_t *delivery = pn_work_head(conn);
  while (delivery)
  {
    pn_binary_t *tag = pn_delivery_tag(delivery);
    pn_format(tagstr, 1024, pn_from_binary(tag));
    pn_link_t *link = pn_link(delivery);
    if (pn_readable(delivery)) {
      printf("received delivery: %s\n", tagstr);
      pn_receiver_t *receiver = (pn_receiver_t *) link;
      printf("  payload = \"");
      while (true) {
        ssize_t n = pn_recv(receiver, msg, 1024);
        if (n == PN_EOM) {
          pn_advance(link);
          pn_disposition(delivery, PN_ACCEPTED);
          break;
        } else {
          pn_print_data(msg, n);
        }
      }
      printf("\"\n");
    } else if (pn_writable(delivery)) {
      pn_sender_t *sender = (pn_sender_t *) link;
      sprintf(msg, "message body for %s", tagstr);
      size_t n = pn_message_data(data, 1024, msg, strlen(msg));
      pn_send(sender, data, n);
      if (pn_advance(link)) {
        printf("sent delivery: %s\n", tagstr);
        char tagbuf[16];
        sprintf(tagbuf, "%i", ctx->count++);
        pn_binary_t *tag = pn_binary(tagbuf, strlen(tagbuf));
        pn_delivery(link, tag);
        pn_free_binary(tag);
      }
    }

    if (pn_dirty(delivery)) {
      printf("disposition for %s: %u\n", tagstr, pn_remote_disp(delivery));
      pn_clean(delivery);
    }

    delivery = pn_work_next(delivery);
  }

  endpoint = pn_endpoint_head(conn, ACTIVE, CLOSED);
  while (endpoint)
  {
    switch (pn_endpoint_type(endpoint))
    {
    case CONNECTION:
    case SESSION:
    case SENDER:
    case RECEIVER:
      if (pn_remote_state(endpoint) == CLOSED) {
        pn_close(endpoint);
      }
      break;
    case TRANSPORT:
      break;
    }

    endpoint = pn_endpoint_next(endpoint, ACTIVE, CLOSED);
  }
}

struct client_context {
  bool init;
  int recv_count;
  int send_count;
  pn_driver_t *driver;
  const char *mechanism;
  const char *username;
  const char *password;
  wchar_t hostname[1024];
  wchar_t address[1024];
};

void client_callback(pn_selectable_t *sel)
{
  struct client_context *ctx = pn_selectable_context(sel);

  pn_sasl_t *sasl = pn_selectable_sasl(sel);
  if (!pn_sasl_init(sasl)) {
    pn_sasl_client(sasl, ctx->mechanism, ctx->username, ctx->password);
  }

  switch (pn_sasl_outcome(sasl)) {
  case SASL_NONE:
    return;
  case SASL_OK:
    break;
  default:
    fprintf(stderr, "auth failed\n");
    pn_driver_stop(ctx->driver);
    return;
  }

  pn_connection_t *connection = pn_selectable_connection(sel);
  char tagstr[1024];
  char msg[1024];
  char data[1024];

  if (!ctx->init) {
    ctx->init = true;

    char container[1024];
    if (gethostname(container, 1024)) pn_fatal("hostname lookup failed");
    wchar_t wcontainer[1024];
    mbstowcs(wcontainer, container, 1024);

    pn_connection_set_container(connection, wcontainer);
    pn_connection_set_hostname(connection, ctx->hostname);

    pn_session_t *ssn = pn_session(connection);
    pn_open((pn_endpoint_t *) connection);
    pn_open((pn_endpoint_t *) ssn);

    if (ctx->send_count) {
      pn_sender_t *snd = pn_sender(ssn, L"sender");
      pn_set_target((pn_link_t *) snd, ctx->address);
      pn_open((pn_endpoint_t *) snd);

      char buf[16];
      for (int i = 0; i < ctx->send_count; i++) {
        sprintf(buf, "%c", 'a' + i);
        pn_binary_t *tag = pn_binary(buf, strlen(buf));
        pn_delivery((pn_link_t *) snd, tag);
        pn_free_binary(tag);
      }
    }

    if (ctx->recv_count) {
      pn_receiver_t *rcv = pn_receiver(ssn, L"receiver");
      pn_set_source((pn_link_t *) rcv, ctx->address);
      pn_open((pn_endpoint_t *) rcv);
      pn_flow(rcv, ctx->recv_count);
    }
  }

  pn_delivery_t *delivery = pn_work_head(connection);
  while (delivery)
  {
    pn_binary_t *tag = pn_delivery_tag(delivery);
    pn_format(tagstr, 1024, pn_from_binary(tag));
    pn_link_t *link = pn_link(delivery);
    if (pn_writable(delivery)) {
      pn_sender_t *snd = (pn_sender_t *) link;
      sprintf(msg, "message body for %s", tagstr);
      ssize_t n = pn_message_data(data, 1024, msg, strlen(msg));
      pn_send(snd, data, n);
      if (pn_advance(link)) printf("sent delivery: %s\n", tagstr);
    } else if (pn_readable(delivery)) {
      printf("received delivery: %s\n", tagstr);
      pn_receiver_t *rcv = (pn_receiver_t *) link;
      printf("  payload = \"");
      while (true) {
        size_t n = pn_recv(rcv, msg, 1024);
        if (n == PN_EOM) {
          pn_advance(link);
          pn_disposition(delivery, PN_ACCEPTED);
          pn_settle(delivery);
          if (!--ctx->recv_count) {
            pn_close((pn_endpoint_t *)link);
          }
          break;
        } else {
          pn_print_data(msg, n);
        }
      }
      printf("\"\n");
    }

    if (pn_dirty(delivery)) {
      printf("disposition for %s: %u\n", tagstr, pn_remote_disp(delivery));
      pn_clean(delivery);
      pn_settle(delivery);
      if (!--ctx->send_count) {
        pn_close((pn_endpoint_t *)link);
      }
    }

    delivery = pn_work_next(delivery);
  }

  if (!ctx->send_count && !ctx->recv_count) {
    printf("closing\n");
    // XXX: how do we close the session?
    //pn_close((pn_endpoint_t *) ssn);
    pn_close((pn_endpoint_t *)connection);
  }

  pn_endpoint_t *endpoint = pn_endpoint_head(connection, CLOSED, CLOSED);
  while (endpoint)
  {
    switch (pn_endpoint_type(endpoint)) {
    case CONNECTION:
      pn_driver_stop(ctx->driver);
      break;
    case SESSION:
    case SENDER:
    case RECEIVER:
    case TRANSPORT:
      break;
    }
    endpoint = pn_endpoint_next(endpoint, CLOSED, CLOSED);
  }
}

#include <ctype.h>

int main(int argc, char **argv)
{
  char *url = NULL;
  char *address = "queue";
  char *mechanism = "ANONYMOUS";

  int opt;
  while ((opt = getopt(argc, argv, "c:a:m:hVX")) != -1)
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
    case 'V':
      printf("proton version %i.%i\n", PN_VERSION_MAJOR, PN_VERSION_MINOR);
      exit(EXIT_SUCCESS);
    case 'X':
      value(argc, argv);
      exit(EXIT_SUCCESS);
    case 'h':
      printf("Usage: %s [-h] [-c [user[:password]@]host[:port]] [-a <address>] [-m <sasl-mech>]\n", argv[0]);
      printf("\n");
      printf("    -c    The connect url.\n");
      printf("    -a    The AMQP address.\n");
      printf("    -m    The SASL mechanism.\n");
      printf("    -h    Print this help.\n");
      exit(EXIT_SUCCESS);
    default: /* '?' */
      pn_fatal("Usage: %s -h\n", argv[0]);
    }
  }

  char *user = NULL;
  char *pass = NULL;
  char *host = "0.0.0.0";
  char *port = "5672";

  parse_url(url, &user, &pass, &host, &port);

  pn_driver_t *drv = pn_driver();
  if (url) {
    struct client_context ctx = {false, 10, 10, drv};
    ctx.username = user;
    ctx.password = pass;
    ctx.mechanism = mechanism;
    mbstowcs(ctx.hostname, host, 1024);
    mbstowcs(ctx.address, address, 1024);
    if (!pn_connector(drv, host, port, client_callback, &ctx)) pn_fatal("connector failed\n");
  } else {
    struct server_context ctx = {0};
    if (!pn_acceptor(drv, host, port, server_callback, &ctx)) pn_fatal("acceptor failed\n");
  }

  pn_driver_run(drv);
  pn_driver_destroy(drv);

  return 0;
}
