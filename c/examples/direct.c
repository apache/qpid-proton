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

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/session.h>
#include <proton/transport.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;
  const char *container_id;
  int message_count;

  pn_proactor_t *proactor;
  pn_listener_t *listener;
  pn_rwbytes_t msgin, msgout;   /* Buffers for incoming/outgoing messages */

  /* Sender values */
  int sent;
  int acknowledged;
  pn_link_t *sender;

  /* Receiver values */
  int received;
} app_data_t;

static const int BATCH = 1000; /* Batch size for unlimited receive */

static int exit_code = 0;

/* Close the connection and the listener so so we will get a
 * PN_PROACTOR_INACTIVE event and exit, once all outstanding events
 * are processed.
 */
static void close_all(pn_connection_t *c, app_data_t *app) {
  if (c) pn_connection_close(c);
  if (app->listener) pn_listener_close(app->listener);
}

static void check_condition(pn_event_t *e, pn_condition_t *cond, app_data_t *app) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    close_all(pn_event_connection(e), app);
    exit_code = 1;
  }
}

/* Create a message with a map { "sequence" : number } encode it and return the encoded buffer. */
static void send_message(app_data_t *app, pn_link_t *sender) {
  /* Construct a message with the map { "sequence": app.sent } */
  pn_message_t* message = pn_message();
  pn_data_t* body = pn_message_body(message);
  pn_data_put_int(pn_message_id(message), app->sent); /* Set the message_id also */
  pn_data_put_map(body);
  pn_data_enter(body);
  pn_data_put_string(body, pn_bytes(sizeof("sequence")-1, "sequence"));
  pn_data_put_int(body, app->sent); /* The sequence number */
  pn_data_exit(body);
  if (pn_message_send(message, sender, &app->msgout) < 0) {
    fprintf(stderr, "send error: %s\n", pn_error_text(pn_message_error(message)));
    exit_code = 1;
  }
  pn_message_free(message);
}

static void decode_message(pn_rwbytes_t data) {
  pn_message_t *m = pn_message();
  int err = pn_message_decode(m, data.start, data.size);
  if (!err) {
    /* Print the decoded message */
    pn_string_t *s = pn_string(NULL);
    pn_inspect(pn_message_body(m), s);
    printf("%s\n", pn_string_get(s));
    fflush(stdout);
    pn_free(s);
    pn_message_free(m);
    free(data.start);
  } else {
    fprintf(stderr, "decode error: %s\n", pn_error_text(pn_message_error(m)));
    exit_code = 1;
  }
}

/* This function handles events when we are acting as the receiver */
static void handle_receive(app_data_t *app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_LINK_REMOTE_OPEN: {
     pn_link_t *l = pn_event_link(event);
     pn_link_open(l);
     pn_link_flow(l, app->message_count ? app->message_count : BATCH);
   } break;

   case PN_DELIVERY: {          /* Incoming message data */
     pn_delivery_t *d = pn_event_delivery(event);
     if (pn_delivery_readable(d)) {
     pn_link_t *l = pn_delivery_link(d);
       size_t size = pn_delivery_pending(d);
       pn_rwbytes_t* m = &app->msgin; /* Append data to incoming message buffer */
       ssize_t recv;
       m->size += size;
       m->start = (char*)realloc(m->start, m->size);
       recv = pn_link_recv(l, m->start, m->size);
       if (recv == PN_ABORTED) {
         printf("Message aborted\n");
         fflush(stdout);
         m->size = 0;           /* Forget the data we accumulated */
         pn_delivery_settle(d); /* Free the delivery so we can receive the next message */
         pn_link_flow(l, 1);    /* Replace credit for aborted message */
       } else if (recv < 0 && recv != PN_EOS) {        /* Unexpected error */
         pn_condition_format(pn_link_condition(l), "broker", "PN_DELIVERY error: %s", pn_code(recv));
         pn_link_close(l);               /* Unexpected error, close the link */
       } else if (!pn_delivery_partial(d)) { /* Message is complete */
         decode_message(*m);
         *m = pn_rwbytes_null;
         pn_delivery_update(d, PN_ACCEPTED);
         pn_delivery_settle(d);  /* settle and free d */
         if (app->message_count == 0) {
           /* receive forever - see if more credit is needed */
           if (pn_link_credit(l) < BATCH/2) {
             pn_link_flow(l, BATCH - pn_link_credit(l));
           }
         } else if (++app->received >= app->message_count) {
           printf("%d messages received\n", app->received);
           close_all(pn_event_connection(event), app);
         }
       }
     }
     break;
   }
   default:
    break;
  }
}

