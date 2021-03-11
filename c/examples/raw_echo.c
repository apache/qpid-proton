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

#include "thread.h"

#include <proton/condition.h>
#include <proton/raw_connection.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;

  pn_proactor_t *proactor;
  pn_listener_t *listener;

  pthread_mutex_t lock;
  int64_t first_idle_time;
  int64_t wake_conn_time;
  int connects;
  int disconnects;

  /* Sender values */

  /* Receiver values */
} app_data_t;

#define MAX_CONNECTIONS 5

typedef struct conn_data_t {
  pn_raw_connection_t *connection;
  int64_t last_recv_time;
  int bytes;
  int buffers;
} conn_data_t;

static conn_data_t conn_data[MAX_CONNECTIONS] = {{0}};

static int exit_code = 0;

/* Close the connection and the listener so so we will get a
 * PN_PROACTOR_INACTIVE event and exit, once all outstanding events
 * are processed.
 */
static void close_all(pn_raw_connection_t *c, app_data_t *app) {
  if (c) pn_raw_connection_close(c);
  if (app->listener) pn_listener_close(app->listener);
}

static bool check_condition(pn_event_t *e, pn_condition_t *cond, app_data_t *app) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    return true;
  }

  return false;
}

static void check_condition_fatal(pn_event_t *e, pn_condition_t *cond, app_data_t *app) {
  if (check_condition(e, cond, app)) {
    close_all(pn_event_raw_connection(e), app);
    exit_code = 1;
  }
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
  // If message not accepted just throw it away!
  if (pn_raw_connection_write_buffers(c, &buffer, 1) < 1) {
    printf("**Couldn't send message: write not accepted**\n");
    free(buf);
  }
}

static void recv_message(pn_raw_buffer_t buf) {
  fwrite(buf.bytes, buf.size, 1, stdout);
}

conn_data_t *make_conn_data(pn_raw_connection_t *c) {
  int i;
  for (i = 0; i < MAX_CONNECTIONS; ++i) {
    if (!conn_data[i].connection) {
      conn_data[i].connection = c;
      return &conn_data[i];
    }
  }
  return NULL;
}

void free_conn_data(conn_data_t *c) {
  if (!c) return;
  c->connection = NULL;
}

#define READ_BUFFERS 4

static void free_buffers(pn_raw_buffer_t buffs[], size_t n) {
  unsigned i;
  for (i=0; i<n; ++i) {
    free(buffs[i].bytes);
  }
}

/* This function handles events when we are acting as the receiver */
static void handle_receive(app_data_t *app, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_RAW_CONNECTION_CONNECTED: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      conn_data_t *cd = (conn_data_t *) pn_raw_connection_get_context(c);
      pn_raw_buffer_t buffers[READ_BUFFERS] = {{0}};
      if (cd) {
        int i = READ_BUFFERS;
        printf("**raw connection %tu connected\n", cd-conn_data);
        pthread_mutex_lock(&app->lock);
        app->connects++;
        pthread_mutex_unlock(&app->lock);
        for (; i; --i) {
          pn_raw_buffer_t *buff = &buffers[READ_BUFFERS-i];
          buff->bytes = (char*) malloc(1024);
          buff->capacity = 1024;
          buff->size = 0;
          buff->offset = 0;
        }
        pn_raw_connection_give_read_buffers(c, buffers, READ_BUFFERS);
      } else {
        printf("**too many raw connections connected: closing\n");
        pn_raw_connection_close(c);
      }
    } break;

    case PN_RAW_CONNECTION_WAKE: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      conn_data_t *cd = (conn_data_t *) pn_raw_connection_get_context(c);
      printf("**raw connection %tu woken\n", cd-conn_data);
    } break;

    case PN_RAW_CONNECTION_DISCONNECTED: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      conn_data_t *cd = (conn_data_t *) pn_raw_connection_get_context(c);
      if (cd) {
        pthread_mutex_lock(&app->lock);
        app->disconnects++;
        pthread_mutex_unlock(&app->lock);
        printf("**raw connection %tu disconnected: bytes: %d, buffers: %d\n", cd-conn_data, cd->bytes, cd->buffers);
      } else {
        printf("**raw connection disconnected: not connected\n");
      }
      check_condition(event, pn_raw_connection_condition(c), app);
      pn_raw_connection_wake(c);
      free_conn_data(cd);
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

    case PN_RAW_CONNECTION_NEED_READ_BUFFERS: {
    } break;

    case PN_RAW_CONNECTION_READ: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      conn_data_t *cd = (conn_data_t *) pn_raw_connection_get_context(c);
      pn_raw_buffer_t buffs[READ_BUFFERS];
      size_t n;
      cd->last_recv_time = pn_proactor_now_64();
      while ( (n = pn_raw_connection_take_read_buffers(c, buffs, READ_BUFFERS)) ) {
        unsigned i;
        for (i=0; i<n && buffs[i].bytes; ++i) {
          cd->bytes += buffs[i].size;
          recv_message(buffs[i]);
        }
        cd->buffers += n;

        // Echo back if we can
        if (!pn_raw_connection_is_write_closed(c)) {
          pn_raw_connection_write_buffers(c, buffs, n);
        } else if (!pn_raw_connection_is_read_closed(c)) {
          pn_raw_connection_give_read_buffers(c, buffs, n);
        } else {
          free_buffers(buffs, n);
        }
      }
    } break;

    case PN_RAW_CONNECTION_CLOSED_READ: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      if (!pn_raw_connection_is_write_closed(c)) {
        send_message(c, "** Goodbye **");
      }
    }
    case PN_RAW_CONNECTION_CLOSED_WRITE:{
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
          free_buffers(buffs, n);
        }
      };
    } break;
    default:
      break;
  }
}

