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

#include "../core/log_private.h"
#include "../core/url-internal.h"

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>

#include <uv.h>

/* All asserts are cheap and should remain in a release build for debugability */
#undef NDEBUG
#include <assert.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  libuv functions are thread unsafe, we use a"leader-worker-follower" model as follows:

  - At most one thread at a time is the "leader". The leader runs the UV loop till there
  are events to process and then becomes a "worker"n

  - Concurrent "worker" threads process events for separate connections or listeners.
  When they run out of work they become "followers"

  - A "follower" is idle, waiting for work. When the leader becomes a worker, one follower
  takes over as the new leader.

  Any thread that calls pn_proactor_wait() or pn_proactor_get() can take on any of the
  roles as required at run-time. Monitored sockets (connections or listeners) are passed
  between threads on thread-safe queues.

  Function naming:
  - on_*() - libuv callbacks, called in leader thread via  uv_run().
  - leader_* - only called in leader thread from
  - *_lh - called with the relevant lock held
*/

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

/* common to connection and listener */
typedef struct psocket_t {
  /* Immutable */
  pn_proactor_t *proactor;
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  bool is_conn;

  /* Protected by proactor.lock */
  struct psocket_t* next;
  bool working;                      /* Owned by a worker thread */

  /* Only used by leader thread when it owns the psocket */
  uv_tcp_t tcp;
} psocket_t;

typedef struct queue { psocket_t *front, *back; } queue;

/* Special value for psocket.next pointer when socket is not on any any list. */
psocket_t UNLISTED;

static void psocket_init(psocket_t* ps, pn_proactor_t* p, bool is_conn, const char *host, const char *port) {
  ps->proactor = p;
  ps->next = &UNLISTED;
  ps->is_conn = is_conn;
  ps->tcp.data = NULL;          /* Set in leader_init */
  ps->working = true;

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

/* a connection socket  */
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

  /* Locked for thread-safe access */
  uv_mutex_t lock;
  bool wake;                    /* pn_connection_wake() was called */
} pconnection_t;


/* a listener socket */
struct pn_listener_t {
  psocket_t psocket;

  /* Only used by owner thread */
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *context;
  size_t backlog;

  /* Locked for thread-safe access. uv_listen can't be stopped or cancelled so we can't
   * detach a listener from the UV loop to prevent concurrent access.
   */
  uv_mutex_t lock;
  pn_condition_t *condition;
  pn_collector_t *collector;
  queue          accept;        /* pconnection_t for uv_accept() */
  bool closed;
};

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

/* Notify the leader thread that there is something to do outside of uv_run() */
static inline void notify(pn_proactor_t* p) {
  uv_async_send(&p->async);
}

/* Notify that this socket needs attention from the leader at the next opportunity */
static void psocket_notify(psocket_t *ps) {
  uv_mutex_lock(&ps->proactor->lock);
  /* If the socket is in use by a worker or is already queued then leave it where it is.
     It will be processed in pn_proactor_done() or when the queue it is on is processed.
  */
  if (!ps->working && ps->next == &UNLISTED) {
    push_lh(&ps->proactor->leader_q, ps);
    notify(ps->proactor);
  }
  uv_mutex_unlock(&ps->proactor->lock);
}

/* Notify the leader of a newly-created socket */
static void psocket_start(psocket_t *ps) {
  uv_mutex_lock(&ps->proactor->lock);
  if (ps->next == &UNLISTED) {  /* No-op if already queued */
    ps->working = false;
    push_lh(&ps->proactor->leader_q, ps);
    notify(ps->proactor);
    uv_mutex_unlock(&ps->proactor->lock);
  }
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

static inline psocket_t *batch_psocket(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) return &pc->psocket;
  pn_listener_t *l = batch_listener(batch);
  if (l) return &l->psocket;
  return NULL;
}

