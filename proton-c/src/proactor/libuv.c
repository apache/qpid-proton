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

#include <uv.h>

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/url.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  libuv loop functions are thread unsafe. The only exception is uv_async_send()
  which is a thread safe "wakeup" that can wake the uv_loop from another thread.

  To provide concurrency the proactor uses a "leader-worker-follower" model,
  threads take turns at the roles:

  - a single "leader" calls libuv functions and runs the uv_loop in short bursts
    to generate work. When there is work available it gives up leadership and
    becomes a "worker"

  - "workers" handle events concurrently for distinct connections/listeners
    They do as much work as they can get, when none is left they become "followers"

  - "followers" wait for the leader to generate work and become workers.
    When the leader itself becomes a worker, one of the followers takes over.

  This model is symmetric: any thread can take on any role based on run-time
  requirements. It also allows the IO and non-IO work associated with an IO
  wake-up to be processed in a single thread with no context switches.

  Function naming:
  - on_ - called in leader thread via uv_run().
  - leader_ - called in leader thread, while processing the leader_q.
  - owner_ - called in owning thread, leader or worker but not concurrently.

  Note on_ and leader_ functions can call each other, the prefix indicates the
  path they are most often called on.
*/

const char *COND_NAME = "proactor";
const char *AMQP_PORT = "5672";
const char *AMQP_PORT_NAME = "amqp";
const char *AMQPS_PORT = "5671";
const char *AMQPS_PORT_NAME = "amqps";

PN_HANDLE(PN_PROACTOR)

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   Class definitions are for identification as pn_event_t context only.
*/
PN_STRUCT_CLASSDEF(pn_proactor, CID_pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener, CID_pn_listener)

/* common to connection and listener */
typedef struct psocket_t {
  /* Immutable */
  pn_proactor_t *proactor;

  /* Protected by proactor.lock */
  struct psocket_t* next;
  void (*wakeup)(struct psocket_t*); /* interrupting action for leader */

  /* Only used by leader */
  uv_tcp_t tcp;
  void (*action)(struct psocket_t*); /* deferred action for leader */
  bool is_conn:1;
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
} psocket_t;

/* Special value for psocket.next pointer when socket is not on any any list. */
psocket_t UNLISTED;

static void psocket_init(psocket_t* ps, pn_proactor_t* p, bool is_conn, const char *host, const char *port) {
  ps->proactor = p;
  ps->next = &UNLISTED;
  ps->is_conn = is_conn;
  ps->tcp.data = ps;

  /* For platforms that don't know about "amqp" and "amqps" service names. */
  if (strcmp(port, AMQP_PORT_NAME) == 0)
    port = AMQP_PORT;
  else if (strcmp(port, AMQPS_PORT_NAME) == 0)
    port = AMQPS_PORT;
  /* Set to "\001" to indicate a NULL as opposed to an empty string "" */
  strncpy(ps->host, host ? host : "\001", sizeof(ps->host));
  strncpy(ps->port, port ? port : "\001", sizeof(ps->port));
}

/* Turn "\001" back to NULL */
static inline const char* fixstr(const char* str) {
  return str[0] == '\001' ? NULL : str;
}

typedef struct pconnection_t {
  psocket_t psocket;

  /* Only used by owner thread */
  pn_connection_driver_t driver;

  /* Only used by leader */
  uv_connect_t connect;
  uv_timer_t timer;
  uv_write_t write;
  uv_shutdown_t shutdown;
  size_t writing;
  bool reading:1;
  bool server:1;                /* accept, not connect */
} pconnection_t;

struct pn_listener_t {
  psocket_t psocket;

  /* Only used by owner thread */
  pconnection_t *accepting;     /* accept in progress */
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *context;
  size_t backlog;
};


typedef struct queue { psocket_t *front, *back; } queue;

struct pn_proactor_t {
  /* Leader thread  */
  uv_cond_t cond;
  uv_loop_t loop;
  uv_async_t async;
  uv_timer_t timer;

  /* Owner thread: proactor collector and batch can belong to leader or a worker */
  pn_collector_t *collector;
  pn_event_batch_t batch;

