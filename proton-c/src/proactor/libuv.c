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

/* All asserts are cheap and should remain in a release build for debugability */
#undef NDEBUG
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
  - on_* - called in leader thread by uv_run().
  - leader_* - called in leader thread (either leader_q processing or from an on_ function)
  - worker_* - called in worker thread
  - *_lh - called with the relevant lock held

  LIFECYCLE: pconnection_t and pn_listener_t objects must not be deleted until all their
  UV handles have received an on_close(). Freeing resources is always initiated by
  uv_close() of the uv_tcp_t handle, and completed in on_close() handler functions when it
  is safe. The only exception is when an error occurs that prevents a pn_connection_t or
  pn_listener_t from being associated with a uv handle at all.
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

/* A psocket (connection or listener) has the following *mutually exclusive* states. */
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

  /* Only used by leader when it owns the psocket */
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
  bool reading;                 /* true if a read request is pending */
  bool server;                  /* accept, not connect */
} pconnection_t;

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

/* Set state and action and push to relevant queue */
static inline void set_state_lh(psocket_t *ps, psocket_state_t state, void (*action)(psocket_t*)) {
  /* Illegal if ps is already listed under a different state */
  assert(ps->next == &UNLISTED || ps->state == state);
  ps->state = state;
  if (action && !ps->action) {
    ps->action = action;
  }
  switch(state) {
   case ON_LEADER: push_lh(&ps->proactor->leader_q, ps); break;
   case ON_WORKER: push_lh(&ps->proactor->worker_q, ps); break;
   case ON_UV:
    assert(ps->next == &UNLISTED);
    break;           /* No queue for UV loop */
  }
}

/* Set state and action, push to queue and notify leader. Thread safe. */
static void set_state(psocket_t *ps, psocket_state_t state, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  set_state_lh(ps, state, action);
  uv_async_send(&ps->proactor->async);
  uv_mutex_unlock(&ps->proactor->lock);
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
  /* No assert(ps->state == ON_UV); may be called in other states during shutdown. */
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

static void leader_unwatch(psocket_t *ps);

static void leader_error(psocket_t *ps, int err, const char* what) {
  assert(ps->state != ON_WORKER);
  if (ps->is_conn) {
    pn_connection_driver_t *driver = &as_pconnection(ps)->driver;
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
    l->closing = true;
  }
  leader_unwatch(ps);               /* Worker to handle the error */
}

/* uv-initialization */
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
  }
  if (err) {
    leader_error(ps, err, "initialization");
  }
  return err;
}

/* Outgoing connection */
static void on_connect(uv_connect_t *connect, int err) {
  pconnection_t *pc = (pconnection_t*)connect->data;
  assert(pc->psocket.state == ON_UV);
  if (!err) {
    leader_unwatch(&pc->psocket);
  } else {
    leader_error(&pc->psocket, err, "on connect to");
  }
}

/* Incoming connection ready to be accepted */
static void on_connection(uv_stream_t* server, int err) {
  /* Unlike most on_* functions, this one can be called by the leader thrad when the
   * listener is ON_WORKER, because there's no way to stop libuv from calling
   * on_connection() in leader_unwatch().  Just increase a counter and deal with it in the
   * worker thread.
   */
  pn_listener_t *l = (pn_listener_t*) server->data;
  assert(l->psocket.state == ON_UV);
  if (!err) {
    ++l->connections;
    leader_unwatch(&l->psocket);
  } else {
    leader_error(&l->psocket, err, "on connection from");
  }
}

static void leader_accept(pn_listener_t * l) {
  assert(l->psocket.state == ON_UV);
  assert(l->accepting);
  pconnection_t *pc = l->accepting;
  l->accepting = NULL;
  int err = leader_init(&pc->psocket);
  if (!err) {
    err = uv_accept((uv_stream_t*)&l->psocket.tcp, (uv_stream_t*)&pc->psocket.tcp);
  }
  if (!err) {
    leader_unwatch(&pc->psocket);
  } else {
    leader_error(&pc->psocket, err, "accepting from");
    leader_error(&l->psocket, err, "accepting from");
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
    leader_error(ps, err, "connecting to");
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
    set_state(ps, ON_UV, NULL);
  } else {
    leader_error(ps, err, "listening on");
  }
}

