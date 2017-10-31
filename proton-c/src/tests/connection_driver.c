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

/* Handler that replies to REMOTE_OPEN, stores the opened object on the handler */
static pn_event_type_t open_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    th->connection = pn_event_connection(e);
    pn_connection_open(th->connection);
    break;
   case PN_SESSION_REMOTE_OPEN:
    th->session =  pn_event_session(e);
    pn_session_open(th->session);
    break;
   case PN_LINK_REMOTE_OPEN:
    th->link = pn_event_link(e);
    pn_link_open(th->link);
    break;
   default:
    break;
  }
  return PN_EVENT_NONE;
}

/* Handler that returns control on PN_DELIVERY and stores the delivery on the handler */
static pn_event_type_t delivery_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_DELIVERY: {
     th->delivery = pn_event_delivery(e);
    return PN_DELIVERY;
   }
   default:
    return open_handler(th, e);
  }
}

/* Blow-by-blow event verification of a single message transfer */
static void test_message_transfer(test_t *t) {
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, open_handler, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL);
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

  pn_link_t *rcv = server.handler.link;
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
  pn_delivery(snd, PN_BYTES_LITERAL(x));
  TEST_INT_EQUAL(t, size, pn_link_send(snd, buf.start, size));
  TEST_CHECK(t, pn_link_advance(snd));
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT(&server.handler, PN_TRANSPORT, PN_DELIVERY, 0);

  /* Receive and decode the message */
  pn_delivery_t *dlv = server.handler.delivery;
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

/* Handler that opens a connection and sender link */
pn_event_type_t send_client_handler(test_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_LOCAL_OPEN: {
    pn_connection_open(pn_event_connection(e));
    pn_session_t *ssn = pn_session(pn_event_connection(e));
    pn_session_open(ssn);
    pn_link_t *snd = pn_sender(ssn, "x");
    pn_link_open(snd);
    break;
   }
   case PN_LINK_REMOTE_OPEN: {
    th->link = pn_event_link(e);
    return PN_LINK_REMOTE_OPEN;
   }
   default:
    break;
  }
  return PN_EVENT_NONE;
}

