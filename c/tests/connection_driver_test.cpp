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

#include "./pn_test.hpp"

#include <proton/codec.h>
#include <proton/connection.h>
#include <proton/connection_driver.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/transport.h>

#include <string.h>

using Catch::Matchers::EndsWith;
using Catch::Matchers::Equals;
using namespace pn_test;

namespace {

/* Handler that replies to REMOTE_OPEN, stores the opened object on the handler
 */
struct open_handler : pn_test::handler {
  bool handle(pn_event_t *e) CATCH_OVERRIDE {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_REMOTE_OPEN:
      connection = pn_event_connection(e);
      pn_connection_open(connection);
      break;
    case PN_SESSION_REMOTE_OPEN:
      session = pn_event_session(e);
      pn_session_open(session);
      break;
    case PN_LINK_REMOTE_OPEN:
      link = pn_event_link(e);
      pn_link_open(link);
      break;
    default:
      break;
    }
    return false;
  }
};

/* Like open_handler but also reply to REMOTE_CLOSE */
struct open_close_handler : public open_handler {
  bool handle(pn_event_t *e) CATCH_OVERRIDE {
    switch (pn_event_type(e)) {
    case PN_CONNECTION_REMOTE_CLOSE:
      pn_connection_open(pn_event_connection(e));
      break;
    case PN_SESSION_REMOTE_CLOSE:
      pn_session_open(pn_event_session(e));
      break;
    case PN_LINK_REMOTE_CLOSE:
      pn_link_close(pn_event_link(e));
      break;
    default:
      return open_handler::handle(e);
    }
    return false;
  }
};

/* open_handler that returns control on PN_DELIVERY and stores the delivery */
struct delivery_handler : public open_handler {
  bool handle(pn_event_t *e) {
    switch (pn_event_type(e)) {
    case PN_DELIVERY: {
      delivery = pn_event_delivery(e);
      return true;
    }
    default:
      return open_handler::handle(e);
    }
  }
};

} // namespace

/* Blow-by-blow event verification of a single message transfer */
TEST_CASE("driver_message_transfer") {
  open_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  pn_connection_open(d.client.connection);
  pn_session_t *ssn = pn_session(d.client.connection);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  d.run();

  CHECK_THAT(ETYPES(PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_OPEN,
                    PN_SESSION_INIT, PN_SESSION_LOCAL_OPEN, PN_LINK_INIT,
                    PN_LINK_LOCAL_OPEN, PN_CONNECTION_BOUND,
                    PN_CONNECTION_REMOTE_OPEN, PN_SESSION_REMOTE_OPEN,
                    PN_LINK_REMOTE_OPEN),
             Equals(client.log_clear()));

  CHECK_THAT(ETYPES(PN_CONNECTION_INIT, PN_CONNECTION_BOUND,
                    PN_CONNECTION_REMOTE_OPEN, PN_SESSION_INIT,
                    PN_SESSION_REMOTE_OPEN, PN_LINK_INIT, PN_LINK_REMOTE_OPEN,
                    PN_CONNECTION_LOCAL_OPEN, PN_TRANSPORT,
                    PN_SESSION_LOCAL_OPEN, PN_TRANSPORT, PN_LINK_LOCAL_OPEN,
                    PN_TRANSPORT),
             Equals(server.log_clear()));

  pn_link_t *rcv = server.link;
  REQUIRE(rcv);
  REQUIRE(pn_link_is_receiver(rcv));
  pn_link_flow(rcv, 1);
  d.run();
  CHECK_THAT(ETYPES(PN_LINK_FLOW), Equals(client.log_clear()));

  /* Encode and send a message */
  auto_free<pn_message_t, pn_message_free> m(pn_message());
  pn_data_put_string(pn_message_body(m),
                     pn_bytes("abc")); /* Include trailing NULL */
  pn_delivery(snd, pn_bytes("x"));
  pn_message_send(m, snd, NULL);

  d.run();
  CHECK_THAT(ETYPES(PN_TRANSPORT, PN_DELIVERY), Equals(server.log_clear()));

  /* Receive and decode the message */
  pn_delivery_t *dlv = server.delivery;
  REQUIRE(dlv);
  auto_free<pn_message_t, pn_message_free> m2(pn_message());
  pn_rwbytes_t buf2 = {0};
  message_decode(m2, dlv, &buf2);
  pn_data_t *body = pn_message_body(m2);
  pn_data_rewind(body);
  CHECK(pn_data_next(body));
  CHECK(PN_STRING == pn_data_type(body));
  CHECK(3 == pn_data_get_string(pn_message_body(m2)).size);
  CHECK_THAT("abc", Equals(pn_data_get_string(pn_message_body(m2)).start));

  free(buf2.start);
}

