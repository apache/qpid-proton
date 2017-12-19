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

#include <proton/connection.h>
#include <proton/condition.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/session.h>
#include <proton/transport.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * Send aborted messages.
 * 
 * An 80 Kbyte (MSG_SIZE) message is generated.
 * The size is chosen to be large enough so that some of the message
 * will go out on the wire before the abort is sent.
 * 
 * 79 Kbytes (MSG_SIZE - HOLDBACK) are sent on one link flow event.
 * The message is aborted on the next link flow event.
 */

#define MSG_SIZE 80000
#define HOLDBACK  1000

#if MSG_SIZE <= HOLDBACK
#error "MSG_SIZE must be greater than HOLDBACK"
#endif

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;
  const char *container_id;
  int message_count;

  pn_proactor_t *proactor;
  pn_rwbytes_t message_buffer;
  int sent;
  int aborted;
  bool in_progress;
} app_data_t;

static int exit_code = 0;

static void check_condition(pn_event_t *e, pn_condition_t *cond) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    pn_connection_close(pn_event_connection(e));
    exit_code = 1;
  }
}

/* Create a message with a map { "sequence" : number } encode it and return the encoded buffer. */
static pn_bytes_t encode_message(app_data_t* app) {
  /* Construct a message with the map { "sequence": app.sent } */
  pn_message_t* message = pn_message();
  char data[MSG_SIZE + 11] = {0};
  pn_data_t* body;
  pn_data_put_int(pn_message_id(message), app->sent); /* Set the message_id also */
  body = pn_message_body(message);
  pn_data_enter(body);
  pn_data_put_string(body, pn_bytes(MSG_SIZE, data));
  pn_data_exit(body);

  /* encode the message, expanding the encode buffer as needed */
  if (app->message_buffer.start == NULL) {
    static const size_t initial_size = MSG_SIZE + 1000;
    app->message_buffer = pn_rwbytes(initial_size, (char*)malloc(initial_size));
  }
  /* app->message_buffer is the total buffer space available. */
  /* mbuf wil point at just the portion used by the encoded message */
  {
  pn_rwbytes_t mbuf = pn_rwbytes(app->message_buffer.size, app->message_buffer.start);
  int status = 0;
  while ((status = pn_message_encode(message, mbuf.start, &mbuf.size)) == PN_OVERFLOW) {
    app->message_buffer.size *= 2;
    app->message_buffer.start = (char*)realloc(app->message_buffer.start, app->message_buffer.size);
    mbuf.size = app->message_buffer.size;
  }
  if (status != 0) {
    fprintf(stderr, "error encoding message: %s\n", pn_error_text(pn_message_error(message)));
    exit(1);
  }
  pn_message_free(message);
  return pn_bytes(mbuf.size, mbuf.start);
  }
}

/* Returns true to continue, false if finished */
static bool handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_CONNECTION_INIT: {
     pn_connection_t* c = pn_event_connection(event);
     pn_session_t* s = pn_session(pn_event_connection(event));
     pn_connection_set_container(c, app->container_id);
     pn_connection_open(c);
     pn_session_open(s);
     {
     pn_link_t* l = pn_sender(s, "my_sender");
     pn_terminus_set_address(pn_link_target(l), app->amqp_address);
     pn_link_open(l);
     break;
     }
   }

   case PN_LINK_FLOW: {
     /* The peer has given us some credit, now we can send messages */
     pn_link_t *sender = pn_event_link(event);
     while (app->in_progress || (pn_link_credit(sender) > 0 && app->sent < app->message_count)) {
        if (!app->in_progress) {
          pn_bytes_t msgbuf = encode_message(app);
          /* Use sent counter as unique delivery tag. */
          pn_delivery(sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
          pn_link_send(sender, msgbuf.start, msgbuf.size - HOLDBACK); /* Send some part of message */
          app->in_progress = true;
          /* Return from this link flow event and abort the message on next, */
          break;
        } else {
          pn_delivery_t * pnd = pn_link_current(sender);
          pn_delivery_abort(pnd);
          /* aborted delivery is presettled and never ack'd. */
          if (++app->aborted == app->message_count) {
            printf("%d messages started and aborted\n", app->aborted);
            pn_connection_close(pn_event_connection(event));
            /* Continue handling events till we receive TRANSPORT_CLOSED */
          }
          ++app->sent;
          app->in_progress = false;
        }
     }
     break;
   }

   case PN_DELIVERY: {
     /* We received acknowledgement from the peer that a message was delivered. */
     pn_delivery_t* d = pn_event_delivery(event);
     fprintf(stderr, "Aborted deliveries should not receive delivery events. Delivery state %d\n", (int)pn_delivery_remote_state(d));
     pn_connection_close(pn_event_connection(event));
     exit_code=1;
     break;
   }

   case PN_TRANSPORT_CLOSED:
    check_condition(event, pn_transport_condition(pn_event_transport(event)));
    break;

   case PN_CONNECTION_REMOTE_CLOSE:
    check_condition(event, pn_connection_remote_condition(pn_event_connection(event)));
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_SESSION_REMOTE_CLOSE:
    check_condition(event, pn_session_remote_condition(pn_event_session(event)));
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_LINK_REMOTE_CLOSE:
   case PN_LINK_REMOTE_DETACH:
    check_condition(event, pn_link_remote_condition(pn_event_link(event)));
    pn_connection_close(pn_event_connection(event));
    break;

   case PN_PROACTOR_INACTIVE:
    return false;

   default: break;
  }
  return true;
}

void run(app_data_t *app) {
  /* Loop and handle events */
  do {
    pn_event_batch_t *events = pn_proactor_wait(app->proactor);
    pn_event_t* e;
    for (e = pn_event_batch_next(events); e; e = pn_event_batch_next(events)) {
      if (!handle(app, e)) {
        return;
      }
    }
    pn_proactor_done(app->proactor, events);
  } while(true);
}

int main(int argc, char **argv) {
  struct app_data_t app = {0};
  char addr[PN_MAX_ADDR];

  app.container_id = argv[0];   /* Should be unique */
  app.host = (argc > 1) ? argv[1] : "";
  app.port = (argc > 2) ? argv[2] : "amqp";
  app.amqp_address = (argc > 3) ? argv[3] : "examples";
  app.message_count = (argc > 4) ? atoi(argv[4]) : 10;

  app.proactor = pn_proactor();
  pn_proactor_addr(addr, sizeof(addr), app.host, app.port);
  pn_proactor_connect2(app.proactor, NULL, NULL, addr);
  run(&app);
  pn_proactor_free(app.proactor);
  free(app.message_buffer.start);
  return exit_code;
}