  /* Protected by lock */
  uv_mutex_t lock;
  queue start_q;
  queue worker_q;
  queue leader_q;
  size_t interrupt;             /* pending interrupts */
  pn_millis_t timeout;
  size_t count;                 /* psocket count */
  bool inactive:1;
  bool timeout_request:1;
  bool timeout_elapsed:1;
  bool has_leader:1;
  bool batch_working:1;          /* batch belongs to a worker.  */
};

static bool push_lh(queue *q, psocket_t *ps) {
  if (ps->next != &UNLISTED)  /* Don't move if already listed. */
    return false;
  ps->next = NULL;
  if (!q->front) {
    q->front = q->back = ps;
  } else {
    q->back->next = ps;
    q->back =  ps;
  }
  return true;
}

static psocket_t* pop_lh(queue *q) {
  psocket_t *ps = q->front;
  if (ps) {
    q->front = ps->next;
    ps->next = &UNLISTED;
  }
  return ps;
}

static inline pconnection_t *as_pconnection_t(psocket_t* ps) {
  return ps->is_conn ? (pconnection_t*)ps : NULL;
}

static inline pn_listener_t *as_listener(psocket_t* ps) {
  return ps->is_conn ? NULL: (pn_listener_t*)ps;
}

/* Put ps on the leader queue for processing. Thread safe. */
static void to_leader_lh(psocket_t *ps) {
  push_lh(&ps->proactor->leader_q, ps);
  uv_async_send(&ps->proactor->async); /* Wake leader */
}

