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
#include <proton/proactor_ext.h>
#include <proton/session.h>
#include <proton/transport.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
void perror_wsa(const char* message) {
    char error_msg[256]; // Buffer for the error message
    DWORD error_code = WSAGetLastError();

    int length = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_msg,
        sizeof(error_msg),
        NULL
    );

    if (length > 0) {
        fprintf(stderr, "%s: %s\n", message, error_msg);
    }
    else {
        fprintf(stderr, "%s: Unknown error code %ld\n", message, error_code);
    }
}
#  undef perror
#  define perror perror_wsa
#else
#  include <errno.h>
#  include <netdb.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR (-1)
#  define closesocket close
#endif

typedef struct app_data_t {
  const char *host, *port;
  const char *amqp_address;
  const char *container_id;
  int message_count;

  pn_proactor_t *proactor;
  pn_message_t *message;
  pn_rwbytes_t message_buffer;
  int sent;
  int acknowledged;
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
static void send_message(app_data_t* app, pn_link_t *sender) {
  /* Construct a message with the map { "sequence": app.sent } */
  pn_data_t* body;
  pn_message_clear(app->message);
  body = pn_message_body(app->message);
  pn_message_set_id(app->message, (pn_atom_t){.type=PN_ULONG, .u.as_ulong=app->sent});
  pn_data_put_map(body);
  pn_data_enter(body);
  pn_data_put_string(body, pn_bytes(sizeof("sequence")-1, "sequence"));
  pn_data_put_int(body, app->sent); /* The sequence number */
  pn_data_exit(body);
  if (pn_message_send(app->message, sender, &app->message_buffer) < 0) {
    fprintf(stderr, "error sending message: %s\n", pn_error_text(pn_message_error(app->message)));
    exit(1);
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
       pn_delivery_settle(d);
       if (++app->acknowledged == app->message_count) {
         printf("%d messages sent and acknowledged\n", app->acknowledged);
         pn_connection_close(pn_event_connection(event));
         /* Continue handling events till we receive TRANSPORT_CLOSED */
       }
     } else {
       fprintf(stderr, "unexpected delivery state %d\n", (int)pn_delivery_remote_state(d));
       pn_connection_close(pn_event_connection(event));
       exit_code=1;
     }
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
    pn_event_t *e;
    for (e = pn_event_batch_next(events); e; e = pn_event_batch_next(events)) {
      if (!handle(app, e)) {
        return;
      }
    }
    pn_proactor_done(app->proactor, events);
  } while(true);
}

inline static bool check_error(int err, const char* message) {
  if (err == SOCKET_ERROR) {
    perror(message);
  }
  return err == SOCKET_ERROR;
}

inline static bool check_socket_error(pn_socket_t err, const char* message) {
    if (err == INVALID_SOCKET) {
        perror(message);
    }
    return err == INVALID_SOCKET;
}

inline static bool checked_gai(const char *node, const char *port, struct addrinfo *hints, struct addrinfo **ai) {
  int err = getaddrinfo(node, port, hints, ai);
  if (err != 0) {
    printf("getaddrinfo: %s", gai_strerror(err));
  }
  return err != 0;
}

static pn_socket_t connect_socket(const char *host, const char *port) {
  struct addrinfo *ai;
  if (checked_gai(host, port, &(struct addrinfo){.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM}, &ai)) return INVALID_SOCKET;
  struct addrinfo *ai_ = ai;
  while (true) {
    pn_socket_t s = socket(ai_->ai_family, ai_->ai_socktype, ai_->ai_protocol);
    if (s != INVALID_SOCKET) {
      int err = connect(s, ai_->ai_addr, ai_->ai_addrlen);
      if (err == 0) {
        freeaddrinfo(ai);
        return s;
      }
    }
    ai_ = ai_->ai_next;
    if (ai_) {
      closesocket(s);
      continue;
    } 
    perror("connect");
    closesocket(s);
    freeaddrinfo(ai);
    return INVALID_SOCKET;
  }
}

int main(int argc, char **argv) {
  struct app_data_t app = {
    .container_id = argv[0],   /* Should be unique */
    .host = (argc > 1) ? argv[1] : NULL,
    .port = (argc > 2) ? argv[2] : "5672",
    .amqp_address = (argc > 3) ? argv[3] : "examples",
    .message_count = (argc > 4) ? atoi(argv[4]) : 10,
    .message = pn_message(),
    .proactor = pn_proactor()};

  pn_socket_t s = connect_socket(app.host, app.port);
  if (s == INVALID_SOCKET) goto error;

  pn_proactor_import_socket(app.proactor, NULL, NULL, s);
  run(&app);

error:
  pn_proactor_free(app.proactor);
  free(app.message_buffer.start);
  pn_message_free(app.message);
  return exit_code;
}