static void leader_count(pn_proactor_t *p, int change) {
  uv_mutex_lock(&p->lock);
  p->count += change;
  if (p->count == 0) {
    p->inactive = true;
    notify(p);
  }
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

static void uv_safe_close(uv_handle_t *h, uv_close_cb cb) {
  if (!uv_is_closing(h)) {
    uv_close(h, cb);
  }
}

/* Close event for uv_tcp_t of a psocket_t */
static void on_close_psocket(uv_handle_t *h) {
  psocket_t *ps = (psocket_t*)h->data;
  leader_count(ps->proactor, -1);
  if (ps->is_conn) {
    pconnection_t *pc = as_pconnection(ps);
    uv_timer_stop(&pc->timer);
    /* Delay the free till the timer handle is also closed */
    uv_safe_close((uv_handle_t*)&pc->timer, on_close_pconnection_final);
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

static void pconnection_error(pconnection_t *pc, int err, const char* what) {
  assert(err);
  pn_connection_driver_t *driver = &pc->driver;
  pn_connection_driver_bind(driver); /* Bind so errors will be reported */
  if (!pn_condition_is_set(pn_transport_condition(driver->transport))) {
    pn_connection_driver_errorf(driver, uv_err_name(err), "%s %s:%s: %s",
                                what, fixstr(pc->psocket.host), fixstr(pc->psocket.port),
                                uv_strerror(err));
  }
  pn_connection_driver_close(driver);
}

static void listener_error(pn_listener_t *l, int err, const char* what) {
  assert(err);
  uv_mutex_lock(&l->lock);
  if (!pn_condition_is_set(l->condition)) {
    pn_condition_format(l->condition, uv_err_name(err), "%s %s:%s: %s",
                        what, fixstr(l->psocket.host), fixstr(l->psocket.port),
                        uv_strerror(err));
  }
  uv_mutex_unlock(&l->lock);
  pn_listener_close(l);
}

static void psocket_error(psocket_t *ps, int err, const char* what) {
  if (ps->is_conn) {
    pconnection_error(as_pconnection(ps), err, "initialization");
  } else {
    listener_error(as_listener(ps), err, "initialization");
  }
}

/* psocket uv-initialization */
static int leader_init(psocket_t *ps) {
  ps->working = false;
  ps->tcp.data = ps;
  leader_count(ps->proactor, +1);
  int err = uv_tcp_init(&ps->proactor->loop, &ps->tcp);
  if (err) {
    psocket_error(ps, err, "initialization");
  } else {
    pconnection_t *pc = as_pconnection(ps);
    if (pc) {
      pc->connect.data = ps;
      int err = uv_timer_init(&ps->proactor->loop, &pc->timer);
      if (!err) {
        pc->timer.data = ps;
      }
    }
  }
  return err;
}

/* Outgoing connection */
static void on_connect(uv_connect_t *connect, int err) {
  pconnection_t *pc = (pconnection_t*)connect->data;
  if (err) pconnection_error(pc, err, "on connect to");
  psocket_notify(&pc->psocket);
}

/* Incoming connection ready to be accepted */
static void on_connection(uv_stream_t* server, int err) {
  /* Unlike most on_* functions, this can be called by the leader thread when the listener
   * is ON_WORKER or ON_LEADER, because there's no way to stop libuv from calling
   * on_connection(). Update the state of the listener and queue it for leader attention.
   */
  pn_listener_t *l = (pn_listener_t*) server->data;
  if (err) {
    listener_error(l, err, "on incoming connection");
  } else {
    uv_mutex_lock(&l->lock);
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
    uv_mutex_unlock(&l->lock);
    psocket_notify(&l->psocket);
  }
}

/* Common address resolution for leader_listen and leader_connect */
static int leader_resolve(psocket_t *ps, uv_getaddrinfo_t *info, bool server) {
  int err = leader_init(ps);
  struct addrinfo hints = { 0 };
  if (server) hints.ai_flags = AI_PASSIVE;
  if (!err) {
    err = uv_getaddrinfo(&ps->proactor->loop, info, NULL, fixstr(ps->host), fixstr(ps->port), &hints);
  }
  return err;
}

static void leader_connect(pconnection_t *pc) {
  uv_getaddrinfo_t info;
  int err = leader_resolve(&pc->psocket, &info, false);
  if (!err) {
    err = uv_tcp_connect(&pc->connect, &pc->psocket.tcp, info.addrinfo->ai_addr, on_connect);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (err) {
    pconnection_error(pc, err, "connecting to");
  } else {
    pn_connection_open(pc->driver.connection);
  }
}

static void leader_listen(pn_listener_t *l) {
  uv_getaddrinfo_t info;
  int err = leader_resolve(&l->psocket, &info, true);
  if (!err) {
    err = uv_tcp_bind(&l->psocket.tcp, info.addrinfo->ai_addr, 0);
    uv_freeaddrinfo(info.addrinfo);
  }
  if (!err) {
    err = uv_listen((uv_stream_t*)&l->psocket.tcp, l->backlog, on_connection);
  }
  uv_mutex_lock(&l->lock);
  /* Always put an OPEN event for symmetry, even if we immediately close with err */
  pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_OPEN);
  uv_mutex_unlock(&l->lock);
  if (err) {
    listener_error(l, err, "listening on");
  }
}

static bool listener_has_work(pn_listener_t *l) {
  uv_mutex_lock(&l->lock);
  bool has_work = pn_collector_peek(l->collector);
  uv_mutex_unlock(&l->lock);
  return has_work;
}

static pconnection_t *listener_pop(pn_listener_t *l) {
  uv_mutex_lock(&l->lock);
  pconnection_t *pc = (pconnection_t*)pop_lh(&l->accept);
  uv_mutex_unlock(&l->lock);
  return pc;
}

static bool listener_finished(pn_listener_t *l) {
  uv_mutex_lock(&l->lock);
  bool finished = l->closed && !pn_collector_peek(l->collector) && !l->accept.front;
  uv_mutex_unlock(&l->lock);
  return finished;
}

/* Process a listener, return true if it has events for a worker thread */
static bool leader_process_listener(pn_listener_t *l) {
  /* NOTE: l may be concurrently accessed by on_connection() */

  if (l->psocket.tcp.data == NULL) {
    /* Start listening if not already listening */
    leader_listen(l);
  } else if (listener_finished(l)) {
    /* Close if listener is finished. */
    uv_safe_close((uv_handle_t*)&l->psocket.tcp, on_close_psocket);
    return false;
  } else {
    /* Process accepted connections if any */
    pconnection_t *pc;
    while ((pc = listener_pop(l))) {
      int err = leader_init(&pc->psocket);
      if (!err) {
        err = uv_accept((uv_stream_t*)&l->psocket.tcp, (uv_stream_t*)&pc->psocket.tcp);
      } else {
        listener_error(l, err, "accepting from");
        pconnection_error(pc, err, "accepting from");
      }
      psocket_start(&pc->psocket);
    }
  }
  return listener_has_work(l);
}

/* Generate tick events and return millis till next tick or 0 if no tick is required */
static pn_millis_t leader_tick(pconnection_t *pc) {
  uint64_t now = uv_now(pc->timer.loop);
  uint64_t next = pn_transport_tick(pc->driver.transport, now);
  return next ? next - now : 0;
}

static void on_tick(uv_timer_t *timer) {
  pconnection_t *pc = (pconnection_t*)timer->data;
  leader_tick(pc);
  psocket_notify(&pc->psocket);
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  if (nread >= 0) {
    pn_connection_driver_read_done(&pc->driver, nread);
  } else if (nread == UV_EOF) { /* hangup */
    pn_connection_driver_read_close(&pc->driver);
  } else {
    pconnection_error(pc, nread, "on read from");
  }
  psocket_notify(&pc->psocket);
}

static void on_write(uv_write_t* write, int err) {
  pconnection_t *pc = (pconnection_t*)write->data;
  size_t size = pc->writing;
  pc->writing = 0;
  if (err) {
    pconnection_error(pc, err, "on write to");
  } else {
    pn_connection_driver_write_done(&pc->driver, size);
  }
  psocket_notify(&pc->psocket);
}

static void on_timeout(uv_timer_t *timer) {
  pn_proactor_t *p = (pn_proactor_t*)timer->data;
  uv_mutex_lock(&p->lock);
  p->timeout_elapsed = true;
  uv_mutex_unlock(&p->lock);
}

/* Read buffer allocation function for uv, just returns the transports read buffer. */
static void alloc_read_buffer(uv_handle_t* stream, size_t size, uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
  *buf = uv_buf_init(rbuf.start, rbuf.size);
}

/* Set the event in the proactor's batch  */
static pn_event_batch_t *proactor_batch_lh(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, pn_proactor__class(), p, t);
  p->batch_working = true;
  return &p->batch;
}

static void on_stopping(uv_handle_t* h, void* v) {
  /* Close all the TCP handles. on_close_psocket will close any other handles if needed */
  if (h->type == UV_TCP) {
    uv_safe_close(h, on_close_psocket);
  }
}

static pn_event_t *log_event(void* p, pn_event_t *e) {
  if (e) {
    pn_logf("[%p]:(%s)", (void*)p, pn_event_type_name(pn_event_type(e)));
  }
  return e;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  uv_mutex_lock(&l->lock);
  pn_event_t *e = pn_collector_next(l->collector);
  uv_mutex_unlock(&l->lock);
  return log_event(l, e);
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  assert(p->batch_working);
  return log_event(p, pn_collector_next(p->collector));
}

static void pn_listener_free(pn_listener_t *l) {
  if (l) {
    if (l->collector) pn_collector_free(l->collector);
    if (l->condition) pn_condition_free(l->condition);
    if (l->attachments) pn_free(l->attachments);
    assert(!l->accept.front);
    free(l);
  }
}

/* Return the next event batch or NULL if no events are available */
static pn_event_batch_t *get_batch_lh(pn_proactor_t *p) {
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
    assert(ps->working);
    if (ps->is_conn) {
      return &as_pconnection(ps)->driver.batch;
    } else {                    /* Listener */
      return &as_listener(ps)->batch;
    }
  }
  return NULL;
}

/* Check and reset the wake flag */
static bool check_wake(pconnection_t *pc) {
  uv_mutex_lock(&pc->lock);
  bool wake = pc->wake;
  pc->wake = false;
  uv_mutex_unlock(&pc->lock);
  return wake;
}

/* Process a pconnection, return true if it has events for a worker thread */
static bool leader_process_pconnection(pconnection_t *pc) {
  /* Important to do the following steps in order */
  if (pc->psocket.tcp.data == NULL) {
    /* Start the connection if not already connected */
    leader_connect(pc);
  } else if (pn_connection_driver_finished(&pc->driver)) {
    /* Close if the connection is finished */
    uv_safe_close((uv_handle_t*)&pc->psocket.tcp, on_close_psocket);
  } else {
    /* Check for events that can be generated without blocking for IO */
    if (check_wake(pc)) {
      pn_connection_t *c = pc->driver.connection;
      pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
    }
    pn_millis_t next_tick = leader_tick(pc);
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);

    /* If we still have no events, make async UV requests */
    if (!pn_connection_driver_has_event(&pc->driver)) {
      int err = 0;
      const char *what = NULL;
      if (!err && next_tick) {
        what = "connection timer start";
        err = uv_timer_start(&pc->timer, on_tick, next_tick, 0);
      }
      if (!err && !pc->writing) {
        what = "write";
        if (wbuf.size > 0) {
          uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size);
          err = uv_write(&pc->write, (uv_stream_t*)&pc->psocket.tcp, &buf, 1, on_write);
          if (!err) {
            pc->writing = wbuf.size;
          }
        } else if (pn_connection_driver_write_closed(&pc->driver)) {
          uv_shutdown(&pc->shutdown, (uv_stream_t*)&pc->psocket.tcp, NULL);
        }
      }
      if (!err && rbuf.size > 0) {
        what = "read";
        err = uv_read_start((uv_stream_t*)&pc->psocket.tcp, alloc_read_buffer, on_read);
      }
      if (err) {
        /* Some IO requests failed, generate the error events */
        pconnection_error(pc, err, what);
      }
    }
  }
  return pn_connection_driver_has_event(&pc->driver);
}

