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

#include "thread.h"

#include <proton/engine.h>
#include <proton/listener.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/transport.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple re-sizable vector that acts as a queue */
#define VEC(T) struct { T* data; size_t len, cap; }

#define VEC_INIT(V)                             \
  do {                                          \
    V.len = 0;                                  \
    V.cap = 16;                                 \
    void **vp = (void**)&V.data;                \
    *vp = malloc(V.cap * sizeof(*V.data));      \
  } while(0)

#define VEC_FINAL(V) free(V.data)

#define VEC_PUSH(V, X)                                  \
  do {                                                  \
    if (V.len == V.cap) {                               \
      V.cap *= 2;                                       \
      void **vp = (void**)&V.data;                      \
      *vp = realloc(V.data, V.cap * sizeof(*V.data));   \
    }                                                   \
    V.data[V.len++] = X;                                \
  } while(0)                                            \

#define VEC_POP(V)                                              \
  do {                                                          \
    if (V.len > 0)                                              \
      memmove(V.data, V.data+1, (--V.len)*sizeof(*V.data));     \
  } while(0)

/* Simple thread-safe queue implementation */
typedef struct queue_t {
  pthread_mutex_t lock;
  char name[256];
  VEC(pn_rwbytes_t) messages;   /* Messages on the queue_t */
  VEC(pn_connection_t*) waiting; /* Connections waiting to send messages from this queue */
  struct queue_t *next;            /* Next queue in chain */
  size_t sent;                     /* Count of messages sent, used as delivery tag */
} queue_t;

static void queue_init(queue_t *q, const char* name, queue_t *next) {
  pthread_mutex_init(&q->lock, NULL);
  strncpy(q->name, name, sizeof(q->name));
  VEC_INIT(q->messages);
  VEC_INIT(q->waiting);
  q->next = next;
  q->sent = 0;
}

static void queue_destroy(queue_t *q) {
  pthread_mutex_destroy(&q->lock);
  free(q->name);
  for (size_t i = 0; i < q->messages.len; ++i)
    free(q->messages.data[i].start);
  VEC_FINAL(q->messages);
  for (size_t i = 0; i < q->waiting.len; ++i)
    pn_decref(q->waiting.data[i]);
  VEC_FINAL(q->waiting);
}

/* Send a message on s, or record s as eating if no messages.
   Called in s dispatch loop, assumes s has credit.
*/
static void queue_send(queue_t *q, pn_link_t *s) {
  pn_rwbytes_t m = { 0 };
  size_t tag = 0;
  pthread_mutex_lock(&q->lock);
  if (q->messages.len == 0) { /* Empty, record connection as waiting */
    /* Record connection for wake-up if not already on the list. */
    pn_connection_t *c = pn_session_connection(pn_link_session(s));
    size_t i = 0;
    for (; i < q->waiting.len && q->waiting.data[i] != c; ++i)
      ;
    if (i == q->waiting.len) {
      VEC_PUSH(q->waiting, c);
    }
  } else {
    m = q->messages.data[0];
    VEC_POP(q->messages);
    tag = ++q->sent;
  }
  pthread_mutex_unlock(&q->lock);
  if (m.start) {
    pn_delivery_t *d = pn_delivery(s, pn_dtag((char*)&tag, sizeof(tag)));
    pn_link_send(s, m.start, m.size);
    pn_link_advance(s);
    pn_delivery_settle(d);  /* Pre-settled: unreliable, there will bea no ack/ */
    free(m.start);
  }
}

/* Data associated with each broker connection */
typedef struct broker_data_t {
  bool check_queues;          /* Check senders on the connection for available data in queues. */
} broker_data_t;

/* Use the context pointer as a boolean flag to indicate we need to check queues */
void pn_connection_set_check_queues(pn_connection_t *c, bool check) {
  pn_connection_set_context(c, (void*)check);
}

bool pn_connection_get_check_queues(pn_connection_t *c) {
  return (bool)pn_connection_get_context(c);
}

/* Put a message on the queue, called in receiver dispatch loop.
   If the queue was previously empty, notify waiting senders.
*/
static void queue_receive(pn_proactor_t *d, queue_t *q, pn_rwbytes_t m) {
  pthread_mutex_lock(&q->lock);
  VEC_PUSH(q->messages, m);
  if (q->messages.len == 1) { /* Was empty, notify waiting connections */
    for (size_t i = 0; i < q->waiting.len; ++i) {
      pn_connection_t *c = q->waiting.data[i];
      pn_connection_set_check_queues(c, true);
      pn_connection_wake(c); /* Wake the connection */
    }
    q->waiting.len = 0;
  }
  pthread_mutex_unlock(&q->lock);
}