static void to_leader(psocket_t *ps) {
  uv_mutex_lock(&ps->proactor->lock);
  to_leader_lh(ps);
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Detach from IO and put ps on the worker queue */
static void leader_to_worker(psocket_t *ps) {
  if (ps->is_conn) {
    pconnection_t *pc = as_pconnection_t(ps);
    /* Don't detach if there are no events yet. */
    if (pn_connection_driver_has_event(&pc->driver)) {
      if (pc->writing) {
        pc->writing  = 0;
        uv_cancel((uv_req_t*)&pc->write);
      }
      if (pc->reading) {
        pc->reading = false;
        uv_read_stop((uv_stream_t*)&pc->psocket.tcp);
      }
      if (pc->timer.data && !uv_is_closing((uv_handle_t*)&pc->timer)) {
        uv_timer_stop(&pc->timer);
      }
    }
  } else {
    pn_listener_t *l = as_listener(ps);
    uv_read_stop((uv_stream_t*)&l->psocket.tcp);
  }
  uv_mutex_lock(&ps->proactor->lock);
  push_lh(&ps->proactor->worker_q, ps);
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Set a deferred action for leader, if not already set. */
static void owner_to_leader(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  if (!ps->action) {
    ps->action = action;
  }
  to_leader_lh(ps);
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Owner thread send to worker thread. Set deferred action if not already set. */
static void owner_to_worker(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  if (!ps->action) {
    ps->action = action;
  }
  push_lh(&ps->proactor->worker_q, ps);
  uv_async_send(&ps->proactor->async); /* Wake leader */
  uv_mutex_unlock(&ps->proactor->lock);
}


/* Re-queue for further work */
static void worker_requeue(psocket_t* ps) {
  uv_mutex_lock(&ps->proactor->lock);
  push_lh(&ps->proactor->worker_q, ps);
  uv_async_send(&ps->proactor->async); /* Wake leader */
  uv_mutex_unlock(&ps->proactor->lock);
}

static pconnection_t *new_pconnection_t(pn_proactor_t *p, pn_connection_t *c, bool server, const char *host, const char *port) {
  pconnection_t *pc = (pconnection_t*)calloc(1, sizeof(*pc));
  if (!pc) return NULL;
  if (pn_connection_driver_init(&pc->driver, c, NULL) != 0) {
    return NULL;
  }
  psocket_init(&pc->psocket, p,  true, host, port);
  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }
  pn_record_t *r = pn_connection_attachments(pc->driver.connection);
  pn_record_def(r, PN_PROACTOR, PN_VOID);
  pn_record_set(r, PN_PROACTOR, pc);
  return pc;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch);
static pn_event_t *proactor_batch_next(pn_event_batch_t *batch);

static inline pn_proactor_t *batch_proactor(pn_event_batch_t *batch) {
  return (batch->next_event == proactor_batch_next) ?
    (pn_proactor_t*)((char*)batch - offsetof(pn_proactor_t, batch)) : NULL;
}

static inline pn_listener_t *batch_listener(pn_event_batch_t *batch) {
  return (batch->next_event == listener_batch_next) ?
    (pn_listener_t*)((char*)batch - offsetof(pn_listener_t, batch)) : NULL;
}

static inline pconnection_t *batch_pconnection(pn_event_batch_t *batch) {
  pn_connection_driver_t *d = pn_event_batch_connection_driver(batch);
  return d ? (pconnection_t*)((char*)d - offsetof(pconnection_t, driver)) : NULL;
}

static void leader_count(pn_proactor_t *p, int change) {
  uv_mutex_lock(&p->lock);
  p->count += change;
  p->inactive = (p->count == 0);
  uv_mutex_unlock(&p->lock);
}

/* Free if there are no uv callbacks pending and no events */
static void leader_pconnection_t_maybe_free(pconnection_t *pc) {
    if (pn_connection_driver_has_event(&pc->driver)) {
      leader_to_worker(&pc->psocket);         /* Return to worker */
    } else if (!(pc->psocket.tcp.data || pc->write.data || pc->shutdown.data || pc->timer.data)) {
      /* All UV requests are finished */
      pn_connection_driver_destroy(&pc->driver);
      leader_count(pc->psocket.proactor, -1);
      free(pc);
    }
}

/* Free if there are no uv callbacks pending and no events */
static void leader_listener_maybe_free(pn_listener_t *l) {
    if (pn_collector_peek(l->collector)) {
      leader_to_worker(&l->psocket);         /* Return to worker */
    } else if (!l->psocket.tcp.data) {
      pn_condition_free(l->condition);
      leader_count(l->psocket.proactor, -1);
      free(l);
    }
}

/* Free if there are no uv callbacks pending and no events */
static void leader_maybe_free(psocket_t *ps) {
  if (ps->is_conn) {
    leader_pconnection_t_maybe_free(as_pconnection_t(ps));
  } else {
    leader_listener_maybe_free(as_listener(ps));
  }
}

static void on_close(uv_handle_t *h) {
  psocket_t *ps = (psocket_t*)h->data;
  h->data = NULL;               /* Mark closed */
  leader_maybe_free(ps);
}

static void on_shutdown(uv_shutdown_t *shutdown, int err) {
  psocket_t *ps = (psocket_t*)shutdown->data;
  shutdown->data = NULL;        /* Mark closed */
  leader_maybe_free(ps);
}

static inline void leader_close(psocket_t *ps) {
  if (ps->tcp.data && !uv_is_closing((uv_handle_t*)&ps->tcp)) {
    uv_close((uv_handle_t*)&ps->tcp, on_close);
  }
  pconnection_t *pc = as_pconnection_t(ps);
  if (pc) {
    pn_connection_driver_close(&pc->driver);
    if (pc->timer.data && !uv_is_closing((uv_handle_t*)&pc->timer)) {
      uv_timer_stop(&pc->timer);
      uv_close((uv_handle_t*)&pc->timer, on_close);
    }
  }
  leader_maybe_free(ps);
}

static pconnection_t *get_pconnection_t(pn_connection_t* c) {
  if (!c) return NULL;
  pn_record_t *r = pn_connection_attachments(c);
  return (pconnection_t*) pn_record_get(r, PN_PROACTOR);
}

static void leader_error(psocket_t *ps, int err, const char* what) {
  if (ps->is_conn) {
    pn_connection_driver_t *driver = &as_pconnection_t(ps)->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pn_connection_driver_errorf(driver, COND_NAME, "%s %s:%s: %s",
                                what, fixstr(ps->host), fixstr(ps->port),
                                uv_strerror(err));
    pn_connection_driver_close(driver);
  } else {
    pn_listener_t *l = as_listener(ps);
    pn_condition_format(l->condition, COND_NAME, "%s %s:%s: %s",
                        what, fixstr(ps->host), fixstr(ps->port),
                        uv_strerror(err));
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_CLOSE);
  }
  leader_to_worker(ps);               /* Worker to handle the error */
}

/* uv-initialization */
static int leader_init(psocket_t *ps) {
  leader_count(ps->proactor, +1);
  int err = uv_tcp_init(&ps->proactor->loop, &ps->tcp);
  if (!err) {
    pconnection_t *pc = as_pconnection_t(ps);
    if (pc) {
      pc->connect.data = ps;
      int err = uv_timer_init(&ps->proactor->loop, &pc->timer);
      if (!err) {
        pc->timer.data = pc;
      }
    }
  }
  if (err) {
    leader_error(ps, err, "initialization");
  }
  return err;
}

/* Common logic for on_connect and on_accept */
static void leader_connect_accept(pconnection_t *pc, int err, const char *what) {
  if (!err) {
    leader_to_worker(&pc->psocket);
  } else {
    leader_error(&pc->psocket, err, what);
  }
}

static void on_connect(uv_connect_t *connect, int err) {
  leader_connect_accept((pconnection_t*)connect->data, err, "on connect");
}

static void on_accept(uv_stream_t* server, int err) {
  pn_listener_t *l = (pn_listener_t*) server->data;
  if (err) {
    leader_error(&l->psocket, err, "on accept");
  }
  pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
  leader_to_worker(&l->psocket); /* Let user call pn_listener_accept */
}

static void leader_accept(psocket_t *ps) {
  pn_listener_t * l = as_listener(ps);
  pconnection_t *pc = l->accepting;
  l->accepting = NULL;
  if (pc) {
    int err = leader_init(&pc->psocket);
    if (!err) err = uv_accept((uv_stream_t*)&l->psocket.tcp, (uv_stream_t*)&pc->psocket.tcp);
    leader_connect_accept(pc, err, "on accept");
  }
}

static int leader_resolve(psocket_t *ps, uv_getaddrinfo_t *info, bool server) {
  int err = leader_init(ps);
  struct addrinfo hints = { 0 };
  if (server) hints.ai_flags = AI_PASSIVE;
  if (!err) {
    err = uv_getaddrinfo(&ps->proactor->loop, info, NULL, fixstr(ps->host), fixstr(ps->port), &hints);
  }
  return err;
}

static void leader_connect(psocket_t *ps) {
  pconnection_t *pc = as_pconnection_t(ps);
  uv_getaddrinfo_t info;
  int err = leader_resolve(ps, &info, false);
  if (!err) {
    err = uv_tcp_connect(&pc->connect, &pc->psocket.tcp, info.addrinfo->ai_addr, on_connect);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (err) {
    leader_error(ps, err, "connect to");
  }
}

static void leader_listen(psocket_t *ps) {
  pn_listener_t *l = as_listener(ps);
   uv_getaddrinfo_t info;
  int err = leader_resolve(ps, &info, true);
  if (!err) {
    err = uv_tcp_bind(&l->psocket.tcp, info.addrinfo->ai_addr, 0);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (!err) err = uv_listen((uv_stream_t*)&l->psocket.tcp, l->backlog, on_accept);
  if (err) {
    leader_error(ps, err, "listen on ");
  }
}

static void on_tick(uv_timer_t *timer) {
  pconnection_t *pc = (pconnection_t*)timer->data;
  pn_transport_t *t = pc->driver.transport;
  if (pn_transport_get_idle_timeout(t) || pn_transport_get_remote_idle_timeout(t)) {
    uv_timer_stop(&pc->timer);
    uint64_t now = uv_now(pc->timer.loop);
    uint64_t next = pn_transport_tick(t, now);
    if (next) {
      uv_timer_start(&pc->timer, on_tick, next - now, 0);
    }
  }
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  if (nread >= 0) {
    pn_connection_driver_read_done(&pc->driver, nread);
    on_tick(&pc->timer);         /* check for tick changes. */
    leader_to_worker(&pc->psocket);
    /* Reading continues automatically until stopped. */
  } else if (nread == UV_EOF) { /* hangup */
    pn_connection_driver_read_close(&pc->driver);
    leader_maybe_free(&pc->psocket);
  } else {
    leader_error(&pc->psocket, nread, "on read from");
  }
}

static void on_write(uv_write_t* write, int err) {
  pconnection_t *pc = (pconnection_t*)write->data;
  write->data = NULL;
  if (err == 0) {
    pn_connection_driver_write_done(&pc->driver, pc->writing);
    leader_to_worker(&pc->psocket);
  } else if (err == UV_ECANCELED) {
    leader_maybe_free(&pc->psocket);
  } else {
    leader_error(&pc->psocket, err, "on write to");
  }
  pc->writing = 0;              /* Need to send a new write request */
}

static void on_timeout(uv_timer_t *timer) {
  pn_proactor_t *p = (pn_proactor_t*)timer->data;
  uv_mutex_lock(&p->lock);
  p->timeout_elapsed = true;
  uv_mutex_unlock(&p->lock);
}

// Read buffer allocation function for uv, just returns the transports read buffer.
static void alloc_read_buffer(uv_handle_t* stream, size_t size, uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
  *buf = uv_buf_init(rbuf.start, rbuf.size);
}

static void leader_rewatch(psocket_t *ps) {
  int err = 0;
  if (ps->is_conn) {
    pconnection_t *pc = as_pconnection_t(ps);
    if (pc->timer.data) {         /* uv-initialized */
      on_tick(&pc->timer);        /* Re-enable ticks if required */
    }
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);

    /* Ticks and checking buffers can generate events, process before proceeding */
    if (pn_connection_driver_has_event(&pc->driver)) {
      leader_to_worker(ps);
    } else {                      /* Re-watch for IO */
      if (wbuf.size > 0 && !pc->writing) {
        pc->writing = wbuf.size;
        uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size);
        pc->write.data = ps;
        uv_write(&pc->write, (uv_stream_t*)&pc->psocket.tcp, &buf, 1, on_write);
      } else if (wbuf.size == 0 && pn_connection_driver_write_closed(&pc->driver)) {
        pc->shutdown.data = ps;
        uv_shutdown(&pc->shutdown, (uv_stream_t*)&pc->psocket.tcp, on_shutdown);
      }
      if (rbuf.size > 0 && !pc->reading) {
        pc->reading = true;
        err = uv_read_start((uv_stream_t*)&pc->psocket.tcp, alloc_read_buffer, on_read);
      }
    }
  } else {
    pn_listener_t *l = as_listener(ps);
    err = uv_listen((uv_stream_t*)&l->psocket.tcp, l->backlog, on_accept);
  }
  if (err) {
    leader_error(ps, err, "rewatch");
  }
}

/* Set the event in the proactor's batch  */
static pn_event_batch_t *proactor_batch_lh(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, pn_proactor__class(), p, t);
  p->batch_working = true;
  return &p->batch;
}

