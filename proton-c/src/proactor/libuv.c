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
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/url.h>

#include <uv.h>

/* All asserts are cheap and should remain in a release build for debugability */
#undef NDEBUG
#include <assert.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  libuv functions are thread unsafe. The exception is uv_async_send(), a thread safe
  "wakeup" that can wake the uv_loop from another thread.

  To provide concurrency proactor uses a "leader-worker-follower" model, threads take
  turns at the roles:

  - a single "leader" thread uses libuv, it runs the uv_loop the in short bursts to
  generate work. Once there is work it becomes becomes a "worker" thread, another thread
  takes over as leader.

  - "workers" handle events for separate connections or listeners concurrently. They do as
  much work as they can, when none is left they become "followers"

  - "followers" wait for the leader to generate work. One follower becomes the new leader,
  the others become workers or continue to follow till they can get work.

  Any thread in a pool can take on any role necessary at run-time. All the work generated
  by an IO wake-up for a single connection can be processed in a single single worker
  thread to minimize context switching.

  Function naming:
  - on_* - called in leader thread via  uv_run().
  - leader_* - called in leader thread (either leader_q processing or from an on_ function)
  - *_lh - called with the relevant lock held

  LIFECYCLE: pconnection_t and pn_listener_t objects must not be deleted until all their
  UV handles have received a close callback. Freeing resources is initiated by uv_close()
  of the uv_tcp_t handle, and executed in an on_close() handler when it is safe.
*/

const char *COND_NAME = "proactor";
const char *AMQP_PORT = "5672";
const char *AMQP_PORT_NAME = "amqp";
const char *AMQPS_PORT = "5671";
const char *AMQPS_PORT_NAME = "amqps";

PN_HANDLE(PN_PROACTOR)

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   CLASSDEF is for identification when used as a pn_event_t context.
*/
PN_STRUCT_CLASSDEF(pn_proactor, CID_pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener, CID_pn_listener)

/* A psocket (connection or listener) has the following mutually exclusive states. */
typedef enum {
  ON_WORKER,               /* On worker_q or in use by user code in worker thread  */
  ON_LEADER,               /* On leader_q or in use the leader loop  */
  ON_UV                    /* Scheduled for a UV event, or in use by leader thread in on_ handler*/
} psocket_state_t;

/* common to connection and listener */
typedef struct psocket_t {
  /* Immutable */
  pn_proactor_t *proactor;

  /* Protected by proactor.lock */
  struct psocket_t* next;
  psocket_state_t state;
  void (*action)(struct psocket_t*); /* deferred action for leader */
  void (*wakeup)(struct psocket_t*); /* wakeup action for leader */

  /* Only used by leader thread when it owns the psocket */
  uv_tcp_t tcp;
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  bool is_conn;
} psocket_t;

/* Special value for psocket.next pointer when socket is not on any any list. */
psocket_t UNLISTED;

static void psocket_init(psocket_t* ps, pn_proactor_t* p, bool is_conn, const char *host, const char *port) {
  ps->proactor = p;
  ps->next = &UNLISTED;
  ps->is_conn = is_conn;
  ps->tcp.data = ps;
  ps->state = ON_WORKER;

  /* For platforms that don't know about "amqp" and "amqps" service names. */
  if (port && strcmp(port, AMQP_PORT_NAME) == 0)
    port = AMQP_PORT;
  else if (port && strcmp(port, AMQPS_PORT_NAME) == 0)
    port = AMQPS_PORT;
  /* Set to "\001" to indicate a NULL as opposed to an empty string "" */
  strncpy(ps->host, host ? host : "\001", sizeof(ps->host));
  strncpy(ps->port, port ? port : "\001", sizeof(ps->port));
}

/* Turn "\001" back to NULL */
static inline const char* fixstr(const char* str) {
  return str[0] == '\001' ? NULL : str;
}

/* Holds a psocket and a pn_connection_driver  */
typedef struct pconnection_t {
  psocket_t psocket;

  /* Only used by owner thread */
  pn_connection_driver_t driver;

  /* Only used by leader */
  uv_connect_t connect;
  uv_timer_t timer;
  uv_write_t write;
  uv_shutdown_t shutdown;
  size_t writing;               /* size of pending write request, 0 if none pending */
  bool server;                  /* accepting not connecting */
} pconnection_t;


/* pn_listener_t with a psocket_t  */
struct pn_listener_t {
  psocket_t psocket;