/* Thread safe set of queues */
typedef struct queues_t {
  pthread_mutex_t lock;
  queue_t *queues;
  size_t sent;
} queues_t;

void queues_init(queues_t *qs) {
  pthread_mutex_init(&qs->lock, NULL);
  qs->queues = NULL;
}

void queues_destroy(queues_t *qs) {
  for (queue_t *q = qs->queues; q; q = q->next) {
    queue_destroy(q);
    free(q);
  }
  pthread_mutex_destroy(&qs->lock);
}

/** Get or create the named queue. */
queue_t* queues_get(queues_t *qs, const char* name) {
  pthread_mutex_lock(&qs->lock);
  queue_t *q;
  for (q = qs->queues; q && strcmp(q->name, name) != 0; q = q->next)
    ;
  if (!q) {
    q = (queue_t*)malloc(sizeof(queue_t));
    queue_init(q, name, qs->queues);
    qs->queues = q;
  }
  pthread_mutex_unlock(&qs->lock);
  return q;
}

/* The broker implementation */
typedef struct broker_t {
  pn_proactor_t *proactor;
  size_t threads;
  const char *container_id;     /* AMQP container-id */
  queues_t queues;
  bool finished;
} broker_t;

void broker_stop(broker_t *b) {
  /* Interrupt the proactor to stop the working threads. */
  pn_proactor_interrupt(b->proactor);
}

/* Try to send if link is sender and has credit */
static void link_send(broker_t *b, pn_link_t *s) {
  if (pn_link_is_sender(s) && pn_link_credit(s) > 0) {
    const char *qname = pn_terminus_get_address(pn_link_source(s));
    queue_t *q = queues_get(&b->queues, qname);
    queue_send(q, s);
  }
}

static void queue_unsub(queue_t *q, pn_connection_t *c) {
  pthread_mutex_lock(&q->lock);
  for (size_t i = 0; i < q->waiting.len; ++i) {
    if (q->waiting.data[i] == c){
      q->waiting.data[i] = q->waiting.data[0]; /* save old [0] */
      VEC_POP(q->waiting);
      break;
    }
  }
  pthread_mutex_unlock(&q->lock);
}

/* Unsubscribe from the queue of interest to this link. */
static void link_unsub(broker_t *b, pn_link_t *s) {
  if (pn_link_is_sender(s)) {
    const char *qname = pn_terminus_get_address(pn_link_source(s));
    if (qname) {
      queue_t *q = queues_get(&b->queues, qname);
      queue_unsub(q, pn_session_connection(pn_link_session(s)));
    }
  }
}

/* Called in connection's event loop when a connection is woken for messages.*/
static void connection_unsub(broker_t *b, pn_connection_t *c) {
  for (pn_link_t *l = pn_link_head(c, 0); l != NULL; l = pn_link_next(l, 0))
    link_unsub(b, l);
}

static void session_unsub(broker_t *b, pn_session_t *ssn) {
  pn_connection_t *c = pn_session_connection(ssn);
  for (pn_link_t *l = pn_link_head(c, 0); l != NULL; l = pn_link_next(l, 0)) {
    if (pn_link_session(l) == ssn)
      link_unsub(b, l);
  }
}

static int exit_code = 0;

static void check_condition(pn_event_t *e, pn_condition_t *cond) {
  if (pn_condition_is_set(cond)) {
    fprintf(stderr, "%s: %s: %s\n", pn_event_type_name(pn_event_type(e)),
            pn_condition_get_name(cond), pn_condition_get_description(cond));
    exit_code = 1;              /* Remeber there was an unexpected error */
  }
}

const int WINDOW=10;            /* Incoming credit window */