/* Detach a connection from the UV loop so it can be used safely by a worker */
void pconnection_detach(pconnection_t *pc) {
  if (!pc->writing) {           /* Can't detach while a write is pending */
    uv_read_stop((uv_stream_t*)&pc->psocket.tcp);
    uv_timer_stop(&pc->timer);
  }
}

/* Process the leader_q and the UV loop, in the leader thread */
static pn_event_batch_t *leader_lead_lh(pn_proactor_t *p, uv_run_mode mode) {
  pn_event_batch_t *batch = NULL;
  for (psocket_t *ps = pop_lh(&p->leader_q); ps; ps = pop_lh(&p->leader_q)) {
    assert(!ps->working);

    uv_mutex_unlock(&p->lock);  /* Unlock to process each item, may add more items to leader_q */
    bool has_work = ps->is_conn ?
      leader_process_pconnection(as_pconnection(ps)) :
      leader_process_listener(as_listener(ps));
    uv_mutex_lock(&p->lock);

    if (has_work && !ps->working && ps->next == &UNLISTED) {
      if (ps->is_conn) {
        pconnection_detach(as_pconnection(ps));
      }
      ps->working = true;
      push_lh(&p->worker_q, ps);
    }
  }
  batch = get_batch_lh(p);      /* Check for work */
  if (!batch) { /* No work, run the UV loop */
    /* Set timeout timer before uv_run */
    if (p->timeout_request) {
      p->timeout_request = false;
      uv_timer_stop(&p->timer);
      if (p->timeout) {
        uv_timer_start(&p->timer, on_timeout, p->timeout, 0);
      }
    }
    uv_mutex_unlock(&p->lock);  /* Unlock to run UV loop */
    uv_run(&p->loop, mode);
    uv_mutex_lock(&p->lock);
    batch = get_batch_lh(p);
  }
  return batch;
}