/* Return the next event batch or 0 if no events are ready */
static pn_event_batch_t* get_batch_lh(pn_proactor_t *p) {
  if (!p->batch_working) {       /* Can generate proactor events */
    if (p->inactive) {
      p->inactive = false;
      return proactor_batch_lh(p, PN_PROACTOR_INACTIVE);
    }
    if (p->interrupt > 0) {
      --p->interrupt;
      return proactor_batch_lh(p, PN_PROACTOR_INTERRUPT);
    }
    if (p->timeout_elapsed) {
      p->timeout_elapsed = false;
      return proactor_batch_lh(p, PN_PROACTOR_TIMEOUT);
    }
  }
  for (psocket_t *ps = pop_lh(&p->worker_q); ps; ps = pop_lh(&p->worker_q)) {
    if (ps->is_conn) {
      pconnection_t *pc = as_pconnection_t(ps);
      return &pc->driver.batch;
    } else {                    /* Listener */
      pn_listener_t *l = as_listener(ps);
      return &l->batch;
    }
    to_leader(ps);      /* No event, back to leader */
  }
  return 0;
}

/* Called in any thread to set a wakeup action. Replaces any previous wakeup action. */
static void wakeup(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  ps->wakeup = action;
  to_leader_lh(ps);
  uv_mutex_unlock(&ps->proactor->lock);
}

