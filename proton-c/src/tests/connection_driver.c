/*
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
 */


#include "test_handler.h"
#include <proton/codec.h>
#include <proton/connection_driver.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/link.h>

struct context {
  pn_link_t *link;
  pn_delivery_t *delivery;
};

/* Handler that replies to REMOTE_OPEN */
static pn_event_type_t open_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(e));
    break;
   case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(e));
    break;
   case PN_LINK_REMOTE_OPEN: {
    pn_link_open(pn_event_link(e));
    struct context *ctx = (struct context*) th->context;
    if (ctx) ctx->link = pn_event_link(e);
    break;
   }
   default:
    break;
  }
  return PN_EVENT_NONE;
}

static pn_event_type_t delivery_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_DELIVERY: {
    struct context *ctx = (struct context*)th->context;
    if (ctx) ctx->delivery = pn_event_delivery(e);
    return PN_DELIVERY;
   }
   default:
    return open_handler(th, e);
  }
}

/* Blow-by-blow event verification of a single message transfer */
static void test_message_transfer(test_t *t) {
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, open_handler, NULL, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL, NULL);
  struct context server_ctx;
  server.handler.context = &server_ctx;
  pn_transport_set_server(server.driver.transport);

  pn_connection_open(client.driver.connection);
  pn_session_t *ssn = pn_session(client.driver.connection);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  test_connection_drivers_run(&client, &server);

  TEST_HANDLER_EXPECT(
    &client.handler,
    PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN,
    PN_SESSION_INIT, PN_SESSION_LOCAL_OPEN,
    PN_LINK_INIT, PN_LINK_LOCAL_OPEN,
    PN_CONNECTION_BOUND, PN_CONNECTION_REMOTE_OPEN, PN_SESSION_REMOTE_OPEN, PN_LINK_REMOTE_OPEN,
    0);

  TEST_HANDLER_EXPECT(
    &server.handler,
    PN_CONNECTION_INIT, PN_CONNECTION_BOUND, PN_CONNECTION_REMOTE_OPEN,
    PN_SESSION_INIT, PN_SESSION_REMOTE_OPEN,
    PN_LINK_INIT, PN_LINK_REMOTE_OPEN,
    PN_CONNECTION_LOCAL_OPEN, PN_TRANSPORT,
    PN_SESSION_LOCAL_OPEN, PN_TRANSPORT,
    PN_LINK_LOCAL_OPEN, PN_TRANSPORT,
    0);

  pn_link_t *rcv = server_ctx.link;
  TEST_CHECK(t, rcv);
  TEST_CHECK(t, pn_link_is_receiver(rcv));
  pn_link_flow(rcv, 1);
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT(&client.handler, PN_LINK_FLOW, 0);

  /* Encode and send a message */
  pn_message_t *m = pn_message();
  pn_data_put_string(pn_message_body(m), pn_bytes(4, "abc")); /* Include trailing NULL */
  pn_rwbytes_t buf = { 0 };
  ssize_t size = message_encode(m, &buf);
  pn_message_free(m);
  pn_delivery(snd, pn_dtag("x", 1));
  TEST_CHECK(t, size == pn_link_send(snd, buf.start, size));
  TEST_CHECK(t, pn_link_advance(snd));
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT(&server.handler, PN_TRANSPORT, PN_DELIVERY, 0);

  /* Receive and decode the message */
  pn_delivery_t *dlv = server_ctx.delivery;
  TEST_ASSERT(dlv);
  pn_message_t *m2 = pn_message();
  pn_rwbytes_t buf2 = { 0 };
  message_decode(m2, dlv, &buf2);
  pn_data_t *body = pn_message_body(m2);
  pn_data_rewind(body);
  TEST_CHECK(t, pn_data_next(body));
  TEST_CHECK(t, PN_STRING == pn_data_type(body));
  TEST_CHECK(t, 4 == pn_data_get_string(pn_message_body(m2)).size);
  TEST_STR_EQUAL(t, "abc", pn_data_get_string(pn_message_body(m2)).start);
  pn_message_free(m2);

  free(buf.start);
  free(buf2.start);
  test_connection_driver_destroy(&client);
  test_connection_driver_destroy(&server);
}

/* Send a message in pieces, ensure each can be received before the next is sent */
static void test_message_stream(test_t *t) {
  /* Set up the link, give credit, start the delivery */
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, open_handler, NULL, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL, NULL);
  struct context server_ctx;
  server.handler.context = &server_ctx;
  pn_transport_set_server(server.driver.transport);

  pn_connection_open(client.driver.connection);
  pn_session_t *ssn = pn_session(client.driver.connection);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  test_connection_drivers_run(&client, &server);
  pn_link_t *rcv = server_ctx.link;
  TEST_CHECK(t, rcv);
  pn_link_flow(rcv, 1);
  test_connection_drivers_run(&client, &server);
  test_handler_keep(&client.handler, 1);
  TEST_HANDLER_EXPECT(&client.handler, PN_LINK_FLOW, 0);
  test_handler_keep(&server.handler, 1);
  TEST_HANDLER_EXPECT(&server.handler, PN_TRANSPORT, 0);

  /* Encode a large (not very) message to send in chunks */
  pn_message_t *m = pn_message();
  char body[1024] = { 0 };
  pn_data_put_binary(pn_message_body(m), pn_bytes(sizeof(body), body));
  pn_rwbytes_t buf = { 0 };
  ssize_t size = message_encode(m, &buf);

  /* Send and receive the message in chunks */
  static const ssize_t CHUNK = 100;
  pn_delivery(snd, pn_dtag("x", 1));
  pn_rwbytes_t buf2 = { 0 };
  ssize_t received = 0;
  for (ssize_t i = 0; i < size; i += CHUNK) {
    /* Send a chunk */
    ssize_t c = (i+CHUNK < size) ? CHUNK : size - i;
    TEST_CHECK(t, c == pn_link_send(snd, buf.start + i, c));
    TEST_CHECK(t, &server == test_connection_drivers_run(&client, &server));
    TEST_HANDLER_EXPECT(&server.handler, PN_DELIVERY, 0);
    /* Receive a chunk */
    pn_delivery_t *dlv = server_ctx.delivery;
    pn_link_t *l = pn_delivery_link(dlv);
    ssize_t dsize = pn_delivery_pending(dlv);
    rwbytes_ensure(&buf2, received+dsize);
    TEST_ASSERT(dsize == pn_link_recv(l, buf2.start + received, dsize));
    received += dsize;
  }
  TEST_CHECK(t, pn_link_advance(snd));
  TEST_CHECK(t, received == size);
  TEST_CHECK(t, !memcmp(buf.start, buf2.start, size));

  pn_message_free(m);
  free(buf.start);
  free(buf2.start);
  test_connection_driver_destroy(&client);
  test_connection_driver_destroy(&server);
}

int main(int argc, char **argv) {
  int failed = 0;
  RUN_ARGV_TEST(failed, t, test_message_transfer(&t));
  RUN_ARGV_TEST(failed, t, test_message_stream(&t));
  return failed;
}