namespace {
/* Handler that opens a connection and sender link */
struct send_client_handler : public pn_test::handler {
  bool handle(pn_event_t *e) {
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
      link = pn_event_link(e);
      return true;
    }
    default:
      break;
    }
    return false;
  }
};
} // namespace

/* Send a message in pieces, ensure each can be received before the next is sent
 */
TEST_CASE("driver_message_stream") {
  send_client_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  d.run();
  pn_link_t *rcv = server.link;
  pn_link_t *snd = client.link;
  pn_link_flow(rcv, 1);
  d.run();
  CHECK(PN_LINK_FLOW == client.log_last());
  CHECK(PN_TRANSPORT == server.log_last());

  /* Encode a large (not very) message to send in chunks */
  auto_free<pn_message_t, pn_message_free> m(pn_message());
  char body[1024] = {0};
  pn_data_put_binary(pn_message_body(m), pn_bytes(sizeof(body), body));
  pn_rwbytes_t buf = {0};
  ssize_t size = pn_message_encode2(m, &buf);

  /* Send and receive the message in chunks */
  static const ssize_t CHUNK = 100;
  pn_delivery(snd, pn_bytes("x"));
  pn_rwbytes_t buf2 = {0};
  ssize_t received = 0;
  for (ssize_t i = 0; i < size; i += CHUNK) {
    /* Send a chunk */
    ssize_t c = (i + CHUNK < size) ? CHUNK : size - i;
    CHECK(c == pn_link_send(snd, buf.start + i, c));
    d.run();
    CHECK_THAT(ETYPES(PN_DELIVERY), Equals(server.log_clear()));
    /* Receive a chunk */
    pn_delivery_t *dlv = server.delivery;
    pn_link_t *l = pn_delivery_link(dlv);
    ssize_t dsize = pn_delivery_pending(dlv);
    rwbytes_ensure(&buf2, received + dsize);
    REQUIRE(dsize == pn_link_recv(l, buf2.start + received, dsize));
    received += dsize;
  }
  CHECK(pn_link_advance(snd));
  CHECK(received == size);
  CHECK(!memcmp(buf.start, buf2.start, size));

  free(buf.start);
  free(buf2.start);
}