/* This function handles events when we are acting as the sender */
static void handle_send(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_LINK_REMOTE_OPEN: {
     pn_link_t* l = pn_event_link(event);
     pn_terminus_set_address(pn_link_target(l), app->amqp_address);
     pn_link_open(l);
   } break;

   case PN_LINK_FLOW: {
     /* The peer has given us some credit, now we can send messages */
     pn_link_t *sender = pn_event_link(event);
     while (pn_link_credit(sender) > 0 && app->sent < app->message_count) {
       ++app->sent;
       /* Use sent counter as unique delivery tag. */
       pn_delivery(sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
       send_message(app, sender);
     }
     break;
   }

   case PN_DELIVERY: {
     /* We received acknowledgement from the peer that a message was delivered. */
     pn_delivery_t* d = pn_event_delivery(event);
     if (pn_delivery_remote_state(d) == PN_ACCEPTED) {
       if (++app->acknowledged == app->message_count) {
         printf("%d messages sent and acknowledged\n", app->acknowledged);
         close_all(pn_event_connection(event), app);
       }
     }
   } break;

   default:
    break;
  }
}

/* Handle all events, delegate to handle_send or handle_receive depending on link mode.
   Return true to continue, false to exit
*/
static bool handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_LISTENER_OPEN: {
     char port[256];             /* Get the listening port */
     pn_netaddr_host_port(pn_listener_addr(pn_event_listener(event)), NULL, 0, port, sizeof(port));
     printf("listening on %s\n", port);
     fflush(stdout);
     break;
   }
   case PN_LISTENER_ACCEPT:
    pn_listener_accept2(pn_event_listener(event), NULL, NULL);
    break;

   case PN_CONNECTION_INIT:
    pn_connection_set_container(pn_event_connection(event), app->container_id);
    break;

   case PN_CONNECTION_BOUND: {
     /* Turn off security */
     pn_transport_t *t = pn_event_transport(event);
     pn_transport_require_auth(t, false);
     pn_sasl_allowed_mechs(pn_sasl(t), "ANONYMOUS");
     break;
   }
   case PN_CONNECTION_REMOTE_OPEN: {
     pn_connection_open(pn_event_connection(event)); /* Complete the open */
     break;
   }

   case PN_SESSION_REMOTE_OPEN: {
     pn_session_open(pn_event_session(event));
     break;
   }

   case PN_TRANSPORT_CLOSED:
    check_condition(event, pn_transport_condition(pn_event_transport(event)), app);
    break;

   case PN_CONNECTION_REMOTE_CLOSE:
    check_condition(event, pn_connection_remote_condition(pn_event_connection(event)), app);
    pn_connection_close(pn_event_connection(event)); /* Return the close */
    break;

   case PN_SESSION_REMOTE_CLOSE:
    check_condition(event, pn_session_remote_condition(pn_event_session(event)), app);
    pn_session_close(pn_event_session(event)); /* Return the close */
    pn_session_free(pn_event_session(event));
    break;

   case PN_LINK_REMOTE_CLOSE:
   case PN_LINK_REMOTE_DETACH:
    check_condition(event, pn_link_remote_condition(pn_event_link(event)), app);
    pn_link_close(pn_event_link(event)); /* Return the close */
    pn_link_free(pn_event_link(event));
    break;

   case PN_PROACTOR_TIMEOUT:
    /* Wake the sender's connection */
    pn_connection_wake(pn_session_connection(pn_link_session(app->sender)));
    break;

   case PN_LISTENER_CLOSE:
    app->listener = NULL;        /* Listener is closed */
    check_condition(event, pn_listener_condition(pn_event_listener(event)), app);
    break;

   case PN_PROACTOR_INACTIVE:
    return false;
    break;

   default: {
     pn_link_t *l = pn_event_link(event);
     if (l) {                      /* Only delegate link-related events */
       if (pn_link_is_sender(l)) {
         handle_send(app, event);
       } else {
         handle_receive(app, event);
       }
     }
   }
  }
  return exit_code == 0;
}

void run(app_data_t *app) {
  /* Loop and handle events */
  do {
    pn_event_batch_t *events = pn_proactor_wait(app->proactor);
    pn_event_t *e;
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

  /* Create the proactor and connect */
  app.proactor = pn_proactor();
  app.listener = pn_listener();
  pn_proactor_addr(addr, sizeof(addr), app.host, app.port);
  pn_proactor_listen(app.proactor, app.listener, addr, 16);
  run(&app);
  pn_proactor_free(app.proactor);
  free(app.msgout.start);
  free(app.msgin.start);
  return exit_code;
}