/* Send a message in pieces, ensure each can be received before the next is sent */
static void test_message_stream(test_t *t) {
  /* Set up the link, give credit, start the delivery */
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, send_client_handler, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL);
  pn_transport_set_server(server.driver.transport);

  pn_connection_open(client.driver.connection);
  test_connection_drivers_run(&client, &server);
  pn_link_t *rcv = server.handler.link;
  pn_link_t *snd = client.handler.link;
  pn_link_flow(rcv, 1);
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT_LAST(&client.handler, PN_LINK_FLOW);
  TEST_HANDLER_EXPECT_LAST(&server.handler, PN_TRANSPORT);

  /* Encode a large (not very) message to send in chunks */
  pn_message_t *m = pn_message();
  char body[1024] = { 0 };
  pn_data_put_binary(pn_message_body(m), pn_bytes(sizeof(body), body));
  pn_rwbytes_t buf = { 0 };
  ssize_t size = message_encode(m, &buf);

  /* Send and receive the message in chunks */
  static const ssize_t CHUNK = 100;
  pn_delivery(snd, PN_BYTES_LITERAL(x));
  pn_rwbytes_t buf2 = { 0 };
  ssize_t received = 0;
  for (ssize_t i = 0; i < size; i += CHUNK) {
    /* Send a chunk */
    ssize_t c = (i+CHUNK < size) ? CHUNK : size - i;
    TEST_CHECK(t, c == pn_link_send(snd, buf.start + i, c));
    test_connection_drivers_run(&client, &server);
    TEST_HANDLER_EXPECT_LAST(&server.handler, PN_DELIVERY);
    /* Receive a chunk */
    pn_delivery_t *dlv = server.handler.delivery;
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

// Test aborting a delivery
static void test_message_abort(test_t *t) {
  /* Set up the link, give credit, start the delivery */
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, send_client_handler, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL);
  pn_transport_set_server(server.driver.transport);
  pn_connection_open(client.driver.connection);

  test_connection_drivers_run(&client, &server);
  pn_link_t *rcv = server.handler.link;
  pn_link_t *snd = client.handler.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */

  /* Send 2 frames with data */
  pn_link_flow(rcv, 1);
  TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
  test_connection_drivers_run(&client, &server);
  TEST_INT_EQUAL(t, 1, pn_link_credit(snd));
  pn_delivery_t *sd = pn_delivery(snd, PN_BYTES_LITERAL(1)); /* Sender delivery */
  for (size_t i = 0; i < 2; ++i) {
    TEST_INT_EQUAL(t, sizeof(data), pn_link_send(snd, data, sizeof(data)));
    test_connection_drivers_run(&client, &server);
    TEST_HANDLER_EXPECT_LAST(&server.handler, PN_DELIVERY);
    pn_delivery_t *rd = server.handler.delivery;
    TEST_CHECK(t, !pn_delivery_aborted(rd));
    TEST_CHECK(t, pn_delivery_partial(rd));
    TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
    TEST_INT_EQUAL(t, sizeof(data), pn_delivery_pending(rd));
    TEST_INT_EQUAL(t, sizeof(rbuf), pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
    TEST_INT_EQUAL(t, 0, pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
    TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
  }
  TEST_INT_EQUAL(t, 1, pn_link_credit(snd));
  /* Abort the delivery */
  pn_delivery_abort(sd);
  TEST_INT_EQUAL(t, 0, pn_link_credit(snd));
  TEST_CHECK(t, pn_link_current(snd) != sd); /* Settled */
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT_LAST(&server.handler, PN_DELIVERY);
  TEST_INT_EQUAL(t, 0, pn_link_credit(snd));

  /* Receive the aborted=true frame, should be empty */
  pn_delivery_t *rd = server.handler.delivery;
  TEST_CHECK(t, pn_delivery_aborted(rd));
  TEST_CHECK(t, !pn_delivery_partial(rd)); /* Aborted deliveries are never partial */
  TEST_CHECK(t, pn_delivery_settled(rd)); /* Aborted deliveries are remote settled */
  TEST_INT_EQUAL(t, 1, pn_delivery_pending(rd));
  TEST_INT_EQUAL(t, PN_ABORTED, pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
  pn_delivery_settle(rd);       /* Must be settled locally to free it */

  TEST_INT_EQUAL(t, 0, pn_link_credit(snd));
  TEST_INT_EQUAL(t, 0, pn_link_credit(rcv));

  /* Abort a delivery before any data has been framed, should be dropped. */
  pn_link_flow(rcv, 1);
  TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
  test_connection_drivers_run(&client, &server);
  test_handler_clear(&client.handler, 0);
  test_handler_clear(&server.handler, 0);

  sd = pn_delivery(snd, PN_BYTES_LITERAL(x));
  TEST_INT_EQUAL(t, sizeof(data), pn_link_send(snd, data, sizeof(data)));
  pn_delivery_abort(sd);
  TEST_CHECK(t, pn_link_current(snd) != sd); /* Settled, possibly freed */
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT(&server.handler, 0); /* Expect no delivery at the server */
  /* Client gets transport/flow after abort to ensure other messages are sent */
  TEST_HANDLER_EXPECT(&client.handler, PN_TRANSPORT, PN_LINK_FLOW, 0);
  /* Aborted delivery consumes no credit */
  TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
  TEST_INT_EQUAL(t, 1, pn_link_credit(snd));

  test_connection_driver_destroy(&client);
  test_connection_driver_destroy(&server);
}


int send_receive_message(test_t *t, const char* tag,
                         test_connection_driver_t *src, test_connection_driver_t *dst)
{
  int errors = t->errors;
  char data[100] = {0};          /* Dummy data to send. */
  strncpy(data, tag, sizeof(data));
  data[99] = 0; /* Ensure terminated as we strcmp this later*/

  if (!TEST_CHECK(t, pn_link_credit(src->handler.link))) return 1;

  pn_delivery_t *sd = pn_delivery(src->handler.link, pn_dtag(tag, strlen(tag)));
  dst->handler.delivery = NULL;
  TEST_CHECK(t, pn_delivery_current(sd));
  TEST_INT_EQUAL(t, sizeof(data), pn_link_send(src->handler.link, data, sizeof(data)));
  pn_delivery_settle(sd);
  test_connection_drivers_run(src, dst);
  pn_delivery_t *rd = dst->handler.delivery;
  dst->handler.delivery = NULL;
  if (!TEST_CHECK(t, rd)) return 1;

  TEST_CHECK(t, pn_delivery_current(rd));
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */
  TEST_INT_EQUAL(t, sizeof(rbuf), pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
  TEST_STR_EQUAL(t, data, rbuf);
  pn_delivery_settle(rd);
  return t->errors > errors;
}

#define SEND_RECEIVE_MESSAGE(T, TAG, SRC, DST)                  \
  TEST_INT_EQUAL(T, 0, send_receive_message(T, TAG, SRC, DST))

// Test mixing aborted and good deliveries, make sure credit is correct.
static void test_message_abort_mixed(test_t *t) {
  /* Set up the link, give credit, start the delivery */
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, t, send_client_handler, NULL);
  test_connection_driver_init(&server, t, delivery_handler, NULL);
  pn_transport_set_server(server.driver.transport);
  pn_connection_open(client.driver.connection);

  test_connection_drivers_run(&client, &server);
  pn_link_t *rcv = server.handler.link;
  pn_link_t *snd = client.handler.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */

  /* We will send 3 good messages, interleaved with aborted ones */
  pn_link_flow(rcv, 5);
  test_connection_drivers_run(&client, &server);
  SEND_RECEIVE_MESSAGE(t, "one", &client, &server);
  TEST_INT_EQUAL(t, 4, pn_link_credit(snd));
  TEST_INT_EQUAL(t, 4, pn_link_credit(rcv));
  pn_delivery_t *sd, *rd;

  /* Send a frame, then an abort */
  sd = pn_delivery(snd, PN_BYTES_LITERAL("x1"));
  server.handler.delivery = NULL;
  TEST_INT_EQUAL(t, sizeof(data), pn_link_send(snd, data, sizeof(data)));
  TEST_INT_EQUAL(t, 4, pn_link_credit(snd)); /* Nothing sent yet */
  test_connection_drivers_run(&client, &server);
  rd = server.handler.delivery;
  if (!TEST_CHECK(t, rd)) goto cleanup;
  TEST_INT_EQUAL(t, sizeof(rbuf), pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));

  pn_delivery_abort(sd);
  test_connection_drivers_run(&client, &server);
  TEST_CHECK(t, pn_delivery_aborted(rd));
  pn_delivery_settle(rd);
  /* Abort after sending data consumes credit */
  TEST_INT_EQUAL(t, 3, pn_link_credit(snd));
  TEST_INT_EQUAL(t, 3, pn_link_credit(rcv));

  SEND_RECEIVE_MESSAGE(t, "two", &client, &server);
  TEST_INT_EQUAL(t, 2, pn_link_credit(snd));
  TEST_INT_EQUAL(t, 2, pn_link_credit(rcv));

  /* Abort a delivery before any data has been framed, should be dropped. */
  test_handler_clear(&server.handler, 0);
  sd = pn_delivery(snd, PN_BYTES_LITERAL(4));
  TEST_INT_EQUAL(t, sizeof(data), pn_link_send(snd, data, sizeof(data)));
  pn_delivery_abort(sd);
  TEST_CHECK(t, pn_link_current(snd) != sd); /* Advanced */
  test_connection_drivers_run(&client, &server);
  TEST_HANDLER_EXPECT(&server.handler, PN_TRANSPORT, 0);
  /* Aborting wit no frames sent should leave credit untouched */
  TEST_INT_EQUAL(t, 2, pn_link_credit(snd));
  TEST_INT_EQUAL(t, 2, pn_link_credit(rcv));

  SEND_RECEIVE_MESSAGE(t, "three", &client, &server);
  TEST_INT_EQUAL(t, 1, pn_link_credit(rcv));
  TEST_INT_EQUAL(t, 1, pn_link_credit(snd));

 cleanup:
  test_connection_driver_destroy(&client);
  test_connection_driver_destroy(&server);
}


int main(int argc, char **argv) {
  int failed = 0;
  RUN_ARGV_TEST(failed, t, test_message_transfer(&t));
  RUN_ARGV_TEST(failed, t, test_message_stream(&t));
  RUN_ARGV_TEST(failed, t, test_message_abort(&t));
  RUN_ARGV_TEST(failed, t, test_message_abort_mixed(&t));
  return failed;

}