/* Generate tick events and return millis till next tick or 0 if no tick is required */
static pn_millis_t leader_tick(pconnection_t *pc) {
  assert(pc->psocket.state != ON_WORKER);
  pn_transport_t *t = pc->driver.transport;
  if (pn_transport_get_idle_timeout(t) || pn_transport_get_remote_idle_timeout(t)) {
    uint64_t now = uv_now(pc->timer.loop);
    uint64_t next = pn_transport_tick(t, now);
    return next ? next - now : 0;
  }
  return 0;
}

static void on_tick(uv_timer_t *timer) {
  if (!timer->data) return;     /* timer closed */
  pconnection_t *pc = (pconnection_t*)timer->data;
  assert(pc->psocket.state == ON_UV);
  uv_timer_stop(&pc->timer);
  pn_millis_t next = leader_tick(pc);
  if (pn_connection_driver_has_event(&pc->driver)) {
    leader_unwatch(&pc->psocket);
  } else if (next) {
    uv_timer_start(&pc->timer, on_tick, next, 0);
  }
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  assert(pc->psocket.state == ON_UV);
  if (nread >= 0) {
    pn_connection_driver_read_done(&pc->driver, nread);
    on_tick(&pc->timer);         /* check for tick changes. */
    /* Reading continues automatically until stopped. */
  } else if (nread == UV_EOF) { /* hangup */
    pn_connection_driver_read_close(&pc->driver);
    leader_unwatch(&pc->psocket);
  } else {
    leader_error(&pc->psocket, nread, "on read from");
  }
}

static void on_write(uv_write_t* write, int err) {
  pconnection_t *pc = (pconnection_t*)write->data;
  assert(pc->psocket.state == ON_UV);
  size_t writing = pc->writing;
  pc->writing = 0;              /* This write is done regardless of outcome */
  if (err == 0) {
    pn_connection_driver_write_done(&pc->driver, writing);
    leader_unwatch(&pc->psocket);
  } else if (err == UV_ECANCELED) {
    leader_unwatch(&pc->psocket);    /* cancelled by leader_unwatch, complete the job */
  } else {
    leader_error(&pc->psocket, err, "on write to");
  }
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
  assert(pc->psocket.state == ON_UV);
  pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
  *buf = uv_buf_init(rbuf.start, rbuf.size);
}

/* Monitor a socket in the UV loop */
static void leader_watch(psocket_t *ps) {
  assert(ps->state == ON_LEADER);
  int err = 0;
  set_state(ps, ON_UV, NULL); /* Assume we are going to UV loop unless sent to worker or leader. */

  if (ps->is_conn) {
    pconnection_t *pc = as_pconnection(ps);
    if (pn_connection_driver_finished(&pc->driver)) {
      uv_close((uv_handle_t*)&ps->tcp, on_close_psocket);
      return;
    }
    pn_millis_t next_tick = leader_tick(pc);
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
    if (pn_connection_driver_has_event(&pc->driver)) {
      /* Ticks and checking buffers have generated events, send back to worker to process */
      set_state(ps, ON_WORKER, NULL);
      return;
    }
    if (next_tick) {
      uv_timer_start(&pc->timer, on_tick, next_tick, 0);
    }
    if (wbuf.size > 0 && !pc->writing) {
      pc->writing = wbuf.size;
      uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size);
      err = uv_write(&pc->write, (uv_stream_t*)&pc->psocket.tcp, &buf, 1, on_write);
    } else if (wbuf.size == 0 && pn_connection_driver_write_closed(&pc->driver)) {
      pc->shutdown.data = ps;
      err = uv_shutdown(&pc->shutdown, (uv_stream_t*)&pc->psocket.tcp, NULL);
    }
    if (rbuf.size > 0 && !pc->reading) {
      pc->reading = true;
      err = uv_read_start((uv_stream_t*)&pc->psocket.tcp, alloc_read_buffer, on_read);
    }
  } else {
    pn_listener_t *l = as_listener(ps);
    if (l->closing && pn_collector_peek(l->collector)) {
      uv_close((uv_handle_t*)&ps->tcp, on_close_psocket);
      return;
    } else {
      if (l->accepting) {
        leader_accept(l);
      }
      if (l->connections) {
        leader_unwatch(ps);
      }
    }
  }
  if (err) {
    leader_error(ps, err, "re-watching");
  }
}

