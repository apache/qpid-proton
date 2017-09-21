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

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;
  const char *container_id;
  int message_count;

  pn_proactor_t *proactor;
  int received;
  bool finished;
  pn_rwbytes_t receiving;       /* Partially received message */
} app_data_t;

static const int BATCH = 1000; /* Batch size for unlimited receive */

static int exit_code = 0;

static void check_condition(pn_event_t *e, pn_condition_t *cond) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    pn_connection_close(pn_event_connection(e));
    exit_code = 1;
  }
}

static void decode_message(pn_rwbytes_t data) {
  pn_message_t *m = pn_message();
  int err = pn_message_decode(m, data.start, data.size);
  if (!err) {
    /* Print the decoded message */
    pn_string_t *s = pn_string(NULL);
    pn_inspect(pn_message_body(m), s);
    printf("%s\n", pn_string_get(s));
    pn_free(s);
    pn_message_free(m);
    free(data.start);
  } else {
    fprintf(stderr, "decode_message: %s\n", pn_code(err));
    exit_code = 1;
  }
}

/* Return true to continue, false to exit */
static bool handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_CONNECTION_INIT: {
     pn_connection_t* c = pn_event_connection(event);
     pn_connection_set_container(c, app->container_id);
     pn_connection_open(c);
     pn_session_t* s = pn_session(c);
     pn_session_open(s);
     pn_link_t* l = pn_receiver(s, "my_receiver");
     pn_terminus_set_address(pn_link_source(l), app->amqp_address);
     pn_link_open(l);
     /* cannot receive without granting credit: */
     pn_link_flow(l, app->message_count ? app->message_count : BATCH);
   } break;

   case PN_DELIVERY: {
     /* A message has been received */
     pn_delivery_t *d = pn_event_delivery(event);
     pn_link_t *r = pn_delivery_link(d);
     if (!pn_delivery_readable(d)) break;
     for (size_t p = pn_delivery_pending(d); p > 0; p = pn_delivery_pending(d)) {
       /* Append data to the receving buffer */
       app->receiving.size += p;
       app->receiving.start = (char*)realloc(app->receiving.start, app->receiving.size);
       int recv = pn_link_recv(r, app->receiving.start + app->receiving.size - p, p);
       if (recv < 0 && recv != PN_EOS) {
         fprintf(stderr, "PN_DELIVERY: pn_link_recv error %s\n", pn_code(recv));
         exit_code = 1;
         break;
       }
     }
     if (!pn_delivery_partial(d)) {
       decode_message(app->receiving);
       app->receiving = pn_rwbytes_null;
       /* Accept the delivery */
       pn_delivery_update(d, PN_ACCEPTED);
       /* done with the delivery, move to the next and free it */
       pn_link_advance(r);
       pn_delivery_settle(d);  /* d is now freed */

       if (app->message_count == 0) {
         /* receive forever - see if more credit is needed */
         if (pn_link_credit(r) < BATCH/2) {
           /* Grant enough credit to bring it up to BATCH: */
           pn_link_flow(r, BATCH - pn_link_credit(r));
         }
       } else if (++app->received >= app->message_count) {
         /* done receiving, close the endpoints */
         printf("%d messages received\n", app->received);
         pn_session_t *ssn = pn_link_session(r);
         pn_link_close(r);
         pn_session_close(ssn);
         pn_connection_close(pn_session_connection(ssn));
       }
     }
   } break;

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
    break;

   default:
    break;
  }
    return true;
}

void run(app_data_t *app) {
  /* Loop and handle events */
  do {
    pn_event_batch_t *events = pn_proactor_wait(app->proactor);
    for (pn_event_t *e = pn_event_batch_next(events); e; e = pn_event_batch_next(events)) {
      if (!handle(app, e) || exit_code != 0) {
        return;
      }
    }
    pn_proactor_done(app->proactor, events);
  } while(true);
}

int main(int argc, char **argv) {
  struct app_data_t app = {0};
  app.container_id = argv[0];   /* Should be unique */
  app.host = (argc > 1) ? argv[1] : "";
  app.port = (argc > 2) ? argv[2] : "amqp";
  app.amqp_address = (argc > 3) ? argv[3] : "examples";
  app.message_count = (argc > 4) ? atoi(argv[4]) : 10;

  /* Create the proactor and connect */
  app.proactor = pn_proactor();
  char addr[PN_MAX_ADDR];
  pn_proactor_addr(addr, sizeof(addr), app.host, app.port);
  pn_proactor_connect(app.proactor, pn_connection(), addr);
  run(&app);
  pn_proactor_free(app.proactor);
  return exit_code;
}