  /* Only used by owner thread */
  pconnection_t *accepting;     /* set in worker, used in UV loop for accept */
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *context;
  size_t backlog;
  bool closing;                 /* close requested or closed by error */

  /* Only used in leader thread */
  size_t connections;           /* number of connections waiting to be accepted  */
  int err;                      /* uv error code, 0 = OK, UV_EOF = closed */
  const char *what;             /* static description string */
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
  queue worker_q;               /* psockets ready for work, to be returned via pn_proactor_wait()  */
  queue leader_q;               /* psockets waiting for attention by the leader thread */
  size_t interrupt;             /* pending interrupts */
  pn_millis_t timeout;
  size_t count;                 /* psocket count */
  bool inactive;
  bool timeout_request;
  bool timeout_elapsed;
  bool has_leader;
  bool batch_working;          /* batch is being processed in a worker thread */
};

/* Push ps to back of q. Must not be on a different queue */
static void push_lh(queue *q, psocket_t *ps) {
  assert(ps->next == &UNLISTED);
  ps->next = NULL;
  if (!q->front) {
    q->front = q->back = ps;
  } else {
    q->back->next = ps;
    q->back =  ps;
  }
}

/* Pop returns front of q or NULL if empty */
static psocket_t* pop_lh(queue *q) {
  psocket_t *ps = q->front;
  if (ps) {
    q->front = ps->next;
    ps->next = &UNLISTED;
  }
  return ps;
}

/* Queue an action for the leader thread */
static void to_leader(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  ps->action = action;
  if (ps->next == &UNLISTED) {
    ps->state = ON_LEADER;
    push_lh(&ps->proactor->leader_q, ps);
  }
  uv_mutex_unlock(&ps->proactor->lock);
  uv_async_send(&ps->proactor->async); /* Wake leader */
}