pn_listener_t *pn_event_listener(pn_event_t *e) {
  return (pn_event_class(e) == pn_listener__class()) ? (pn_listener_t*)pn_event_context(e) : NULL;
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == pn_proactor__class()) return (pn_proactor_t*)pn_event_context(e);
  pn_listener_t *l = pn_event_listener(e);
  if (l) return l->psocket.proactor;
  pn_connection_t *c = pn_event_connection(e);
  if (c) return pn_connection_proactor(pn_event_connection(e));
  return NULL;
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) {
    if (pn_connection_driver_has_event(&pc->driver)) {
      /* Process all events before going back to IO. */
      worker_requeue(&pc->psocket);
    } else if (pn_connection_driver_finished(&pc->driver)) {
      owner_to_leader(&pc->psocket, leader_close);
    } else {
      owner_to_leader(&pc->psocket, leader_rewatch);
    }
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    owner_to_leader(&l->psocket, leader_rewatch);
    return;
  }
  pn_proactor_t *bp = batch_proactor(batch);
  if (bp == p) {
    uv_mutex_lock(&p->lock);
    p->batch_working = false;
    uv_async_send(&p->async); /* Wake leader */
    uv_mutex_unlock(&p->lock);
    return;
  }
}

/* Run follower/leader loop till we can return an event and be a worker */
pn_event_batch_t *pn_proactor_wait(struct pn_proactor_t* p) {
  uv_mutex_lock(&p->lock);
  /* Try to grab work immediately. */
  pn_event_batch_t *batch = get_batch_lh(p);
  if (batch == NULL) {
    /* No work available, follow the leader */
    while (p->has_leader) {
      uv_cond_wait(&p->cond, &p->lock);
    }
    /* Lead till there is work to do. */
    p->has_leader = true;
    while (batch == NULL) {
      if (p->timeout_request) {
        p->timeout_request = false;
        if (p->timeout) {
          uv_timer_start(&p->timer, on_timeout, p->timeout, 0);
        } else {
          uv_timer_stop(&p->timer);
        }
      }
      for (psocket_t *ps = pop_lh(&p->leader_q); ps; ps = pop_lh(&p->leader_q)) {
        void (*action)(psocket_t*) = ps->action;
        void (*wakeup)(psocket_t*) = ps->wakeup;
        ps->action = NULL;
        ps->wakeup = NULL;
        if (action || wakeup) {
          uv_mutex_unlock(&p->lock);
          if (action) action(ps);
          if (wakeup) wakeup(ps);
          uv_mutex_lock(&p->lock);
        }
      }
      batch = get_batch_lh(p);
      if (batch == NULL) {
        uv_mutex_unlock(&p->lock);
        uv_run(&p->loop, UV_RUN_ONCE);
        uv_mutex_lock(&p->lock);
      }
    }
    /* Signal the next leader and return to work */
    p->has_leader = false;
    uv_cond_signal(&p->cond);
  }
  uv_mutex_unlock(&p->lock);
  return batch;
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  ++p->interrupt;
  uv_async_send(&p->async);   /* Interrupt the UV loop */
  uv_mutex_unlock(&p->lock);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  uv_mutex_lock(&p->lock);
  p->timeout = t;
  p->timeout_request = true;
  uv_async_send(&p->async);   /* Interrupt the UV loop */
  uv_mutex_unlock(&p->lock);
}

