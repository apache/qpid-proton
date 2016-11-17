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
#include <proton/connection_driver.h>
#include <proton/delivery.h>
#include <proton/proactor.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <proton/url.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef char str[1024];

typedef struct app_data_t {
  str address;
  str container_id;
  pn_rwbytes_t message_buffer;
  int message_count;
  int sent;
  int acknowledged;
  pn_proactor_t *proactor;
  pn_millis_t delay;
  bool delaying;
  pn_link_t *sender;
  bool finished;
} app_data_t;

int exit_code = 0;

static void check_condition(pn_event_t *e, pn_condition_t *cond) {
  if (pn_condition_is_set(cond)) {
    exit_code = 1;
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
  }
}

/* Create a message with a map { "sequence" : number } encode it and return the encoded buffer. */
static pn_bytes_t encode_message(app_data_t* app) {
  /* Construct a message with the map { "sequence": app.sent } */
  pn_message_t* message = pn_message();
  pn_data_put_int(pn_message_id(message), app->sent); /* Set the message_id also */
  pn_data_t* body = pn_message_body(message);
  pn_data_put_map(body);
  pn_data_enter(body);
  pn_data_put_string(body, pn_bytes(sizeof("sequence")-1, "sequence"));
  pn_data_put_int(body, app->sent); /* The sequence number */
  pn_data_exit(body);

  /* encode the message, expanding the encode buffer as needed */
  if (app->message_buffer.start == NULL) {
    static const size_t initial_size = 128;
    app->message_buffer = pn_rwbytes(initial_size, (char*)malloc(initial_size));
  }
  /* app->message_buffer is the total buffer space available. */
  /* mbuf wil point at just the portion used by the encoded message */
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

static void send(app_data_t* app) {
  while (pn_link_credit(app->sender) > 0 && app->sent < app->message_count) {
    ++app->sent;
    // Use sent counter bytes as unique delivery tag.
    pn_delivery(app->sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
    pn_bytes_t msgbuf = encode_message(app);
    pn_link_send(app->sender, msgbuf.start, msgbuf.size);
    pn_link_advance(app->sender);
    if (app->delay && app->sent < app->message_count) {
      /* If delay is set, wait for TIMEOUT event to send more */
      app->delaying = true;
      pn_proactor_set_timeout(app->proactor, app->delay);
      break;
    }
  }
}

static void handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

   case PN_CONNECTION_INIT: {
     pn_connection_t* c = pn_event_connection(event);
     pn_connection_set_container(c, app->container_id);
     pn_connection_open(c);
     pn_session_t* s = pn_session(c);
     pn_session_open(s);
     pn_link_t* l = pn_sender(s, "my_sender");
     pn_terminus_set_address(pn_link_target(l), app->address);
     pn_link_open(l);
   } break;

   case PN_LINK_FLOW:
    /* The peer has given us some credit, now we can send messages */
    if (!app->delaying) {
      app->sender = pn_event_link(event);
      send(app);
    }
    break;

   case PN_PROACTOR_TIMEOUT:
    /* Wake the sender's connection */
    pn_connection_wake(pn_session_connection(pn_link_session(app->sender)));
    break;

   case PN_CONNECTION_WAKE:
    /* Timeout, we can send more. */
    app->delaying = false;
    send(app);
    break;

   case PN_DELIVERY: {
     /* We received acknowledgedment from the peer that a message was delivered. */
     pn_delivery_t* d = pn_event_delivery(event);
     if (pn_delivery_remote_state(d) == PN_ACCEPTED) {
       if (++app->acknowledged == app->message_count) {
         printf("%d messages sent and acknowledged\n", app->acknowledged);
         pn_connection_close(pn_event_connection(event));
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
    app->finished = true;
    break;

   default: break;
  }
}

static void usage(const char *arg0) {
  fprintf(stderr, "Usage: %s [-a url] [-m message-count] [-d delay-ms]\n", arg0);
  exit(1);
}

int main(int argc, char **argv) {
  /* Default values for application and connection. */
  app_data_t app = {{0}};
  app.message_count = 100;
  const char* urlstr = NULL;

  int opt;
  while((opt = getopt(argc, argv, "a:m:d:")) != -1) {
    switch(opt) {
     case 'a': urlstr = optarg; break;
     case 'm': app.message_count = atoi(optarg); break;
     case 'd': app.delay = atoi(optarg); break;
     default: usage(argv[0]); break;
    }
  }
  if (optind < argc)
    usage(argv[0]);

  snprintf(app.container_id, sizeof(app.container_id), "%s:%d", argv[0], getpid());

  /* Parse the URL or use default values */
  pn_url_t *url = urlstr ? pn_url_parse(urlstr) : NULL;
  const char *host = url ? pn_url_get_host(url) : NULL;
  const char *port = url ? pn_url_get_port(url) : "amqp";
  strncpy(app.address, (url && pn_url_get_path(url)) ? pn_url_get_path(url) : "example", sizeof(app.address));

  /* Create the proactor and connect */
  app.proactor = pn_proactor();
  pn_proactor_connect(app.proactor, pn_connection(), host, port);
  if (url) pn_url_free(url);

  do {
    pn_event_batch_t *events = pn_proactor_wait(app.proactor);
    pn_event_t *e;
    while ((e = pn_event_batch_next(events))) {
      handle(&app, e);
    }
    pn_proactor_done(app.proactor, events);
  } while(!app.finished);

  pn_proactor_free(app.proactor);
  free(app.message_buffer.start);
  return exit_code;
}
