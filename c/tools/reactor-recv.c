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
 *
 */

/*
 * Implements a subset of msgr-recv.c using reactor events.
 */

#define PN_USE_DEPRECATED_API 1

#include "proton/message.h"
#include "proton/error.h"
#include "proton/types.h"
#include "proton/reactor.h"
#include "proton/handlers.h"
#include "proton/engine.h"
#include "proton/url.h"
#include "msgr-common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// The exact struct from msgr-recv, mostly fallow.
typedef struct {
  Addresses_t subscriptions;
  uint64_t msg_count;
  int recv_count;
  int incoming_window;
  int timeout;  // seconds
  unsigned int report_interval;  // in seconds
  int   outgoing_window;
  int   reply;
  const char *name;
  const char *ready_text;
  char *certificate;
  char *privatekey;   // used to sign certificate
  char *password;     // for private key file
  char *ca_db;        // trusted CA database
} Options_t;


static void usage(int rc)
{
    printf("Usage: reactor-recv [OPTIONS] \n"
           " -c # \tNumber of messages to receive before exiting [0=forever]\n"
           " -R \tSend reply if 'reply-to' present\n"
           " -t # \tInactivity timeout in seconds, -1 = no timeout [-1]\n"
           " -X <text> \tPrint '<text>\\n' to stdout after all subscriptions are created\n"
           );
    exit(rc);
}


// Global context for this process
typedef struct {
  Options_t *opts;
  Statistics_t *stats;
  uint64_t sent;
  uint64_t received;
  pn_message_t *message;
  pn_acceptor_t *acceptor;
  char *encoded_data;
  size_t encoded_data_size;
  int connections;
  pn_list_t *active_connections;
  bool shutting_down;
  pn_handler_t *listener_handler;
  int quiesce_count;
} global_context_t;

// Per connection context
typedef struct {
  global_context_t *global;
  int connection_id;
  pn_link_t *recv_link;
  pn_link_t *reply_link;
} connection_context_t;

/**
 * @return buffer of sufficient size, or NULL
 */
static char *ensure_buffer(char *buf, size_t needed, size_t *actual)
{
  // Make room for the largest message seen so far, plus extra for slight changes in metadata content
  if (needed + 1024 <= *actual)
    return buf;
  needed += 2048;
  buf = (char *) realloc(buf, needed);
  *actual = buf ? needed : 0;
  return buf;
}

void global_shutdown(global_context_t *gc)
{
  if (gc->shutting_down) return;
  gc->shutting_down = true;
  pn_acceptor_close(gc->acceptor);
  size_t n = pn_list_size(gc->active_connections);
  for (size_t i = 0; i < n; i++) {
    pn_connection_t *conn = (pn_connection_t *) pn_list_get(gc->active_connections, i);
    if (!(pn_connection_state(conn) & PN_LOCAL_CLOSED)) {
      pn_connection_close(conn);
    }
  }
}

connection_context_t *connection_context(pn_handler_t *h)
{
  connection_context_t *p = (connection_context_t *) pn_handler_mem(h);
  return p;
}

void connection_context_init(connection_context_t *cc, global_context_t *gc)
{
  cc->global = gc;
  pn_incref(gc->listener_handler);
  cc->connection_id = gc->connections++;
  cc->recv_link = 0;
  cc->reply_link = 0;
}

void connection_cleanup(pn_handler_t *h)
{
  connection_context_t *cc = connection_context(h);
  // Undo pn_incref() from connection_context_init()
  pn_decref(cc->global->listener_handler);
}