int pn_proactor_connect(pn_proactor_t *p, pn_connection_t *c, const char *host, const char *port) {
  pconnection_t *pc = new_pconnection_t(p, c, false, host, port);
  if (!pc) {
    return PN_OUT_OF_MEMORY;
  }
  /* Process PN_CONNECTION_INIT before binding */
  owner_to_worker(&pc->psocket, leader_connect);
  return 0;
}

int pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *host, const char *port, int backlog)
{
  psocket_init(&l->psocket, p, false, host, port);
  l->backlog = backlog;
  owner_to_leader(&l->psocket, leader_listen);
  return 0;
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection_t(c);
  return pc ? pc->psocket.proactor : NULL;
}

void leader_wake_connection(psocket_t *ps) {
  pconnection_t *pc = as_pconnection_t(ps);
  pn_connection_t *c = pc->driver.connection;
  pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
  leader_to_worker(ps);
}

void pn_connection_wake(pn_connection_t* c) {
  wakeup(&get_pconnection_t(c)->psocket, leader_wake_connection);
}

pn_proactor_t *pn_proactor() {
  pn_proactor_t *p = (pn_proactor_t*)calloc(1, sizeof(*p));
  p->collector = pn_collector();
  p->batch.next_event = &proactor_batch_next;
  if (!p->collector) return NULL;
  uv_loop_init(&p->loop);
  uv_mutex_init(&p->lock);
  uv_cond_init(&p->cond);
  uv_async_init(&p->loop, &p->async, NULL);
  uv_timer_init(&p->loop, &p->timer); /* Just wake the loop */
  p->timer.data = p;
  return p;
}