/* Push to the worker thread */
static void to_worker(psocket_t *ps) {
  uv_mutex_lock(&ps->proactor->lock);
  /* If already ON_WORKER do nothing */
  if (ps->next == &UNLISTED && ps->state != ON_WORKER) {
    ps->state = ON_WORKER;
    push_lh(&ps->proactor->worker_q, ps);
  }
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Set state to ON_UV */
static void to_uv(psocket_t *ps) {
  uv_mutex_lock(&ps->proactor->lock);
  if (ps->next == &UNLISTED) {
    ps->state = ON_UV;
  }
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Called in any thread to set a wakeup action */
static void wakeup(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  ps->wakeup = action;
  /* If ON_WORKER we'll do the wakeup in pn_proactor_done() */
  if (ps->next == &UNLISTED && ps->state != ON_WORKER) {
    push_lh(&ps->proactor->leader_q, ps);
    ps->state = ON_LEADER;      /* Otherwise notify the leader */
  }
  uv_mutex_unlock(&ps->proactor->lock);
  uv_async_send(&ps->proactor->async); /* Wake leader */
}

static inline pconnection_t *as_pconnection(psocket_t* ps) {
  return ps->is_conn ? (pconnection_t*)ps : NULL;
}

static inline pn_listener_t *as_listener(psocket_t* ps) {
  return ps->is_conn ? NULL: (pn_listener_t*)ps;
}

static pconnection_t *pconnection(pn_proactor_t *p, pn_connection_t *c, bool server, const char *host, const char *port) {
  pconnection_t *pc = (pconnection_t*)calloc(1, sizeof(*pc));
  if (!pc || pn_connection_driver_init(&pc->driver, c, NULL) != 0) {
    return NULL;
  }
  psocket_init(&pc->psocket, p,  true, host, port);
  pc->write.data = &pc->psocket;
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

static void pconnection_free(pconnection_t *pc) {
  pn_connection_driver_destroy(&pc->driver);
  free(pc);
}

static void pn_listener_free(pn_listener_t *l);

/* Final close event for for a pconnection_t, closes the timer */
static void on_close_pconnection_final(uv_handle_t *h) {
  pconnection_free((pconnection_t*)h->data);
}

/* Close event for uv_tcp_t of a psocket_t */
static void on_close_psocket(uv_handle_t *h) {
  psocket_t *ps = (psocket_t*)h->data;
  if (ps->is_conn) {
    leader_count(ps->proactor, -1);
    pconnection_t *pc = as_pconnection(ps);
    uv_timer_stop(&pc->timer);
    /* Delay the free till the timer handle is also closed */
    uv_close((uv_handle_t*)&pc->timer, on_close_pconnection_final);
  } else {
    pn_listener_free(as_listener(ps));
  }
}

static pconnection_t *get_pconnection(pn_connection_t* c) {
  if (!c) {
    return NULL;
  }
  pn_record_t *r = pn_connection_attachments(c);
  return (pconnection_t*) pn_record_get(r, PN_PROACTOR);
}

static void pconnection_to_worker(pconnection_t *pc);
static void listener_to_worker(pn_listener_t *l);

int pconnection_error(pconnection_t *pc, int err, const char* what) {
  if (err) {
    pn_connection_driver_t *driver = &pc->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pn_connection_driver_errorf(driver, COND_NAME, "%s %s:%s: %s",
                                what, fixstr(pc->psocket.host), fixstr(pc->psocket.port),
                                uv_strerror(err));
    pn_connection_driver_close(driver);
    pconnection_to_worker(pc);
  }
  return err;
}

static int listener_error(pn_listener_t *l, int err, const char* what) {
  if (err) {
    l->err = err;
    l->what = what;
    listener_to_worker(l);
  }
  return err;
}

static int psocket_error(psocket_t *ps, int err, const char* what) {
  if (err) {
    if (ps->is_conn) {
      pconnection_error(as_pconnection(ps), err, "initialization");
    } else {
      listener_error(as_listener(ps), err, "initialization");
    }
  }
  return err;
}

/* psocket uv-initialization */
static int leader_init(psocket_t *ps) {
  ps->state = ON_LEADER;
  leader_count(ps->proactor, +1);
  int err = uv_tcp_init(&ps->proactor->loop, &ps->tcp);
  if (!err) {
    pconnection_t *pc = as_pconnection(ps);
    if (pc) {
      pc->connect.data = ps;
      int err = uv_timer_init(&ps->proactor->loop, &pc->timer);
      if (!err) {
        pc->timer.data = ps;
      }
    }
  } else {
    psocket_error(ps, err, "initialization");
  }
  return err;
}

/* Outgoing connection */
static void on_connect(uv_connect_t *connect, int err) {
  pconnection_t *pc = (pconnection_t*)connect->data;
  if (!err) {
    pconnection_to_worker(pc);
  } else {
    pconnection_error(pc, err, "on connect to");
  }
}

/* Incoming connection ready to be accepted */
static void on_connection(uv_stream_t* server, int err) {
  /* Unlike most on_* functions, this one can be called by the leader thread when the
   * listener is ON_WORKER, because there's no way to stop libuv from calling
   * on_connection().  Just increase a counter and generate events in to_worker.
   */
  pn_listener_t *l = (pn_listener_t*) server->data;
  l->err = err;
  if (!err) ++l->connections;
  listener_to_worker(l);        /* If already ON_WORKER it will stay there */
}

static void leader_accept(pn_listener_t * l) {
  assert(l->accepting);
  pconnection_t *pc = l->accepting;
  l->accepting = NULL;
  int err = leader_init(&pc->psocket);
  if (!err) {
    err = uv_accept((uv_stream_t*)&l->psocket.tcp, (uv_stream_t*)&pc->psocket.tcp);
  }
  if (!err) {
    pconnection_to_worker(pc);
  } else {
    pconnection_error(pc, err, "accepting from");
    listener_error(l, err, "accepting from");
  }
}

static int leader_resolve(psocket_t *ps, uv_getaddrinfo_t *info, bool server) {
  assert(ps->state == ON_LEADER);
  int err = leader_init(ps);
  struct addrinfo hints = { 0 };
  if (server) hints.ai_flags = AI_PASSIVE;
  if (!err) {
    err = uv_getaddrinfo(&ps->proactor->loop, info, NULL, fixstr(ps->host), fixstr(ps->port), &hints);
  }
  return err;
}

static void leader_connect(psocket_t *ps) {
  assert(ps->state == ON_LEADER);
  pconnection_t *pc = as_pconnection(ps);
  uv_getaddrinfo_t info;
  int err = leader_resolve(ps, &info, false);
  if (!err) {
    err = uv_tcp_connect(&pc->connect, &pc->psocket.tcp, info.addrinfo->ai_addr, on_connect);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (!err) {
    ps->state = ON_UV;
  } else {
    psocket_error(ps, err, "connecting to");
  }
}

static void leader_listen(psocket_t *ps) {
  assert(ps->state == ON_LEADER);
  pn_listener_t *l = as_listener(ps);
  uv_getaddrinfo_t info;
  int err = leader_resolve(ps, &info, true);
  if (!err) {
    err = uv_tcp_bind(&l->psocket.tcp, info.addrinfo->ai_addr, 0);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (!err) {
    err = uv_listen((uv_stream_t*)&l->psocket.tcp, l->backlog, on_connection);
  }
  if (!err) {
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_OPEN);
    listener_to_worker(l);      /* Let worker see the OPEN event */
  } else {
    listener_error(l, err, "listening on");
  }
}

/* Generate tick events and return millis till next tick or 0 if no tick is required */
static pn_millis_t leader_tick(pconnection_t *pc) {
  assert(pc->psocket.state != ON_WORKER);
  uint64_t now = uv_now(pc->timer.loop);
  uint64_t next = pn_transport_tick(pc->driver.transport, now);
  return next ? next - now : 0;
}

static void on_tick(uv_timer_t *timer) {
  pconnection_t *pc = (pconnection_t*)timer->data;
  pn_millis_t next = leader_tick(pc); /* May generate events */
  if (pn_connection_driver_has_event(&pc->driver)) {
    pconnection_to_worker(pc);
  } else if (next) {
    uv_timer_start(&pc->timer, on_tick, next, 0);
  }
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  if (nread >= 0) {
    pn_connection_driver_read_done(&pc->driver, nread);
    pconnection_to_worker(pc);
  } else if (nread == UV_EOF) { /* hangup */
    pn_connection_driver_read_close(&pc->driver);
    pconnection_to_worker(pc);
  } else {
    pconnection_error(pc, nread, "on read from");
  }
}

static void on_write(uv_write_t* write, int err) {
  pconnection_t *pc = (pconnection_t*)write->data;
  if (err == 0) {
    pn_connection_driver_write_done(&pc->driver, pc->writing);
    pconnection_to_worker(pc);
  } else if (err == UV_ECANCELED) {
    pconnection_to_worker(pc);
  } else {
    pconnection_error(pc, err, "on write to");
  }
  pc->writing = 0;
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

static void pconnection_to_uv(pconnection_t *pc) {
  to_uv(&pc->psocket);          /* Assume we're going to UV unless sent elsewhere */
  if (pn_connection_driver_finished(&pc->driver)) {
    if (!uv_is_closing((uv_handle_t*)&pc->psocket)) {
      uv_close((uv_handle_t*)&pc->psocket.tcp, on_close_psocket);
    }
    return;
  }
  pn_millis_t next_tick = leader_tick(pc);
  pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
  pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
  if (pn_connection_driver_has_event(&pc->driver)) {
    to_worker(&pc->psocket);  /* Ticks/buffer checks generated events */
    return;
  }
  if (next_tick &&
      pconnection_error(pc, uv_timer_start(&pc->timer, on_tick, next_tick, 0), "timer start")) {
    return;
  }
  if (wbuf.size > 0) {
    uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size);
    if (pconnection_error(
          pc, uv_write(&pc->write, (uv_stream_t*)&pc->psocket.tcp, &buf, 1, on_write), "write"))
      return;
    pc->writing = wbuf.size;
  } else if (pn_connection_driver_write_closed(&pc->driver)) {
    pc->shutdown.data = &pc->psocket;
    if (pconnection_error(
          pc, uv_shutdown(&pc->shutdown, (uv_stream_t*)&pc->psocket.tcp, NULL), "shutdown write"))
      return;
  }
  if (rbuf.size > 0) {
    if (pconnection_error(
          pc, uv_read_start((uv_stream_t*)&pc->psocket.tcp, alloc_read_buffer, on_read), "read"))
        return;
  }
}

static void listener_to_uv(pn_listener_t *l) {
  to_uv(&l->psocket);           /* Assume we're going to UV unless sent elsewhere */
  if (l->err) {
    if (!uv_is_closing((uv_handle_t*)&l->psocket.tcp)) {
      uv_close((uv_handle_t*)&l->psocket.tcp, on_close_psocket);
    }
  } else {
    if (l->accepting) {
      leader_accept(l);
    }
    if (l->connections) {
      listener_to_worker(l);
    }
  }
}

/* Monitor a psocket_t in the UV loop */
static void psocket_to_uv(psocket_t *ps) {
  if (ps->is_conn) {
    pconnection_to_uv(as_pconnection(ps));
  } else {
    listener_to_uv(as_listener(ps));
  }
}

/* Detach a connection from IO and put it on the worker queue */
static void pconnection_to_worker(pconnection_t *pc) {
  /* Can't go to worker if a write is outstanding or the batch is empty */
  if (!pc->writing && pn_connection_driver_has_event(&pc->driver)) {
    uv_read_stop((uv_stream_t*)&pc->psocket.tcp);
    uv_timer_stop(&pc->timer);
  }
  to_worker(&pc->psocket);
}

/* Can't really detach a listener, as on_connection can always be called.
   Generate events here safely.
*/
static void listener_to_worker(pn_listener_t *l) {
  if (pn_collector_peek(l->collector)) { /* Already have events */
    to_worker(&l->psocket);
  } else if (l->err) {
    if (l->err != UV_EOF) {
      pn_condition_format(l->condition, COND_NAME, "%s %s:%s: %s",
                          l->what, fixstr(l->psocket.host), fixstr(l->psocket.port),
                          uv_strerror(l->err));
    }
    l->err = 0;
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_CLOSE);
    to_worker(&l->psocket);
  } else if (l->connections) {    /* Generate accept events one at a time */
    --l->connections;
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
    to_worker(&l->psocket);
  } else {
    listener_to_uv(l);
  }
}

/* Set the event in the proactor's batch  */
static pn_event_batch_t *proactor_batch_lh(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, pn_proactor__class(), p, t);
  p->batch_working = true;
  return &p->batch;
}

/* Return the next event batch or 0 if no events are available in the worker_q */
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
    assert(ps->state == ON_WORKER);
    if (ps->is_conn) {
      return &as_pconnection(ps)->driver.batch;
    } else {                    /* Listener */
      return &as_listener(ps)->batch;
    }
  }
  return 0;
}