static void handle(broker_t* b, pn_event_t* e) {
  pn_connection_t *c = pn_event_connection(e);

  switch (pn_event_type(e)) {

   case PN_LISTENER_OPEN:
    printf("listening\n");
    fflush(stdout);
    break;

   case PN_LISTENER_ACCEPT:
    pn_listener_accept(pn_event_listener(e), pn_connection());
    break;

   case PN_CONNECTION_INIT:
     pn_connection_set_container(c, b->container_id);
     break;

   case PN_CONNECTION_BOUND: {
     /* Turn off security */
     pn_transport_t *t = pn_connection_transport(c);
     pn_transport_require_auth(t, false);
     pn_sasl_allowed_mechs(pn_sasl(t), "ANONYMOUS");
     break;
   }
   case PN_CONNECTION_REMOTE_OPEN: {
     pn_connection_open(pn_event_connection(e)); /* Complete the open */
     break;
   }
   case PN_CONNECTION_WAKE: {
     if (pn_connection_get_check_queues(c)) {
       pn_connection_set_check_queues(c, false);
       int flags = PN_LOCAL_ACTIVE&PN_REMOTE_ACTIVE;
       for (pn_link_t *l = pn_link_head(c, flags); l != NULL; l = pn_link_next(l, flags))
         link_send(b, l);
     }
     break;
   }
   case PN_SESSION_REMOTE_OPEN: {
     pn_session_open(pn_event_session(e));
     break;
   }
   case PN_LINK_REMOTE_OPEN: {
     pn_link_t *l = pn_event_link(e);
     if (pn_link_is_sender(l)) {
       const char *source = pn_terminus_get_address(pn_link_remote_source(l));
       pn_terminus_set_address(pn_link_source(l), source);
     } else {
       const char* target = pn_terminus_get_address(pn_link_remote_target(l));
       pn_terminus_set_address(pn_link_target(l), target);
       pn_link_flow(l, WINDOW);
     }
     pn_link_open(l);
     break;
   }
   case PN_LINK_FLOW: {
     link_send(b, pn_event_link(e));
     break;
   }
   case PN_DELIVERY: {
     pn_delivery_t *d = pn_event_delivery(e);
     pn_link_t *r = pn_delivery_link(d);
     if (pn_link_is_receiver(r) &&
         pn_delivery_readable(d) && !pn_delivery_partial(d))
     {
       size_t size = pn_delivery_pending(d);
       /* The broker does not decode the message, just forwards it. */
       pn_rwbytes_t m = { size, (char*)malloc(size) };
       pn_link_recv(r, m.start, m.size);
       const char *qname = pn_terminus_get_address(pn_link_target(r));
       queue_receive(b->proactor, queues_get(&b->queues, qname), m);
       pn_delivery_update(d, PN_ACCEPTED);
       pn_delivery_settle(d);
       pn_link_flow(r, WINDOW - pn_link_credit(r));
     }
     break;
   }

   case PN_TRANSPORT_CLOSED:
    check_condition(e, pn_transport_condition(pn_event_transport(e)));
    connection_unsub(b, pn_event_connection(e));
    break;

   case PN_CONNECTION_REMOTE_CLOSE:
    check_condition(e, pn_connection_remote_condition(pn_event_connection(e)));
    pn_connection_close(pn_event_connection(e));
    break;

   case PN_SESSION_REMOTE_CLOSE:
    check_condition(e, pn_session_remote_condition(pn_event_session(e)));
    session_unsub(b, pn_event_session(e));
    pn_session_close(pn_event_session(e));
    pn_session_free(pn_event_session(e));
    break;

   case PN_LINK_REMOTE_CLOSE:
    check_condition(e, pn_link_remote_condition(pn_event_link(e)));
    link_unsub(b, pn_event_link(e));
    pn_link_close(pn_event_link(e));
    pn_link_free(pn_event_link(e));
    break;

   case PN_LISTENER_CLOSE:
    check_condition(e, pn_listener_condition(pn_event_listener(e)));
    broker_stop(b);
    break;

 break;

   case PN_PROACTOR_INACTIVE:   /* listener and all connections closed */
    broker_stop(b);
    break;

   case PN_PROACTOR_INTERRUPT:
    b->finished = true;
    pn_proactor_interrupt(b->proactor); /* Pass along the interrupt to the other threads */
    break;

   default:
    break;
  }
}

static void* broker_thread(void *void_broker) {
  broker_t *b = (broker_t*)void_broker;
  do {
    pn_event_batch_t *events = pn_proactor_wait(b->proactor);
    pn_event_t *e;
    while ((e = pn_event_batch_next(events))) {
      handle(b, e);
    }
    pn_proactor_done(b->proactor, events);
  } while(!b->finished);
  return NULL;
}

int main(int argc, char **argv) {
  broker_t b = {0};
  b.proactor = pn_proactor();
  queues_init(&b.queues);
  b.container_id = argv[0];
  b.threads = 4;
  int i = 1;
  const char *host = (argc > i) ? argv[i++] : "";
  const char *port = (argc > i) ? argv[i++] : "amqp";

  /* Listen on addr */
  char addr[PN_MAX_ADDR];
  pn_proactor_addr(addr, sizeof(addr), host, port);
  pn_proactor_listen(b.proactor, pn_listener(), addr, 16);

  /* Start n-1 threads */
  pthread_t* threads = (pthread_t*)calloc(sizeof(pthread_t), b.threads);
  for (size_t i = 0; i < b.threads-1; ++i) {
    pthread_create(&threads[i], NULL, broker_thread, &b);
  }
  broker_thread(&b);            /* Use the main thread too. */
  /* Join the other threads */
  for (size_t i = 0; i < b.threads-1; ++i) {
    pthread_join(threads[i], NULL);
  }
  pn_proactor_free(b.proactor);
  free(threads);
  return exit_code;
}