/**** public API ****/

pn_event_batch_t *pn_proactor_get(struct pn_proactor_t* p) {
  uv_mutex_lock(&p->lock);
  pn_event_batch_t *batch = get_batch_lh(p);
  if (batch == NULL && !p->has_leader) {
    /* Try a non-blocking lead to generate some work */
    p->has_leader = true;
    batch = leader_lead_lh(p, UV_RUN_NOWAIT);
    p->has_leader = false;
    uv_cond_broadcast(&p->cond);   /* Signal followers for possible work */
  }
  uv_mutex_unlock(&p->lock);
  return batch;
}

pn_event_batch_t *pn_proactor_wait(struct pn_proactor_t* p) {
  uv_mutex_lock(&p->lock);
  pn_event_batch_t *batch = get_batch_lh(p);
  while (!batch && p->has_leader) {
    uv_cond_wait(&p->cond, &p->lock); /* Follow the leader */
    batch = get_batch_lh(p);
  }
  if (!batch) {                 /* Become leader */
    p->has_leader = true;
    do {
      batch = leader_lead_lh(p, UV_RUN_ONCE);
    } while (!batch);
    p->has_leader = false;
    uv_cond_broadcast(&p->cond); /* Signal a followers. One takes over, many can work. */
  }
  uv_mutex_unlock(&p->lock);
  return batch;
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  uv_mutex_lock(&p->lock);
  psocket_t *ps = batch_psocket(batch);
  if (ps) {
    assert(ps->working);
    assert(ps->next == &UNLISTED);
    ps->working = false;
    push_lh(&p->leader_q, ps);
  }
  pn_proactor_t *bp = batch_proactor(batch); /* Proactor events */
  if (bp == p) {
    p->batch_working = false;
  }
  uv_mutex_unlock(&p->lock);
  notify(p);
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

void pn_proactor_interrupt(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  ++p->interrupt;
  uv_mutex_unlock(&p->lock);
  notify(p);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  uv_mutex_lock(&p->lock);
  p->timeout = t;
  p->timeout_request = true;
  uv_mutex_unlock(&p->lock);
  notify(p);
}

int pn_proactor_connect(pn_proactor_t *p, pn_connection_t *c, const char *addr) {
  char *buf = strdup(addr);
  if (!buf) {
    return PN_OUT_OF_MEMORY;
  }
  char *scheme, *user, *pass, *host, *port, *path;
  pni_parse_url(buf, &scheme, &user, &pass, &host, &port, &path);
  pconnection_t *pc = pconnection(p, c, false, host, port);
  free(buf);
  if (!pc) {
    return PN_OUT_OF_MEMORY;
  }
  psocket_start(&pc->psocket);
  return 0;
}

int pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog) {
  assert(!l->closed);
  char *buf = strdup(addr);
  if (!buf) {
    return PN_OUT_OF_MEMORY;
  }
  char *scheme, *user, *pass, *host, *port, *path;
  pni_parse_url(buf, &scheme, &user, &pass, &host, &port, &path);
  psocket_init(&l->psocket, p, false, host, port);
  free(buf);
  l->backlog = backlog;
  psocket_start(&l->psocket);
  return 0;
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
  uv_timer_init(&p->loop, &p->timer);
  p->timer.data = p;
  return p;
}

void pn_proactor_free(pn_proactor_t *p) {
  uv_timer_stop(&p->timer);
  uv_safe_close((uv_handle_t*)&p->timer, NULL);
  uv_safe_close((uv_handle_t*)&p->async, NULL);
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

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->psocket.proactor : NULL;
}

void pn_connection_wake(pn_connection_t* c) {
  /* May be called from any thread */
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    uv_mutex_lock(&pc->lock);
    pc->wake = true;
    uv_mutex_unlock(&pc->lock);
    psocket_notify(&pc->psocket);
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

void pn_listener_close(pn_listener_t* l) {
  /* May be called from any thread */
  uv_mutex_lock(&l->lock);
  if (!l->closed) {
    l->closed = true;
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_CLOSE);
  }
  uv_mutex_unlock(&l->lock);
  psocket_notify(&l->psocket);
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
  uv_mutex_lock(&l->lock);
  assert(!l->closed);
  pconnection_t *pc = pconnection(l->psocket.proactor, c, true, l->psocket.host, l->psocket.port);
  if (!pc) {
    return PN_OUT_OF_MEMORY;
  }
  push_lh(&l->accept, &pc->psocket);
  uv_mutex_unlock(&l->lock);
  psocket_notify(&l->psocket);
  return 0;
}