static void on_stopping(uv_handle_t* h, void* v) {
  uv_close(h, NULL);           /* Close this handle */
  if (!uv_loop_alive(h->loop)) /* Everything closed */
    uv_stop(h->loop);        /* Stop the loop, pn_proactor_destroy() can return */
}

void pn_proactor_free(pn_proactor_t *p) {
  uv_walk(&p->loop, on_stopping, NULL); /* Close all handles */
  uv_run(&p->loop, UV_RUN_DEFAULT);     /* Run till stop, all handles closed */
  uv_loop_close(&p->loop);
  uv_mutex_destroy(&p->lock);
  uv_cond_destroy(&p->cond);
  pn_collector_free(p->collector);
  free(p);
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  pn_event_t *handled = pn_collector_prev(l->collector);
  if (handled && pn_event_type(handled) == PN_LISTENER_CLOSE) {
    owner_to_leader(&l->psocket, leader_close); /* Close event handled, do close */
  }
  return pn_collector_next(l->collector);
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  return pn_collector_next(batch_proactor(batch)->collector);
}

static void pn_listener_free(pn_listener_t *l) {
  if (l) {
    if (!l->collector) pn_collector_free(l->collector);
    if (!l->condition) pn_condition_free(l->condition);
    if (!l->attachments) pn_free(l->attachments);
    free(l);
  }
}

pn_listener_t *pn_listener() {
  pn_listener_t *l = (pn_listener_t*)calloc(1, sizeof(pn_listener_t));
  if (l) {
    l->batch.next_event = listener_batch_next;
    l->collector = pn_collector();
    l->condition = pn_condition();
    l->attachments = pn_record();
    if (!l->condition || !l->collector || !l->attachments) {
      pn_listener_free(l);
      return NULL;
    }
  }
  return l;
}

void pn_listener_close(pn_listener_t* l) {
  wakeup(&l->psocket, leader_close);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->psocket.proactor : NULL;
}

pn_condition_t* pn_listener_condition(pn_listener_t* l) {
  return l->condition;
}

void *pn_listener_get_context(pn_listener_t *l) {
  return l->context;
}

void pn_listener_set_context(pn_listener_t *l, void *context) {
  l->context = context;
}

pn_record_t *pn_listener_attachments(pn_listener_t *l) {
  return l->attachments;
}

int pn_listener_accept(pn_listener_t *l, pn_connection_t *c) {
  if (l->accepting) {
    return PN_STATE_ERR;        /* Only one at a time */
  }
  l->accepting = new_pconnection_t(
      l->psocket.proactor, c, true, l->psocket.host, l->psocket.port);
  if (!l->accepting) {
    return UV_ENOMEM;
  }
  owner_to_leader(&l->psocket, leader_accept);
  return 0;
}