void connection_dispatch(pn_handler_t *h, pn_event_t *event, pn_event_type_t type)
{
  connection_context_t *cc = connection_context(h);
  bool replying = cc->global->opts->reply;

  switch (type) {
  case PN_LINK_REMOTE_OPEN:
    {
      pn_link_t *link = pn_event_link(event);
      if (pn_link_is_receiver(link)) {
        check(cc->recv_link == NULL, "Multiple incoming links on one connection");
        cc->recv_link = link;
        pn_connection_t *conn = pn_event_connection(event);
        pn_list_add(cc->global->active_connections, conn);
        if (cc->global->shutting_down) {
          pn_connection_close(conn);
          break;
        }
        if (replying) {
          // Set up a reply link and defer granting credit to the incoming link
          pn_connection_t *conn = pn_session_connection(pn_link_session(link));
          pn_session_t *ssn = pn_session(conn);
          pn_session_open(ssn);
          char name[100]; // prefer a multiplatform uuid generator
          sprintf(name, "reply_sender_%d", cc->connection_id);
          cc->reply_link = pn_sender(ssn, name);
          pn_link_open(cc->reply_link);
        }
        else {
          pn_flowcontroller_t *fc = pn_flowcontroller(1024);
          pn_handler_add(h, fc);
          pn_decref(fc);
        }
      }
    }
    break;
  case PN_LINK_FLOW:
    {
      if (replying) {
        pn_link_t *reply_link = pn_event_link(event);
        // pn_flowcontroller handles the non-reply case
        check(reply_link == cc->reply_link, "internal error");

        // Grant the sender as much credit as just given to us for replies
        int delta = pn_link_credit(reply_link) - pn_link_credit(cc->recv_link);
        if (delta > 0)
          pn_link_flow(cc->recv_link, delta);
      }
    }
    break;
  case PN_DELIVERY:
    {
      pn_link_t *recv_link = pn_event_link(event);
      pn_delivery_t *dlv = pn_event_delivery(event);
      if (pn_link_is_receiver(recv_link) && !pn_delivery_partial(dlv)) {
        if (cc->global->received == 0) statistics_start(cc->global->stats);

        size_t encoded_size = pn_delivery_pending(dlv);
        cc->global->encoded_data = ensure_buffer(cc->global->encoded_data, encoded_size,
                                                 &cc->global->encoded_data_size);
        check(cc->global->encoded_data, "decoding buffer realloc failure");

        ssize_t n = pn_link_recv(recv_link, cc->global->encoded_data, encoded_size);
        check(n == (ssize_t) encoded_size, "message data read fail");
        pn_message_t *msg = cc->global->message;
        int err = pn_message_decode(msg, cc->global->encoded_data, n);
        check(err == 0, "message decode error");
        cc->global->received++;
        pn_delivery_settle(dlv);
        statistics_msg_received(cc->global->stats, msg);

        if (replying) {
          const char *reply_addr = pn_message_get_reply_to(msg);
          if (reply_addr) {
            pn_link_t *rl = cc->reply_link;
            check(pn_link_credit(rl) > 0, "message received without corresponding reply credit");
            LOG("Replying to: %s\n", reply_addr );

            pn_message_set_address(msg, reply_addr);
            pn_message_set_creation_time(msg, msgr_now());

            char tag[8];
            void *ptr = &tag;
            *((uint64_t *) ptr) = cc->global->sent;
            pn_delivery_t *dlv = pn_delivery(rl, pn_dtag(tag, 8));
            size_t size = cc->global->encoded_data_size;
            int err = pn_message_encode(msg, cc->global->encoded_data, &size);
            check(err == 0, "message encoding error");
            pn_link_send(rl, cc->global->encoded_data, size);
            pn_delivery_settle(dlv);

            cc->global->sent++;
          }
        }
      }
      if (cc->global->received >= cc->global->opts->msg_count) {
        global_shutdown(cc->global);
      }
    }
    break;
  case PN_CONNECTION_UNBOUND:
    {
      pn_connection_t *conn = pn_event_connection(event);
      pn_list_remove(cc->global->active_connections, conn);
      pn_connection_release(conn);
    }
    break;
  default:
    break;
  }
}

pn_handler_t *connection_handler(global_context_t *gc)
{
  pn_handler_t *h = pn_handler_new(connection_dispatch, sizeof(connection_context_t), connection_cleanup);
  connection_context_t *cc = connection_context(h);
  connection_context_init(cc, gc);
  return h;
}


void start_listener(global_context_t *gc, pn_reactor_t *reactor)
{
  check(gc->opts->subscriptions.count > 0, "no listening address");
  pn_url_t *listen_url = pn_url_parse(gc->opts->subscriptions.addresses[0]);
  const char *host = pn_url_get_host(listen_url);
  const char *port = pn_url_get_port(listen_url);
  if (port == 0 || strlen(port) == 0)
    port = "5672";
  if (host == 0 || strlen(host) == 0)
    host = "0.0.0.0";
  if (*host == '~') host++;
  gc->acceptor = pn_reactor_acceptor(reactor, host, port, NULL);
  check(gc->acceptor, "acceptor creation failed");
  pn_url_free(listen_url);
}

void global_context_init(global_context_t *gc, Options_t *o, Statistics_t *s)
{
  gc->opts = o;
  gc->stats = s;
  gc->sent = 0;
  gc->received = 0;
  gc->encoded_data_size = 0;
  gc->encoded_data = 0;
  gc->message = pn_message();
  check(gc->message, "failed to allocate a message");
  gc->connections = 0;
  gc->active_connections = pn_list(PN_OBJECT, 0);
  gc->acceptor = 0;
  gc->shutting_down = false;
  gc->listener_handler = 0;
  gc->quiesce_count = 0;
}

