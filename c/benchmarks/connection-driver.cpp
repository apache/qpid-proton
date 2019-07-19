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

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#include <benchmark/benchmark.h>

#include "proton/connection_driver.h"
#include "proton/engine.h"
#include "proton/log.h"
#include "proton/message.h"
#include "proton/transport.h"
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/sasl.h>


// variant of the receive.c proactor example
// borrowed things from the quiver arrow, too

#define MAX_SIZE 1024

typedef char str[MAX_SIZE];

typedef struct app_data_t {
  str container_id;
  pn_rwbytes_t message_buffer{};
  int message_count;
  int received = 0;
  pn_message_t *message;
  int sent = 0;
  pn_rwbytes_t msgout;
  int credit_window = 5000;
  int acknowledged = 0;
  int closed = 0;
} app_data_t;

static void decode_message(pn_delivery_t *dlv);
static void check_condition(pn_event_t *e, pn_condition_t *cond);
static void shovel(pn_connection_driver_t &sender, pn_connection_driver_t &receiver);

static void handle_receiver(app_data_t *app, pn_event_t *event);
static void handle_sender(app_data_t *app, pn_event_t *event);

const bool VERBOSE = false;
const bool ERRORS = true;

// useful for debugging
static void turn_on_transport_logging(pn_transport_t& transport) {
    pn_log_enable(true);
    pn_transport_trace(&transport,
        PN_TRACE_FRM | PN_TRACE_EVT);
}

static void BM_InitCloseSender(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_InitCloseSender\n");

  for (auto _ : state) {
    pn_connection_driver_t sender;
    if (pn_connection_driver_init(&sender, NULL, NULL) != 0) {
      printf("sender: pn_connection_driver_init failed\n");
      exit(1);
    }

    pn_connection_driver_close(&sender);
    pn_connection_driver_destroy(&sender);
  }

  if (VERBOSE)
    printf("END BM_InitCloseSender\n");
}

BENCHMARK(BM_InitCloseSender)->Unit(benchmark::kMicrosecond);

static void BM_InitCloseReceiver(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_InitCloseReceiver\n");

  for (auto _ : state) {
    pn_connection_driver_t receiver;
    if (pn_connection_driver_init(&receiver, NULL, NULL) != 0) {
      printf("receiver: pn_connection_driver_init failed\n");
      exit(1);
    }

    pn_connection_driver_close(&receiver);
    pn_connection_driver_destroy(&receiver);
  }

  if (VERBOSE)
    printf("END BM_InitCloseReceiver\n");
}

BENCHMARK(BM_InitCloseReceiver)->Unit(benchmark::kMicrosecond);

// sends single message and closes the connection
static void BM_EstablishConnection(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_EstablishConnection\n");

  for (auto _ : state) {
    app_data_t app = {};
    app.message_count = 1;
    app.message = pn_message();
    sprintf(app.container_id, "%s:%06x", "BM_EstablishConnection",
            rand() & 0xffffff);

    pn_connection_driver_t receiver;
    if (pn_connection_driver_init(&receiver, NULL, NULL) != 0) {
      printf("receiver: pn_connection_driver_init failed\n");
      exit(1);
    }

    pn_connection_driver_t sender;
    if (pn_connection_driver_init(&sender, NULL, NULL) != 0) {
      printf("sender: pn_connection_driver_init failed\n");
      exit(1);
    }

    do {
      pn_event_t *event;
      while ((event = pn_connection_driver_next_event(&sender)) != NULL) {
        handle_sender(&app, event);
      }
      shovel(sender, receiver);
      while ((event = pn_connection_driver_next_event(&receiver)) != NULL) {
        handle_receiver(&app, event);
      }
      shovel(receiver, sender);
    } while (app.closed < 2); // until both sender and receiver are closed

    pn_message_free(app.message);

    pn_connection_driver_close(&receiver);
    pn_connection_driver_close(&sender);

    shovel(receiver, sender);
    shovel(sender, receiver);

    // this can take long time, up to 500 ms
    pn_connection_driver_destroy(&receiver);
    pn_connection_driver_destroy(&sender);
  }

  state.SetLabel("connections");
  state.SetItemsProcessed(state.iterations());

  if (VERBOSE)
    printf("END BM_EstablishConnection\n");
}

BENCHMARK(BM_EstablishConnection)->Unit(benchmark::kMicrosecond);