pn_listener_t *pn_event_listener(pn_event_t *e) {
  return (pn_event_class(e) == pn_listener__class()) ? (pn_listener_t*)pn_event_context(e) : NULL;
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == pn_proactor__class()) {
    return (pn_proactor_t*)pn_event_context(e);
  }
  pn_listener_t *l = pn_event_listener(e);
  if (l) {
    return l->psocket.proactor;
  }
  pn_connection_t *c = pn_event_connection(e);
  if (c) {
    return pn_connection_proactor(pn_event_connection(e));
  }
  return NULL;
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) {
    assert(pc->psocket.state == ON_WORKER);
    if (pn_connection_driver_has_event(&pc->driver)) {
      /* Process all events before going back to leader */
      pconnection_to_worker(pc);
    } else {
      to_leader(&pc->psocket, psocket_to_uv);
    }
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    assert(l->psocket.state == ON_WORKER);
    to_leader(&l->psocket, psocket_to_uv);
    return;
  }
  pn_proactor_t *bp = batch_proactor(batch);
  if (bp == p) {
    uv_mutex_lock(&p->lock);
    p->batch_working = false;
    uv_mutex_unlock(&p->lock);
    return;
  }
  uv_async_send(&p->async); /* Wake leader */
}

/* Process the leader_q, in the leader thread */
static void leader_process_lh(pn_proactor_t *p) {
  if (p->timeout_request) {
    p->timeout_request = false;
    if (p->timeout) {
      uv_timer_start(&p->timer, on_timeout, p->timeout, 0);
    } else {
      uv_timer_stop(&p->timer);
    }
  }
  for (psocket_t *ps = pop_lh(&p->leader_q); ps; ps = pop_lh(&p->leader_q)) {
    assert(ps->state == ON_LEADER);
    if (ps->wakeup) {
      uv_mutex_unlock(&p->lock);
      ps->wakeup(ps);
      ps->wakeup = NULL;
      uv_mutex_lock(&p->lock);
    } else if (ps->action) {
      uv_mutex_unlock(&p->lock);
      ps->action(ps);
      ps->action = NULL;
      uv_mutex_lock(&p->lock);
    }
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
      leader_process_lh(p);
      batch = get_batch_lh(p);
      if (batch == NULL) {
        uv_mutex_unlock(&p->lock);
        uv_run(&p->loop, UV_RUN_ONCE);
        uv_mutex_lock(&p->lock);
      }
    }
    /* Signal the next leader and go to work */
    p->has_leader = false;
    uv_cond_signal(&p->cond);
  }
  uv_mutex_unlock(&p->lock);
  return batch;
}