global_context_t *global_context(pn_handler_t *h)
{
  return (global_context_t *) pn_handler_mem(h);
}

void listener_cleanup(pn_handler_t *h)
{
  global_context_t *gc = global_context(h);
  pn_message_free(gc->message);
  free(gc->encoded_data);
  pn_free(gc->active_connections);
}

void listener_dispatch(pn_handler_t *h, pn_event_t *event, pn_event_type_t type)
{
  global_context_t *gc = global_context(h);
  if (type == PN_REACTOR_QUIESCED)
    gc->quiesce_count++;
  else
    gc->quiesce_count = 0;

  switch (type) {
  case PN_CONNECTION_INIT:
    {
      pn_connection_t *connection = pn_event_connection(event);

      // New incoming connection on listener socket.  Give each a separate handler.
      pn_handler_t *ch = connection_handler(gc);
      pn_handshaker_t *handshaker = pn_handshaker();
      pn_handler_add(ch, handshaker);
      pn_decref(handshaker);
      pn_record_t *record = pn_connection_attachments(connection);
      pn_record_set_handler(record, ch);
      pn_decref(ch);
    }
    break;
  case PN_REACTOR_QUIESCED:
    {
      // Two quiesce in a row means we have been idle for a timeout period
      if (gc->opts->timeout != -1 && gc->quiesce_count > 1)
        global_shutdown(gc);
    }
    break;
  case PN_REACTOR_INIT:
    {
      pn_reactor_t *reactor = pn_event_reactor(event);
      start_listener(gc, reactor);

      // hack to let test scripts know when the receivers are ready (so
      // that the senders may be started)
      if (gc->opts->ready_text) {
        fprintf(stdout, "%s\n", gc->opts->ready_text);
        fflush(stdout);
      }
      if (gc->opts->timeout != -1)
        pn_reactor_set_timeout(pn_event_reactor(event), gc->opts->timeout);
    }
    break;
  case PN_REACTOR_FINAL:
    {
      if (gc->received == 0) statistics_start(gc->stats);
      statistics_report(gc->stats, gc->sent, gc->received);
    }
    break;
  default:
    break;
  }
}

pn_handler_t *listener_handler(Options_t *opts, Statistics_t *stats)
{
  pn_handler_t *h = pn_handler_new(listener_dispatch, sizeof(global_context_t), listener_cleanup);
  global_context_t *gc = global_context(h);
  global_context_init(gc, opts, stats);
  gc->listener_handler = h;
  return h;
}

static void parse_options( int argc, char **argv, Options_t *opts )
{
    int c;
    opterr = 0;

    memset( opts, 0, sizeof(*opts) );
    opts->recv_count = -1;
    opts->timeout = -1;
    addresses_init( &opts->subscriptions);

    while ((c = getopt(argc, argv,
                       "a:c:b:w:t:e:RW:F:VN:X:T:C:K:P:")) != -1) {
        switch (c) {
        case 'a':
          {
            // TODO: multiple addresses?
            char *comma = strchr(optarg, ',');
            check(comma == 0, "multiple addresses not implemented");
            check(opts->subscriptions.count == 0, "multiple addresses not implemented");
            addresses_merge( &opts->subscriptions, optarg );
          }
          break;
        case 'c':
            if (sscanf( optarg, "%" SCNu64, &opts->msg_count ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 't':
            if (sscanf( optarg, "%d", &opts->timeout ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            if (opts->timeout > 0) opts->timeout *= 1000;
            break;
        case 'R': opts->reply = 1; break;
        case 'V': enable_logging(); break;
        case 'X': opts->ready_text = optarg; break;
        default:
            usage(1);
        }
    }

    if (opts->subscriptions.count == 0) addresses_add( &opts->subscriptions,
                                                       "amqp://~0.0.0.0" );
}

int main(int argc, char** argv)
{
  Options_t opts;
  Statistics_t stats;
  parse_options( argc, argv, &opts );
  pn_reactor_t *reactor = pn_reactor();

  // set up default handlers for our reactor
  pn_handler_t *root = pn_reactor_get_handler(reactor);
  pn_handler_t *lh = listener_handler(&opts, &stats);
  pn_handler_add(root, lh);
  pn_handshaker_t *handshaker = pn_handshaker();
  pn_handler_add(root, handshaker);

  // Omit decrefs else segfault.  Not sure why they are necessary
  // to keep valgrind happy for the connection_handler, but not here.
  // pn_decref(handshaker);
  // pn_decref(lh);

  pn_reactor_run(reactor);
  pn_reactor_free(reactor);

  addresses_free( &opts.subscriptions );
  return 0;
}

#undef PN_USE_DEPRECATED_API