static void BM_SendReceiveMessages(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_SendReceiveMessages\n");

  app_data_t app = {};
  app.message_count = -1; // unlimited
  app.message = pn_message();
  app.credit_window = state.range(0);
  sprintf(app.container_id, "%s:%06x", "BM_SendReceiveMessages",
          rand() & 0xffffff);

  pn_connection_driver_t receiver;
  if (pn_connection_driver_init(&receiver, NULL, NULL) != 0) {
    printf("receiver: pn_connection_driver_init failed\n");
    exit(1);
  }

  pn_connection_driver_t sender;
  if (pn_connection_driver_init(&sender, NULL, NULL) != 0) {
    printf("sender: pn_connection_driver_init failed\n");
    exit(1);
  }

  for (auto _ : state) {
    pn_event_t *event;
    while ((event = pn_connection_driver_next_event(&sender)) != NULL) {
      handle_sender(&app, event);
    }
    shovel(sender, receiver);
    while ((event = pn_connection_driver_next_event(&receiver)) != NULL) {
      handle_receiver(&app, event);
    }
    shovel(receiver, sender);
  }

  pn_connection_driver_close(&receiver);
  pn_connection_driver_close(&sender);

  shovel(receiver, sender);
  shovel(sender, receiver);

  // this can take long time, up to 500 ms
  pn_connection_driver_destroy(&receiver);
  pn_connection_driver_destroy(&sender);

  state.SetLabel("messages");
  state.SetItemsProcessed(app.acknowledged);

  if (VERBOSE)
    printf("END BM_SendReceiveMessages\n");
}

BENCHMARK(BM_SendReceiveMessages)
    ->RangeMultiplier(3)
    ->Range(1, 200000)
    ->ArgName("creditWindow")
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);


///* Create a message with a map { "sequence" : number } encode it and return
/// the encoded buffer. */
static void send_message(app_data_t *app, pn_link_t *sender) {
  /* Construct a message with the map { "sequence": 42 } */
  pn_data_t *body;
  pn_message_clear(app->message);
  body = pn_message_body(app->message);
  pn_data_put_int(pn_message_id(app->message),
                  app->sent); /* Set the message_id also */
  pn_data_put_map(body);
  pn_data_enter(body);
  pn_data_put_string(body, pn_bytes(sizeof("sequence") - 1, "sequence"));
  pn_data_put_int(body, 42); /* The sequence number */
  pn_data_exit(body);
  if (pn_message_send(app->message, sender, &app->message_buffer) < 0) {
    fprintf(stderr, "error sending message: %s\n",
            pn_error_text(pn_message_error(app->message)));
    exit(1);
  }
}