#define WRITE_BUFFERS 4

/* Handle all events, delegate to handle_send or handle_receive
   Return true to continue, false to exit
*/
static bool handle(app_data_t* app, pn_event_t* event) {
  switch (pn_event_type(event)) {

    case PN_LISTENER_OPEN: {
      char port[256];             /* Get the listening port */
      pn_netaddr_host_port(pn_listener_addr(pn_event_listener(event)), NULL, 0, port, sizeof(port));
      printf("**listening on %s\n", port);
      fflush(stdout);
      break;
    }
    case PN_LISTENER_ACCEPT: {
      pn_listener_t *listener = pn_event_listener(event);
      pn_raw_connection_t *c = pn_raw_connection();
      void *cd = make_conn_data(c);
      int64_t now = pn_proactor_now_64();

      if (cd) {
        pthread_mutex_lock(&app->lock);
        app->first_idle_time = 0;
        if (app->wake_conn_time < now) {
          app->wake_conn_time = now + 5000;
          pn_proactor_set_timeout(pn_listener_proactor(listener), 5000);
        }
        pn_raw_connection_set_context(c, cd);

        pn_listener_raw_accept(listener, c);
        pthread_mutex_unlock(&app->lock);
      } else {
        printf("**too many connections, trying again later...\n");

        /* No other sensible/correct way to reject connection - have to defer closing to event handler */
        pn_raw_connection_set_context(c, 0);
        pn_listener_raw_accept(listener, c);
      }

    } break;

    case PN_LISTENER_CLOSE: {
      pn_listener_t *listener = pn_event_listener(event);
      app->listener = NULL;        /* Listener is closed */
      printf("**listener closed\n");
      check_condition_fatal(event, pn_listener_condition(listener), app);
    } break;

    case PN_PROACTOR_TIMEOUT: {
      pn_proactor_t *proactor = pn_event_proactor(event);
      pthread_mutex_lock(&app->lock);
      int64_t now = pn_proactor_now_64();
      pn_millis_t timeout = 5000;
      if (app->connects - app->disconnects == 0) {
        timeout = 20000;
        if (app->first_idle_time == 0) {
          printf("**idle detected, shutting down in %dms\n", timeout);
          app->first_idle_time = now;
        } else if (app->first_idle_time + 20000 <= now) {
          printf("**no activity for %dms: shutting down now\n", timeout);
          pn_listener_close(app->listener);
          break; // No more timeouts
        }
      } else if (now >= app->wake_conn_time) {
        int i;
        for (i = 0; i < MAX_CONNECTIONS; ++i) {
          if (conn_data[i].connection) pn_raw_connection_wake(conn_data[i].connection);
        }
        app->wake_conn_time = now + 5000;
      }
      pn_proactor_set_timeout(proactor, timeout);
      pthread_mutex_unlock(&app->lock);
    }  break;

    case PN_PROACTOR_INACTIVE:
    case PN_PROACTOR_INTERRUPT: {
      pn_proactor_t *proactor = pn_event_proactor(event);
      pn_proactor_interrupt(proactor);
      return false;
    } break;

    default: {
      pn_raw_connection_t *c = pn_event_raw_connection(event);
      if (c) {
          handle_receive(app, event);
      }
    }
  }
  return exit_code == 0;
}

void* run(void *arg) {
  app_data_t *app = arg;

  /* Loop and handle events */
  bool again = true;
  do {
    pn_event_batch_t *events = pn_proactor_wait(app->proactor);
    pn_event_t *e;
    for (e = pn_event_batch_next(events); e && again; e = pn_event_batch_next(events)) {
      again = handle(app, e);
    }
    pn_proactor_done(app->proactor, events);
  } while(again);
  return NULL;
}


int main(int argc, char **argv) {
  struct app_data_t app = {0};
  pthread_mutex_init(&app.lock, NULL);

  char addr[PN_MAX_ADDR];
  app.host = (argc > 1) ? argv[1] : "";
  app.port = (argc > 2) ? argv[2] : "amqp";

  /* Create the proactor and connect */
  app.proactor = pn_proactor();
  app.listener = pn_listener();
  pn_proactor_addr(addr, sizeof(addr), app.host, app.port);
  pn_proactor_listen(app.proactor, app.listener, addr, 16);

  size_t thread_count = 3; 
  pthread_t* threads = (pthread_t*)calloc(sizeof(pthread_t), thread_count);
  int n;
  for (n=0; n<thread_count; n++) {
    int rc = pthread_create(&threads[n], 0, run, (void*)&app);
    if (rc) {
      fprintf(stderr, "Failed to create thread\n");
      exit(-1);
    }
  }
  run(&app);

  for (n=0; n<thread_count; n++) {
    pthread_join(threads[n], 0);
  }

  pn_proactor_free(app.proactor);
  return exit_code;
}