pn_event_batch_t *pn_proactor_get(struct pn_proactor_t* p) {
  uv_mutex_lock(&p->lock);
  pn_event_batch_t *batch = get_batch_lh(p);
  if (batch == NULL && !p->has_leader) {
    /* If there is no leader, try a non-waiting lead to generate some work */
    p->has_leader = true;
    leader_process_lh(p);
    uv_mutex_unlock(&p->lock);
    uv_run(&p->loop, UV_RUN_NOWAIT);
    uv_mutex_lock(&p->lock);
    batch = get_batch_lh(p);
    p->has_leader = false;
  }
  uv_mutex_unlock(&p->lock);
  return batch;
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  ++p->interrupt;
  uv_mutex_unlock(&p->lock);
  uv_async_send(&p->async);   /* Interrupt the UV loop */
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  uv_mutex_lock(&p->lock);
  p->timeout = t;
  p->timeout_request = true;
  uv_mutex_unlock(&p->lock);
  uv_async_send(&p->async);   /* Interrupt the UV loop */
}

int pn_proactor_connect(pn_proactor_t *p, pn_connection_t *c, const char *host, const char *port) {
  pconnection_t *pc = pconnection(p, c, false, host, port);
  if (!pc) {
    return PN_OUT_OF_MEMORY;
  }
  to_leader(&pc->psocket, leader_connect);
  return 0;
}

int pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *host, const char *port, int backlog)
{
  psocket_init(&l->psocket, p, false, host, port);
  l->backlog = backlog;
  to_leader(&l->psocket, leader_listen);
  return 0;
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->psocket.proactor : NULL;
}

void leader_wake_connection(psocket_t *ps) {
  assert(ps->state == ON_LEADER);
  pconnection_t *pc = as_pconnection(ps);
  pn_connection_t *c = pc->driver.connection;
  pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
  pconnection_to_worker(pc);
}

void pn_connection_wake(pn_connection_t* c) {
  /* May be called from any thread */
  wakeup(&get_pconnection(c)->psocket, leader_wake_connection);
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
  /* Close all the TCP handles. on_close_psocket will close any other handles if needed */
  if (h->type == UV_TCP && !uv_is_closing(h)) {
    uv_close(h, on_close_psocket);
  }
}

void pn_proactor_free(pn_proactor_t *p) {
  uv_timer_stop(&p->timer);
  uv_close((uv_handle_t*)&p->timer, NULL);
  uv_close((uv_handle_t*)&p->async, NULL);
  uv_walk(&p->loop, on_stopping, NULL); /* Close all TCP handles */
  while (uv_loop_alive(&p->loop)) {
    uv_run(&p->loop, UV_RUN_ONCE);       /* Run till all handles closed */
  }
  uv_loop_close(&p->loop);
  uv_mutex_destroy(&p->lock);
  uv_cond_destroy(&p->cond);
  pn_collector_free(p->collector);
  free(p);
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  assert(l->psocket.state == ON_WORKER);
  pn_event_t *prev = pn_collector_prev(l->collector);
  if (prev && pn_event_type(prev) == PN_LISTENER_CLOSE) {
    l->err = UV_EOF;
  }
  return pn_collector_next(l->collector);
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  assert(p->batch_working);
  return pn_collector_next(p->collector);
}

static void pn_listener_free(pn_listener_t *l) {
  /* No  assert(l->psocket.state == ON_WORKER);  can be called during shutdown */
  if (l) {
    if (l->collector) pn_collector_free(l->collector);
    if (l->condition) pn_condition_free(l->condition);
    if (l->attachments) pn_free(l->attachments);
    free(l);
  }
}

pn_listener_t *pn_listener(void) {
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

void leader_listener_close(psocket_t *ps) {
  assert(ps->state = ON_LEADER);
  pn_listener_t *l = (pn_listener_t*)ps;
  l->err = UV_EOF;
  listener_to_uv(l);
}

void pn_listener_close(pn_listener_t* l) {
  /* This can be called from any thread, not just the owner of l */
  wakeup(&l->psocket, leader_listener_close);
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
  assert(l->psocket.state == ON_WORKER);
  if (l->accepting) {
    return PN_STATE_ERR;        /* Only one at a time */
  }
  l->accepting = pconnection(l->psocket.proactor, c, true, l->psocket.host, l->psocket.port);
  if (!l->accepting) {
    return UV_ENOMEM;
  }
  return 0;
}