// Test aborting a delivery
TEST_CASE("driver_message_abort") {
  send_client_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  d.run();
  pn_link_t *rcv = server.link;
  pn_link_t *snd = client.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */

  /* Send 2 frames with data */
  pn_link_flow(rcv, 1);
  CHECK(1 == pn_link_credit(rcv));
  d.run();
  CHECK(1 == pn_link_credit(snd));
  pn_delivery_t *sd = pn_delivery(snd, pn_bytes("1")); /* Sender delivery */
  for (size_t i = 0; i < 2; ++i) {
    CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
    d.run();
    CHECK(server.log_last() == PN_DELIVERY);
    pn_delivery_t *rd = server.delivery;
    CHECK(!pn_delivery_aborted(rd));
    CHECK(pn_delivery_partial(rd));
    CHECK(1 == pn_link_credit(rcv));
    CHECK(sizeof(data) == pn_delivery_pending(rd));
    CHECK(sizeof(rbuf) ==
          pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
    CHECK(0 == pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
    CHECK(1 == pn_link_credit(rcv));
  }
  CHECK(1 == pn_link_credit(snd));
  /* Abort the delivery */
  pn_delivery_abort(sd);
  CHECK(0 == pn_link_credit(snd));
  CHECK(pn_link_current(snd) != sd); /* Settled */
  d.run();
  CHECK(PN_DELIVERY == server.log_last());
  CHECK(0 == pn_link_credit(snd));

  /* Receive the aborted=true frame, should be empty */
  pn_delivery_t *rd = server.delivery;
  CHECK(pn_delivery_aborted(rd));
  CHECK(!pn_delivery_partial(rd)); /* Aborted deliveries are never partial */
  CHECK(pn_delivery_settled(rd));  /* Aborted deliveries are remote settled */
  CHECK(1 == pn_delivery_pending(rd));
  CHECK(PN_ABORTED == pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));
  pn_delivery_settle(rd); /* Must be settled locally to free it */

  CHECK(0 == pn_link_credit(snd));
  CHECK(0 == pn_link_credit(rcv));

  /* Abort a delivery before any data has been framed, should be dropped. */
  pn_link_flow(rcv, 1);
  CHECK(1 == pn_link_credit(rcv));
  d.run();
  client.log_clear();
  server.log_clear();

  sd = pn_delivery(snd, pn_bytes("x"));
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  pn_delivery_abort(sd);
  CHECK(pn_link_current(snd) != sd); /* Settled, possibly freed */
  CHECK_FALSE(d.run());
  CHECK_THAT(server.log_clear(), Equals(etypes()));
  /* Client gets transport/flow after abort to ensure other messages are sent */
  CHECK_THAT(ETYPES(PN_TRANSPORT, PN_LINK_FLOW), Equals(client.log_clear()));
  /* Aborted delivery consumes no credit */
  CHECK(1 == pn_link_credit(rcv));
  CHECK(1 == pn_link_credit(snd));
  CHECK(0 == pn_session_outgoing_bytes(pn_link_session(snd)));
}

void send_receive_message(const std::string &tag, pn_test::driver_pair &d) {
  pn_link_t *l = d.client.handler.link;
  CHECKED_IF(pn_link_credit(l) > 0) {
    pn_delivery_t *sd = pn_delivery(l, pn_dtag(tag.data(), tag.size()));
    d.server.handler.delivery = NULL;
    CHECK(pn_delivery_current(sd));
    CHECK(ssize_t(tag.size()) == pn_link_send(l, tag.data(), tag.size()));
    pn_delivery_settle(sd);
    d.run();
    pn_delivery_t *rd = d.server.handler.delivery;
    d.server.handler.delivery = NULL;
    CHECKED_IF(rd) {
      CHECK(pn_delivery_current(rd));
      std::string rbuf(tag.size() * 2, 'x');
      CHECK(ssize_t(tag.size()) ==
            pn_link_recv(pn_delivery_link(rd), &rbuf[0], rbuf.size()));
      rbuf.resize(tag.size());
      CHECK(tag == rbuf);
    }
    pn_delivery_settle(rd);
  }
}

#define SEND_RECEIVE_MESSAGE(TAG, DP)                                          \
  do {                                                                         \
    INFO("in send_receive_message: " << TAG);                                  \
    send_receive_message(TAG, DP);                                             \
  } while (0)

// Test mixing aborted and good deliveries, make sure credit is correct.
TEST_CASE("driver_message_abort_mixed") {
  send_client_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  d.run();
  pn_link_t *rcv = server.link;
  pn_link_t *snd = client.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */

  /* We will send 3 good messages, interleaved with aborted ones */
  pn_link_flow(rcv, 5);
  d.run();
  SEND_RECEIVE_MESSAGE("one", d);
  CHECK(4 == pn_link_credit(snd));
  CHECK(4 == pn_link_credit(rcv));
  pn_delivery_t *sd, *rd;

  /* Send a frame, then an abort */
  sd = pn_delivery(snd, pn_bytes("x1"));
  server.delivery = NULL;
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  CHECK(4 == pn_link_credit(snd)); /* Nothing sent yet */
  d.run();
  rd = server.delivery;
  REQUIRE(rd);
  CHECK(sizeof(rbuf) == pn_link_recv(pn_delivery_link(rd), rbuf, sizeof(rbuf)));

  pn_delivery_abort(sd);
  d.run();
  CHECK(pn_delivery_aborted(rd));
  pn_delivery_settle(rd);
  /* Abort after sending data consumes credit */
  CHECK(3 == pn_link_credit(snd));
  CHECK(3 == pn_link_credit(rcv));

  SEND_RECEIVE_MESSAGE("two", d);
  CHECK(2 == pn_link_credit(snd));
  CHECK(2 == pn_link_credit(rcv));

  /* Abort a delivery before any data has been framed, should be dropped. */
  server.log.clear();
  sd = pn_delivery(snd, pn_bytes("4"));
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  pn_delivery_abort(sd);
  CHECK(pn_link_current(snd) != sd); /* Advanced */
  d.run();
  CHECK_THAT(ETYPES(PN_TRANSPORT), Equals(server.log_clear()));
  /* Aborting wit no frames sent should leave credit untouched */
  CHECK(2 == pn_link_credit(snd));
  CHECK(2 == pn_link_credit(rcv));

  SEND_RECEIVE_MESSAGE("three", d);
  CHECK(1 == pn_link_credit(rcv));
  CHECK(1 == pn_link_credit(snd));
}

/* Set capacity and max frame, send a single message */
static void set_capacity_and_max_frame(size_t capacity, size_t max_frame,
                                       pn_test::driver_pair &d,
                                       const char *data) {
  pn_transport_set_max_frame(d.client.transport, max_frame);
  pn_connection_open(d.client.connection);
  pn_session_t *ssn = pn_session(d.client.connection);
  pn_session_set_incoming_capacity(ssn, capacity);
  pn_session_open(ssn);
  pn_link_t *snd = pn_sender(ssn, "x");
  pn_link_open(snd);
  d.run();
  pn_link_flow(d.server.handler.link, 1);
  d.run();
  if (pn_transport_closed(d.client.transport) ||
      pn_transport_closed(d.server.transport))
    return;
  /* Send a message */
  auto_free<pn_message_t, pn_message_free> m(pn_message());
  pn_message_set_address(m, data);
  pn_delivery(snd, pn_bytes("x"));
  pn_message_send(m, snd, NULL);
  d.run();
}

/* Test different settings for max-frame, outgoing-window, incoming-capacity */
TEST_CASE("driver_session_flow_control") {
  open_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  auto_free<pn_message_t, pn_message_free> m(pn_message());
  pn_rwbytes_t buf = {0};

  /* Capacity equal to frame size OK */
  set_capacity_and_max_frame(1234, 1234, d, "foo");
  pn_delivery_t *dlv = server.delivery;
  CHECKED_IF(dlv) {
    message_decode(m, dlv, &buf);
    CHECK_THAT("foo", Equals(pn_message_get_address(m)));
  }

  /* Capacity bigger than frame size OK */
  set_capacity_and_max_frame(12345, 1234, d, "foo");
  dlv = server.delivery;
  CHECKED_IF(dlv) {
    message_decode(m, dlv, &buf);
    CHECK_THAT("foo", Equals(pn_message_get_address(m)));
  }

  /* Capacity smaller than frame size is an error */
  set_capacity_and_max_frame(1234, 12345, d, "foo");
  CHECK_THAT(
      *client.last_condition,
      cond_matches("amqp:internal-error",
                   "session capacity 1234 is less than frame size 12345"));
  free(buf.start);
}

/* Regression test for https://issues.apache.org/jira/browse/PROTON-1832.
   Make sure we error on attempt to re-attach an already-attached link name.
   No crash or memory error.
*/
TEST_CASE("driver_duplicate_link_server") {
  open_close_handler client, server;
  pn_test::driver_pair d(client, server);

  pn_connection_open(d.client.connection);
  pn_session_t *ssn = pn_session(d.client.connection);
  pn_session_open(ssn);

  /* Set up link "x" */
  auto_free<pn_link_t, pn_link_free> x(pn_sender(ssn, "xxx"));
  pn_link_open(x);
  d.run();
  client.log.clear();
  server.log.clear();
  /* Free (but don't close) the link and open a new one to generate the invalid
   * double-attach */
  pn_link_open(pn_sender(ssn, "xxx"));
  d.run();

  CHECK_THAT(*pn_transport_condition(d.server.transport),
             cond_matches("amqp:invalid-field", "xxx"));
  CHECK_THAT(*pn_connection_remote_condition(d.client.connection),
             cond_matches("amqp:invalid-field", "xxx"));

  /* Freeing the link at this point is allowed but caused a crash in
   * transport_unbind with the bug */
}

/* Reproducer test for https://issues.apache.org/jira/browse/PROTON-1832.
   Make sure the client does not generate an illegal "attach; attach; detach"
   sequence from a legal "pn_link_open(); pn_link_close(); pn_link_open()"
   sequence

   This test is expected to fail till PROTON-1832 is fully fixed
*/
TEST_CASE("driver_duplicate_link_client", "[!hide][!shouldfail]") {
  /* Set up the initial link */
  open_close_handler client, server;
  pn_test::driver_pair d(client, server);

  pn_session_t *ssn = pn_session(d.client.connection);
  pn_session_open(ssn);
  pn_link_t *x = pn_sender(ssn, "x");
  pn_link_open(x);
  d.run();
  client.log.clear();
  server.log.clear();

  /* Close the link and open a new link with same name in the same batch of
   * events. */
  pn_link_close(x);
  pn_link_open(pn_sender(ssn, "x"));
  d.run();

  CHECK_THAT(ETYPES(PN_LINK_REMOTE_CLOSE, PN_LINK_LOCAL_CLOSE, PN_TRANSPORT,
                    PN_LINK_INIT, PN_LINK_REMOTE_OPEN, PN_LINK_LOCAL_OPEN,
                    PN_TRANSPORT),
             Equals(server.log_clear()));
  CHECK_THAT(*pn_transport_condition(d.server.transport), cond_empty());

  d.run();
  CHECK_THAT(ETYPES(PN_LINK_LOCAL_CLOSE, PN_TRANSPORT, PN_LINK_REMOTE_CLOSE,
                    PN_LINK_INIT, PN_LINK_LOCAL_OPEN, PN_TRANSPORT,
                    PN_LINK_REMOTE_OPEN),
             Equals(client.log_clear()));
  CHECK_THAT(*pn_connection_remote_condition(d.client.connection),
             cond_empty());
}

/* Settling an incomplete delivery should not cause an error.
*/
TEST_CASE("driver_settle_incomplete_receiver") {
  send_client_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  d.run();
  pn_link_t *rcv = server.link;
  pn_link_t *snd = client.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */
  pn_link_flow(rcv, 1);
  pn_delivery(snd, pn_bytes("1")); /* Prepare to send */
  d.run();

  /* Send/receive a frame */
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  server.log_clear();
  d.run();
  CHECK_THAT(ETYPES(PN_DELIVERY), Equals(server.log_clear()));
  CHECK(sizeof(data) == pn_link_recv(rcv, rbuf, sizeof(data)));
  d.run();

  /* Settle early while the sender is still sending */
  pn_delivery_settle(pn_link_current(rcv));
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  d.run();
  CHECK_THAT(*pn_connection_remote_condition(d.client.connection),
             cond_empty());
  CHECK_THAT(*pn_connection_condition(d.server.connection), cond_empty());

  pn_delivery_settle(pn_link_current(snd));

  /* Send/receive a new message, should not cause error */
  pn_link_flow(rcv, 1);
  d.run();
  pn_delivery(snd, pn_bytes("2")); /* Prepare to send */
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  server.log_clear();
  d.run();
  CHECK_THAT(ETYPES(PN_DELIVERY), Equals(server.log_clear()));
  CHECK(sizeof(data) == pn_link_recv(rcv, rbuf, sizeof(data)));
  pn_delivery_tag_t tag = pn_delivery_tag(pn_link_current(rcv));
  CHECK(tag.size == 1);
  CHECK(tag.start[0] == '2');
  CHECK_THAT(*pn_connection_remote_condition(d.client.connection),
             cond_empty());
  CHECK_THAT(*pn_connection_condition(d.server.connection), cond_empty());
}

/* Empty last frame in streaming message.
*/
TEST_CASE("driver_empty_last_frame") {
  send_client_handler client;
  delivery_handler server;
  pn_test::driver_pair d(client, server);

  d.run();
  pn_link_t *rcv = server.link;
  pn_link_t *snd = client.link;
  char data[100] = {0};          /* Dummy data to send. */
  char rbuf[sizeof(data)] = {0}; /* Read buffer for incoming data. */
  pn_link_flow(rcv, 1);
  pn_delivery_t *sd = pn_delivery(snd, pn_bytes("1")); /* Prepare to send */
  d.run();

  /* Send/receive a frame */
  CHECK(sizeof(data) == pn_link_send(snd, data, sizeof(data)));
  server.log_clear();
  d.run();
  CHECK_THAT(ETYPES(PN_DELIVERY), Equals(server.log_clear()));
  CHECK(sizeof(data) == pn_link_recv(rcv, rbuf, sizeof(data)));
  CHECK(pn_delivery_partial(pn_link_current(rcv)));
  d.run();

  /* Advance after all data transfered over wire. */
  CHECK(pn_link_advance(snd));
  server.log_clear();
  d.run();
  CHECK_THAT(ETYPES(PN_DELIVERY), Equals(server.log_clear()));
  CHECK(PN_EOS == pn_link_recv(rcv, rbuf, sizeof(data)));
  CHECK(!pn_delivery_partial(pn_link_current(rcv)));

  pn_delivery_settle(sd);
  sd = NULL;
  pn_delivery_settle(pn_link_current(rcv));
  d.run();
  CHECK_THAT(*pn_connection_remote_condition(d.client.connection),
             cond_empty());
  CHECK_THAT(*pn_connection_condition(d.server.connection), cond_empty());
}
