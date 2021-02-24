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
#include <proton/raw_connection.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;

  pn_proactor_t *proactor;
  pn_listener_t *listener;
  pn_raw_connection_t *towake;

  int connects;
  int disconnects;

  /* Sender values */

  /* Receiver values */
} app_data_t;

typedef struct connection_data_t {
  bool sender;
} connection_data_t;

static int exit_code = 0;

/* Close the connection and the listener so so we will get a
 * PN_PROACTOR_INACTIVE event and exit, once all outstanding events
 * are processed.
 */
static void close_all(pn_raw_connection_t *c, app_data_t *app) {
  if (c) pn_raw_connection_close(c);
  if (app->listener) pn_listener_close(app->listener);
}

static int check_condition(pn_event_t *e, pn_condition_t *cond, app_data_t *app) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    close_all(pn_event_raw_connection(e), app);
    return 1;
  }
  return 0;
}

static void send_message(pn_raw_connection_t *c, const char* msg) {
  pn_raw_buffer_t buffer;
  uint32_t len = strlen(msg);
  char *buf = (char*) malloc(1024);
  memcpy(buf, msg, len);
  buffer.bytes = buf;
  buffer.capacity = 1024;
  buffer.offset = 0;
  buffer.size = len;
  pn_raw_connection_write_buffers(c, &buffer, 1);
}

static void recv_message(pn_raw_buffer_t buf) {
  fwrite(buf.bytes, buf.size, 1, stdout);
}

connection_data_t *make_receiver_data(void) {
  connection_data_t *cd = (connection_data_t*) malloc(sizeof(connection_data_t));
  cd->sender = false;
  return cd;
}

connection_data_t *make_sender_data(void) {
  connection_data_t *cd = (connection_data_t*) malloc(sizeof(connection_data_t));
  cd->sender = true;
  return cd;
}

#define READ_BUFFERS 4

/* This function handles events when we are acting as the receiver */
static void handle_receive(app_data_t *app, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_RAW_CONNECTION_CONNECTED: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      pn_raw_buffer_t buffers[READ_BUFFERS] = {{0}};
      int i = READ_BUFFERS;
      for (; i; --i) {
        pn_raw_buffer_t *buff = &buffers[READ_BUFFERS-i];
        buff->bytes = (char*) malloc(1024);
        buff->capacity = 1024;
        buff->size = 0;
        buff->offset = 0;
      }
      pn_raw_connection_give_read_buffers(c, buffers, READ_BUFFERS);
    } break;

    case PN_RAW_CONNECTION_NEED_READ_BUFFERS: {
    } break;

    default:
      break;
  }
}

#define WRITE_BUFFERS 4

static void free_buffers(pn_raw_buffer_t buffs[], size_t n) {
  unsigned i;
  for (i=0; i<n; ++i) {
    free(buffs[i].bytes);
  }
}

/* This function handles events when we are acting as the sender */
static void handle_send(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_RAW_CONNECTION_CONNECTED: {
      printf("**raw connection connected\n");
      app->connects++;
    } break;

    case PN_RAW_CONNECTION_DISCONNECTED: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      connection_data_t *cd = (connection_data_t*) pn_raw_connection_get_context(c);
      pn_raw_connection_set_context(c, NULL);
      free(cd);
      printf("**raw connection disconnected\n");
      pn_proactor_cancel_timeout(app->proactor);
      app->disconnects++;
      check_condition(event, pn_raw_connection_condition(c), app);
    } break;

    case PN_RAW_CONNECTION_DRAIN_BUFFERS: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      pn_raw_buffer_t buffs[READ_BUFFERS];
      size_t n;
      while ( (n = pn_raw_connection_take_read_buffers(c, buffs, READ_BUFFERS)) ) {
        free_buffers(buffs, n);
      }
      while ( (n = pn_raw_connection_take_written_buffers(c, buffs, READ_BUFFERS)) ) {
        free_buffers(buffs, n);
      }
    }

    case PN_RAW_CONNECTION_NEED_WRITE_BUFFERS: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      char line[120];
      if (fgets(line, sizeof(line), stdin)) {
        send_message(c, line);
      } else {
        /* On end of file, close for write then wait 2 sec for response */
        pn_raw_connection_write_close(c);
        app->towake = c;
        pn_proactor_set_timeout(app->proactor, 2000);
      }
    } break;

    /* This path handles received bytes */
    case PN_RAW_CONNECTION_READ: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      pn_raw_buffer_t buffs[READ_BUFFERS];
      size_t n;
      while ( (n = pn_raw_connection_take_read_buffers(c, buffs, READ_BUFFERS)) ) {
        unsigned i;
        for (i=0; i<n && buffs[i].bytes; ++i) {
          recv_message(buffs[i]);
          free(buffs[i].bytes);
        }
      }
    } break;

    /* This is signal to close 2 secs after input closes */
    case PN_RAW_CONNECTION_WAKE:

    /* This is signal the other end of the network closed */
    case PN_RAW_CONNECTION_CLOSED_READ: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      pn_raw_connection_close(c);
    } break;

    case PN_RAW_CONNECTION_WRITTEN: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      pn_raw_buffer_t buffs[READ_BUFFERS];
      size_t n;
      while ( (n = pn_raw_connection_take_written_buffers(c, buffs, READ_BUFFERS)) ) {
        if (!pn_raw_connection_is_read_closed(c)) {
          pn_raw_connection_give_read_buffers(c, buffs, n);
        } else {
          unsigned i;
          for (i=0; i<n && buffs[i].bytes; ++i) {
            free(buffs[i].bytes);
          }
        }
      };
    } break;

    default:
      break;
  }
}

/* Handle all events, delegate to handle_send or handle_receive
   Return true to continue, false to exit
*/
static bool handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_PROACTOR_TIMEOUT: {
      if (app->towake) {
        pn_raw_connection_wake(app->towake);
        app->towake = NULL;
      }
    }  break;

    case PN_PROACTOR_INACTIVE: {
      return false;
    } break;

    default: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      if (c) {
        connection_data_t *cd = (connection_data_t*) pn_raw_connection_get_context(c);
        if (cd && cd->sender) {
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
  pn_raw_connection_t *c = pn_raw_connection();
  connection_data_t *cd = make_sender_data();

  app.host = (argc > 1) ? argv[1] : "";
  app.port = (argc > 2) ? argv[2] : "amqp";

  /* Create the proactor and connect */
  app.proactor = pn_proactor();
  pn_raw_connection_set_context(c, cd);
  pn_proactor_addr(addr, sizeof(addr), app.host, app.port);
  pn_proactor_raw_connect(app.proactor, c, addr);
  run(&app);
  pn_proactor_free(app.proactor);
  return exit_code;
}