/* Detach a socket from IO and put it on the worker queue */
static void leader_unwatch(psocket_t *ps) {
  assert(ps->state != ON_WORKER); /* From ON_UV or ON_LEADER */
  if (ps->is_conn) {
    pconnection_t *pc = as_pconnection(ps);
    if (!pn_connection_driver_has_event(&pc->driver)) {
      /* Don't return an empty event batch */
      if (ps->state == ON_UV) {
        return;                 /* Just leave it in the UV loop */
      } else {
        leader_watch(ps);     /* Re-attach to UV loop */
      }
      return;
    } else {
      if (pc->writing) {
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
  }
  set_state(ps, ON_WORKER, NULL);
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
    assert(ps->state == ON_WORKER);
    if (ps->is_conn) {
      pconnection_t *pc = as_pconnection(ps);
      return &pc->driver.batch;
    } else {                    /* Listener */
      pn_listener_t *l = as_listener(ps);
      /* Generate accept events one at a time */
      if (l->connections && !pn_collector_peek(l->collector)) {
        --l->connections;
        pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
      }
      return &l->batch;
    }
    set_state_lh(ps, ON_LEADER, NULL); /* No event, back to leader */
  }
  return 0;
}

/* Called in any thread to set a wakeup action */
static void wakeup(psocket_t *ps, void (*action)(psocket_t*)) {
  uv_mutex_lock(&ps->proactor->lock);
  if (action && !ps->wakeup) {
    ps->wakeup = action;
  }
  set_state_lh(ps, ON_LEADER, NULL);
  uv_async_send(&ps->proactor->async); /* Wake leader */
  uv_mutex_unlock(&ps->proactor->lock);
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
      set_state(&pc->psocket, ON_WORKER, NULL);
    } else {
      set_state(&pc->psocket, ON_LEADER, leader_watch);
    }
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    assert(l->psocket.state == ON_WORKER);
    set_state(&l->psocket, ON_LEADER, leader_watch);
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
  pconnection_t *pc = pconnection(p, c, false, host, port);
  if (!pc) {
    return PN_OUT_OF_MEMORY;
  }
  set_state(&pc->psocket, ON_LEADER, leader_connect);
  return 0;
}

int pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *host, const char *port, int backlog)
{
  psocket_init(&l->psocket, p, false, host, port);
  l->backlog = backlog;
  set_state(&l->psocket, ON_LEADER, leader_listen);
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
  leader_unwatch(ps);
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
  return pn_collector_next(l->collector);
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  return pn_collector_next(batch_proactor(batch)->collector);
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

void leader_listener_close(psocket_t *ps) {
  assert(ps->state = ON_LEADER);
  pn_listener_t *l = (pn_listener_t*)ps;
  l->closing = true;
  leader_watch(ps);
}

void pn_listener_close(pn_listener_t* l) {
  /* This can be called from any thread, not just the owner of l */
  wakeup(&l->psocket, leader_listener_close);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  assert(l->psocket.state == ON_WORKER);
  return l ? l->psocket.proactor : NULL;
}

pn_condition_t* pn_listener_condition(pn_listener_t* l) {
  assert(l->psocket.state == ON_WORKER);
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