static void handle_sender(app_data_t *app, pn_event_t *event) {
  switch (pn_event_type(event)) {

  case PN_CONNECTION_INIT: {
    pn_connection_t *c = pn_event_connection(event);
    pn_connection_set_container(c, "sendercid");
    pn_connection_open(c);
    pn_session_t *s = pn_session(c);
    pn_session_open(s);

    pn_link_t *l = pn_sender(s, "my_sender");
    pn_terminus_set_address(pn_link_target(l), "example");
    pn_link_set_snd_settle_mode(l, PN_SND_UNSETTLED);
    pn_link_set_rcv_settle_mode(l, PN_RCV_FIRST);
    pn_link_open(l);
  } break;

  case PN_LINK_FLOW: {
    if (VERBOSE)
      printf("BEGIN handle_sender: PN_LINK_FLOW\n");
    /* The peer has given us some credit, now we can send messages */
    pn_link_t *sender = pn_event_link(event);
    while (pn_link_credit(sender) > 0 && app->sent != app->message_count) {
      ++app->sent;
      /* Use sent counter as unique delivery tag. */
      pn_delivery(sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
      send_message(app, sender);
    }
    break;
  }

  case PN_DELIVERY: {
    /* We received acknowledgement from the peer that a message was delivered. */
    pn_delivery_t *d = pn_event_delivery(event);
    if (pn_delivery_remote_state(d) == PN_ACCEPTED) {
      if (VERBOSE)
        printf("got PN_ACCEPTED\n");
      if (++app->acknowledged == app->message_count) {
        if (VERBOSE)
          printf("%d messages sent and acknowledged\n", app->acknowledged);
        pn_connection_close(pn_event_connection(event));
        /* Continue handling events till we receive TRANSPORT_CLOSED */
      }
    } else {
      fprintf(stderr, "unexpected delivery state %d\n", (int) pn_delivery_remote_state(d));
      pn_connection_close(pn_event_connection(event));
      exit(EXIT_FAILURE);
    }
    break;
  }

  case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(event)); /* Complete the open */
    break;

  case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(event));
    break;

  case PN_TRANSPORT_ERROR:
    check_condition(event, pn_transport_condition(pn_event_transport(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_CONNECTION_REMOTE_CLOSE:
    check_condition(event,
                    pn_connection_remote_condition(pn_event_connection(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_SESSION_REMOTE_CLOSE:
    check_condition(event,
                    pn_session_remote_condition(pn_event_session(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_LINK_REMOTE_CLOSE:
  case PN_LINK_REMOTE_DETACH:
    check_condition(event, pn_link_remote_condition(pn_event_link(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_TRANSPORT_CLOSED:
    app->closed++;
    break;

  default:
    break;
  }
}

static void handle_receiver(app_data_t *app, pn_event_t *event) {
  switch (pn_event_type(event)) {

  case PN_CONNECTION_INIT: {
    pn_connection_t *c = pn_event_connection(event);
    pn_connection_set_container(c, app->container_id);
    // pn_connection_open(c);
    pn_session_t *s = pn_session(c);
    pn_session_open(s);

    pn_link_t *l = pn_receiver(s, "my_receiver");
    pn_terminus_set_address(pn_link_source(l), "example");
    pn_link_open(l);
  } break;

  case PN_LINK_REMOTE_OPEN: {
    pn_link_t *l = pn_event_link(event);
    pn_terminus_t *t = pn_link_target(l);
    pn_terminus_t *rt = pn_link_remote_target(l);
    pn_terminus_set_address(t, pn_terminus_get_address(rt));
    pn_link_open(l);
    if (pn_link_is_receiver(l)) {
      pn_link_flow(l, app->credit_window);
    }
    break;
  }

  case PN_DELIVERY: {
    /* A message has been received */
    pn_link_t *link = NULL;
    pn_delivery_t *dlv = pn_event_delivery(event);
    if (pn_delivery_readable(dlv) && !pn_delivery_partial(dlv)) {
      link = pn_delivery_link(dlv);
      decode_message(dlv);
      /* Accept the delivery */
      pn_delivery_update(dlv, PN_ACCEPTED);
      /* done with the delivery, move to the next and free it */
      pn_link_advance(link);
      pn_delivery_settle(dlv); /* dlv is now freed */
    }
    pn_link_flow(link, app->credit_window - pn_link_credit(link));
  } break;

  case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(event)); /* Complete the open */
    break;

  case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(event));
    break;

  case PN_TRANSPORT_ERROR:
    check_condition(event, pn_transport_condition(pn_event_transport(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_CONNECTION_REMOTE_CLOSE:
    check_condition(event,
                    pn_connection_remote_condition(pn_event_connection(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_SESSION_REMOTE_CLOSE:
    check_condition(event,
                    pn_session_remote_condition(pn_event_session(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_LINK_REMOTE_CLOSE:
  case PN_LINK_REMOTE_DETACH:
    check_condition(event, pn_link_remote_condition(pn_event_link(event)));
    pn_connection_close(pn_event_connection(event));
    break;

  case PN_TRANSPORT_CLOSED:
    app->closed++;
    break;

  default:
    break;
  }
}

static void check_condition(pn_event_t *e, pn_condition_t *cond) {
  if (VERBOSE)
    printf("beginning check_condition\n");
  if (pn_condition_is_set(cond)) {
    if (VERBOSE || ERRORS)
      fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
              pn_condition_get_name(cond), pn_condition_get_description(cond));
  }
}

///* Copies output from first connection driver into input of the second. */
static void shovel(pn_connection_driver_t &sender, pn_connection_driver_t &receiver) {
  pn_bytes_t wbuf = pn_connection_driver_write_buffer(&receiver);
  if (wbuf.size == 0) {
    pn_connection_driver_write_done(&receiver, 0);
    return;
  }

  pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&sender);
  if (rbuf.start == NULL) {
    printf("shovel: rbuf.start is null\n");
    fflush(stdout);
    fflush(stderr);
    exit(1);
  }

  size_t s = rbuf.size < wbuf.size ? rbuf.size : wbuf.size;
  memcpy(rbuf.start, wbuf.start, s);

  pn_connection_driver_read_done(&sender, s);
  pn_connection_driver_write_done(&receiver, s);
}

static void decode_message(pn_delivery_t *dlv) {
  static char buffer[MAX_SIZE];
  ssize_t len;
  // try to decode the message body
  if (pn_delivery_pending(dlv) < MAX_SIZE) {
    // read in the raw data
    len = pn_link_recv(pn_delivery_link(dlv), buffer, MAX_SIZE);
    if (len > 0) {
      // decode it into a proton message
      pn_message_t *m = pn_message();
      if (PN_OK == pn_message_decode(m, buffer, len)) {
        pn_string_t *s = pn_string(NULL);
        pn_inspect(pn_message_body(m), s);
        if (VERBOSE)
          printf("%s\n", pn_string_get(s));
        pn_free(s);
      }
      pn_message_free(m);
    }
  }
}
