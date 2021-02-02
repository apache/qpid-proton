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

/* Enable POSIX features for uv.h */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "core/logger_private.h"
#include "proactor-internal.h"

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/raw_connection.h>
#include <proton/transport.h>

#include <uv.h>
#include "netaddr-internal.h"   /* Include after socket headers via uv.h */

/* All asserts are cheap and should remain in a release build for debuggability */
#undef NDEBUG
#include <assert.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  libuv functions are thread unsafe, we use a"leader-worker-follower" model as follows:

  - At most one thread at a time is the "leader". The leader runs the UV loop till there
  are events to process and then becomes a "worker"

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

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   CLASSDEF is for identification when used as a pn_event_t context.
*/
PN_STRUCT_CLASSDEF(pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener)


/* ================ Queues ================ */
static int unqueued;            /* Provide invalid address for _unqueued pointers */

#define QUEUE_DECL(T)                                                   \
  typedef struct T##_queue_t { T##_t *front, *back; } T##_queue_t;      \
                                                                        \
  static T##_t *T##_unqueued = (T##_t*)&unqueued;                       \
                                                                        \
  static void T##_push(T##_queue_t *q, T##_t *x) {                      \
    assert(x->next == T##_unqueued);                                    \
    x->next = NULL;                                                     \
    if (!q->front) {                                                    \
      q->front = q->back = x;                                           \
    } else {                                                            \
      q->back->next = x;                                                \
      q->back =  x;                                                     \
    }                                                                   \
  }                                                                     \
                                                                        \
  static T##_t* T##_pop(T##_queue_t *q) {                               \
    T##_t *x = q->front;                                                \
    if (x) {                                                            \
      q->front = x->next;                                               \
      x->next = T##_unqueued;                                           \
    }                                                                   \
    return x;                                                           \
  }


/* All work structs and UV callback data structs start with a struct_type member  */
typedef enum { T_CONNECTION, T_LISTENER, T_LSOCKET } struct_type;

/* A stream of serialized work for the proactor */
typedef struct work_t {
  /* Immutable */
  struct_type type;
  pn_proactor_t *proactor;

  /* Protected by proactor.lock */
  struct work_t* next;
  bool working;                      /* Owned by a worker thread */
} work_t;

QUEUE_DECL(work)

static void work_init(work_t* w, pn_proactor_t* p, struct_type type) {
  w->proactor = p;
  w->next = work_unqueued;
  w->type = type;
  w->working = true;
}

/* ================ IO ================ */

/* A resolvable address */
typedef struct addr_t {
  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
  uv_getaddrinfo_t getaddrinfo; /* UV getaddrinfo request, contains list of addrinfo */
  struct addrinfo* addrinfo;    /* The current addrinfo being tried */
} addr_t;

/* A single listening socket, a listener can have more than one */
typedef struct lsocket_t {
  struct_type type;             /* Always T_LSOCKET */
  pn_listener_t *parent;
  uv_tcp_t tcp;
  struct lsocket_t *next;
} lsocket_t;

PN_STRUCT_CLASSDEF(pn_listener_socket)

typedef enum { W_NONE, W_PENDING, W_CLOSED } wake_state;

/* An incoming or outgoing connection. */
typedef struct pconnection_t {
  work_t work;                  /* Must be first to allow casting */
  struct pconnection_t *next;   /* For listener list */

  /* Only used by owner thread */
  pn_connection_driver_t driver;
  pn_event_batch_t batch;

  /* Only used by leader */
  uv_tcp_t tcp;
  addr_t addr;

  uv_connect_t connect;         /* Outgoing connection only */
  int connected;      /* 0: not connected, <0: connecting after error, 1 = connected ok */

  lsocket_t *lsocket;           /* Incoming connection only */

  struct pn_netaddr_t local, remote; /* Actual addresses */
  uv_timer_t timer;
  uv_write_t write;
  size_t writing;               /* size of pending write request, 0 if none pending */
  uv_shutdown_t shutdown;

  /* Locked for thread-safe access */
  uv_mutex_t lock;
  wake_state wake;
} pconnection_t;

QUEUE_DECL(pconnection)

typedef enum {
  L_UNINIT,                     /**<< Not yet listening */
  L_LISTENING,                  /**<< Listening */
  L_CLOSE,                      /**<< Close requested  */
  L_CLOSING,                    /**<< Socket close initiated, wait for all to close */
  L_CLOSED                      /**<< User saw PN_LISTENER_CLOSED, all done  */
} listener_state;

/* A listener */
struct pn_listener_t {
  work_t work;                  /* Must be first to allow casting */

  /* Only used by owner thread */
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *context;
  size_t backlog;

  /* Only used by leader */
  addr_t addr;
  lsocket_t *lsockets;
  int dynamic_port;             /* Record dynamic port from first bind(0) */

  /* Invariant listening addresses allocated during leader_listen_lh() */
  struct pn_netaddr_t *addrs;
  int addrs_len;

  /* Locked for thread-safe access. uv_listen can't be stopped or cancelled so we can't
   * detach a listener from the UV loop to prevent concurrent access.
   */
  uv_mutex_t lock;
  pn_condition_t *condition;
  pn_collector_t *collector;
  pconnection_queue_t accept;   /* pconnection_t list for accepting */
  listener_state state;
};

typedef enum { TM_NONE, TM_REQUEST, TM_PENDING, TM_FIRED } timeout_state_t;

struct pn_proactor_t {
  /* Notification */
  uv_async_t notify;
  uv_async_t interrupt;

  /* Leader thread  */
  uv_cond_t cond;
  uv_loop_t loop;
  uv_timer_t timer;

  /* Owner thread: proactor collector and batch can belong to leader or a worker */
  pn_collector_t *collector;
  pn_event_batch_t batch;

  /* Protected by lock */
  uv_mutex_t lock;
  work_queue_t worker_q; /* ready for work, to be returned via pn_proactor_wait()  */
  work_queue_t leader_q; /* waiting for attention by the leader thread */
  timeout_state_t timeout_state;
  pn_millis_t timeout;
  size_t active;         /* connection/listener count for INACTIVE events */
  pn_condition_t *disconnect_cond; /* disconnect condition */

  bool has_leader;             /* A thread is working as leader */
  bool disconnect;             /* disconnect requested */
  bool batch_working;          /* batch is being processed in a worker thread */
  bool need_interrupt;         /* Need a PN_PROACTOR_INTERRUPT event */
  bool need_inactive;          /* need INACTIVE event */
  bool timeout_processed;
};


/* Notify the leader thread that there is something to do outside of uv_run() */
static inline void notify(pn_proactor_t* p) {
  uv_async_send(&p->notify);
}

/* Set the interrupt flag in the leader thread to avoid race conditions. */
void on_interrupt(uv_async_t *async) {
  if (async->data) {
    pn_proactor_t *p = (pn_proactor_t*)async->data;
    p->need_interrupt = true;
  }
}

/* Notify that this work item needs attention from the leader at the next opportunity */
static void work_notify(work_t *w) {
  uv_mutex_lock(&w->proactor->lock);
  /* If the socket is in use by a worker or is already queued then leave it where it is.
     It will be processed in pn_proactor_done() or when the queue it is on is processed.
  */
  if (!w->working && w->next == work_unqueued) {
    work_push(&w->proactor->leader_q, w);
    notify(w->proactor);
  }
  uv_mutex_unlock(&w->proactor->lock);
}

/* Notify the leader of a newly-created work item */
static void work_start(work_t *w) {
  uv_mutex_lock(&w->proactor->lock);
  if (w->next == work_unqueued) {  /* No-op if already queued */
    w->working = false;
    work_push(&w->proactor->leader_q, w);
    notify(w->proactor);
    uv_mutex_unlock(&w->proactor->lock);
  }
}

static void parse_addr(addr_t *addr, const char *str) {
  pni_parse_addr(str, addr->addr_buf, sizeof(addr->addr_buf), &addr->host, &addr->port);
}

/* Protect read/update of pn_connnection_t pointer to it's pconnection_t
 *
 * Global because pn_connection_wake()/pn_connection_proactor() navigate from
 * the pn_connection_t before we know the proactor or driver. Critical sections
 * are small: only get/set of the pn_connection_t driver pointer.
 *
 * TODO: replace mutex with atomic load/store
 */
static pthread_mutex_t driver_ptr_mutex;

static uv_once_t global_init_once = UV_ONCE_INIT;
static void global_init_fn(void) {  /* Call via uv_once(&global_init_once, global_init_fn) */
  uv_mutex_init(&driver_ptr_mutex);
}

static pconnection_t *get_pconnection(pn_connection_t* c) {
  if (!c) return NULL;
  uv_mutex_lock(&driver_ptr_mutex);
  pn_connection_driver_t *d = *pn_connection_driver_ptr(c);
  uv_mutex_unlock(&driver_ptr_mutex);
  if (!d) return NULL;
  return containerof(d, pconnection_t, driver);
}

static void set_pconnection(pn_connection_t* c, pconnection_t *pc) {
  uv_mutex_lock(&driver_ptr_mutex);
  *pn_connection_driver_ptr(c) = pc ? &pc->driver : NULL;
  uv_mutex_unlock(&driver_ptr_mutex);
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch);

static pconnection_t *pconnection(pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, bool server) {
  pconnection_t *pc = (pconnection_t*)calloc(1, sizeof(*pc));
  if (!pc || pn_connection_driver_init(&pc->driver, c, t) != 0) {
    return NULL;
  }
  work_init(&pc->work, p,  T_CONNECTION);
  pc->batch.next_event = pconnection_batch_next;
  pc->next = pconnection_unqueued;
  pc->write.data = &pc->work;
  uv_mutex_init(&pc->lock);
  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }
  set_pconnection(pc->driver.connection, pc);
  return pc;
}

static void pconnection_free(pconnection_t *pc) {
  pn_connection_t *c = pc->driver.connection;
  if (c) set_pconnection(c, NULL);
  pn_connection_driver_destroy(&pc->driver);
  if (pc->addr.getaddrinfo.addrinfo) {
    uv_freeaddrinfo(pc->addr.getaddrinfo.addrinfo); /* Interrupted after resolve */
  }
  uv_mutex_destroy(&pc->lock);
  free(pc);
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch);
static pn_event_t *proactor_batch_next(pn_event_batch_t *batch);

static inline pn_proactor_t *batch_proactor(pn_event_batch_t *batch) {
  return (batch->next_event == proactor_batch_next) ?
    containerof(batch, pn_proactor_t, batch) : NULL;
}

static inline pn_listener_t *batch_listener(pn_event_batch_t *batch) {
  return (batch->next_event == listener_batch_next) ?
    containerof(batch, pn_listener_t, batch) : NULL;
}

static inline pconnection_t *batch_pconnection(pn_event_batch_t *batch) {
  return (batch->next_event == pconnection_batch_next) ?
    containerof(batch, pconnection_t, batch) : NULL;
}

static inline work_t *batch_work(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) return &pc->work;
  pn_listener_t *l = batch_listener(batch);
  if (l) return &l->work;
  return NULL;
}

static void check_for_inactive(pn_proactor_t *p) {
  /* No future events: no active socket io, no pending timer, no
     current event processing. */
  if (!p->batch_working && !p->active && !p->need_interrupt && p->timeout_state == TM_NONE)
    p->need_inactive = true;
}

/* Total count of listener and connections for PN_PROACTOR_INACTIVE */
static void add_active(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  ++p->active;
  uv_mutex_unlock(&p->lock);
}

static void remove_active_lh(pn_proactor_t *p) {
  assert(p->active > 0);
  if (--p->active == 0) {
    check_for_inactive(p);
  }
}

static void remove_active(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  remove_active_lh(p);
  uv_mutex_unlock(&p->lock);
}

/* Final close event for for a pconnection_t, disconnects from proactor */
static void on_close_pconnection_final(uv_handle_t *h) {
  /* Free resources associated with a pconnection_t.
     If the life of the pn_connection_t has been extended with reference counts
     we want the pconnection_t to have the same lifespan so calls to pn_connection_wake()
     will be valid, but no-ops.
  */
  pconnection_t *pc = (pconnection_t*)h->data;
  remove_active(pc->work.proactor);
  pconnection_free(pc);
}

static void uv_safe_close(uv_handle_t *h, uv_close_cb cb) {
  /* Only close if h has been initialized and is not already closing */
  if (h->type && !uv_is_closing(h)) {
    uv_close(h, cb);
  }
}

static void on_close_pconnection(uv_handle_t *h) {
  pconnection_t *pc = (pconnection_t*)h->data;
  /* Delay the free till the timer handle is also closed */
  uv_timer_stop(&pc->timer);
  uv_safe_close((uv_handle_t*)&pc->timer, on_close_pconnection_final);
}

static void listener_close_lh(pn_listener_t* l) {
  if (l->state < L_CLOSE) {
    l->state = L_CLOSE;
  }
  work_notify(&l->work);
}

static void on_close_lsocket(uv_handle_t *h) {
  lsocket_t* ls = (lsocket_t*)h->data;
  pn_listener_t *l = ls->parent;
  if (l) {
    /* Remove from list */
    lsocket_t **pp = &l->lsockets;
    for (; *pp != ls; pp = &(*pp)->next)
      ;
    *pp = ls->next;
    work_notify(&l->work);
  }
  free(ls);
}

/* Remember the first error code from a bad connect attempt.
 * This is not yet a full-blown error as we might succeed connecting
 * to a different address if there are several.
 */
static inline void pconnection_bad_connect(pconnection_t *pc, int err) {
  if (!pc->connected) {
    pc->connected = err;        /* Remember first connect error in case they all fail  */
  }
}

/* Set the error condition, but don't close the driver. */
static void pconnection_set_error(pconnection_t *pc, int err, const char* what) {
  pn_connection_driver_t *driver = &pc->driver;
  pn_connection_driver_bind(driver); /* Make sure we are bound so errors will be reported */
  pni_proactor_set_cond(pn_transport_condition(driver->transport),
                        what, pc->addr.host , pc->addr.port, uv_strerror(err));
}

/* Set the error condition and close the driver. */
static void pconnection_error(pconnection_t *pc, int err, const char* what) {
  assert(err);
  pconnection_bad_connect(pc, err);
  pconnection_set_error(pc, err, what);
  pn_connection_driver_close(&pc->driver);
}

static void listener_error_lh(pn_listener_t *l, int err, const char* what) {
  assert(err);
  if (!pn_condition_is_set(l->condition)) {
    pni_proactor_set_cond(l->condition, what, l->addr.host, l->addr.port, uv_strerror(err));
  }
  listener_close_lh(l);
}

static void listener_error(pn_listener_t *l, int err, const char* what) {
  uv_mutex_lock(&l->lock);
  listener_error_lh(l, err, what);
  uv_mutex_unlock(&l->lock);
}

static int pconnection_init(pconnection_t *pc) {
  int err = 0;
  err = uv_tcp_init(&pc->work.proactor->loop, &pc->tcp);
  if (!err) {
    pc->tcp.data = pc;
    pc->connect.data = pc;
    err = uv_timer_init(&pc->work.proactor->loop, &pc->timer);
    if (!err) {
      pc->timer.data = pc;
    } else {
      uv_close((uv_handle_t*)&pc->tcp, NULL);
    }
  }
  if (err) {
    pconnection_error(pc, err, "initialization");
  }
  return err;
}

static void try_connect(pconnection_t *pc);

static void on_connect_fail(uv_handle_t *handle) {
  pconnection_t *pc = (pconnection_t*)handle->data;
  /* Create a new TCP socket, the current one is closed */
  int err = uv_tcp_init(&pc->work.proactor->loop, &pc->tcp);
  if (err) {
    pc->connected = err;
    pc->addr.addrinfo = NULL; /* No point in trying anymore, we can't create a socket */
  } else {
    try_connect(pc);
  }
}

static void pconnection_addresses(pconnection_t *pc) {
  int len;
  len = sizeof(pc->local.ss);
  uv_tcp_getsockname(&pc->tcp, (struct sockaddr*)&pc->local.ss, &len);
  len = sizeof(pc->remote.ss);
  uv_tcp_getpeername(&pc->tcp, (struct sockaddr*)&pc->remote.ss, &len);
}

/* Outgoing connection */
static void on_connect(uv_connect_t *connect, int err) {
  pconnection_t *pc = (pconnection_t*)connect->data;
  if (!err) {
    pc->connected = 1;
    pconnection_addresses(pc);
    work_notify(&pc->work);
    uv_freeaddrinfo(pc->addr.getaddrinfo.addrinfo); /* Done with address info */
    pc->addr.getaddrinfo.addrinfo = NULL;
  } else {
    pconnection_bad_connect(pc, err);
    uv_safe_close((uv_handle_t*)&pc->tcp, on_connect_fail); /* Try the next addr if there is one */
  }
}

/* Incoming connection ready to be accepted */
static void on_connection(uv_stream_t* server, int err) {
  /* Unlike most on_* functions, this can be called by the leader thread when the listener
   * is ON_WORKER or ON_LEADER, because
   *
   * 1. There's no way to stop libuv from calling on_connection().
   * 2. There can be multiple lsockets per listener.
   *
   * Update the state of the listener and queue it for leader attention.
   */
  lsocket_t *ls = (lsocket_t*)server->data;
  pn_listener_t *l = ls->parent;
  if (err) {
    listener_error(l, err, "on incoming connection");
  } else {
    uv_mutex_lock(&l->lock);
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener_socket), ls, PN_LISTENER_ACCEPT);
    uv_mutex_unlock(&l->lock);
  }
  work_notify(&l->work);
}

/* Common address resolution for leader_listen and leader_connect */
static int leader_resolve(pn_proactor_t *p, addr_t *addr, bool listen) {
  struct addrinfo hints = { 0 };
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;
  if (listen) {
    hints.ai_flags |= AI_PASSIVE | AI_ALL;
  }
  int err = uv_getaddrinfo(&p->loop, &addr->getaddrinfo, NULL, addr->host, addr->port, &hints);
  addr->addrinfo = addr->getaddrinfo.addrinfo; /* Start with the first addrinfo */
  return err;
}

/* Try to connect to the current addrinfo. Called by leader and via callbacks for retry.*/
static void try_connect(pconnection_t *pc) {
  struct addrinfo *ai = pc->addr.addrinfo;
  if (!ai) {                    /* End of list, connect fails */
    uv_freeaddrinfo(pc->addr.getaddrinfo.addrinfo);
    pc->addr.getaddrinfo.addrinfo = NULL;
    pconnection_bad_connect(pc, UV_EAI_NODATA);
    pconnection_error(pc, pc->connected, "connecting to");
    work_notify(&pc->work);
  } else {
    pc->addr.addrinfo = ai->ai_next; /* Advance for next attempt */
    int err = uv_tcp_connect(&pc->connect, &pc->tcp, ai->ai_addr, on_connect);
    if (err) {
      pconnection_bad_connect(pc, err);
      uv_close((uv_handle_t*)&pc->tcp, on_connect_fail); /* Queue up next attempt */
    }
  }
}

static bool leader_connect(pconnection_t *pc) {
  int err = pconnection_init(pc);
  if (!err) err = leader_resolve(pc->work.proactor, &pc->addr, false);
  if (err) {
    pconnection_error(pc, err, "on connect resolving");
    return true;
  } else {
    try_connect(pc);
    return false;
  }
}

static int lsocket(pn_listener_t *l, struct addrinfo *ai) {
  lsocket_t *ls = (lsocket_t*)calloc(1, sizeof(lsocket_t));
  ls->type = T_LSOCKET;
  ls->tcp.data = ls;
  ls->parent = NULL;
  ls->next = NULL;
  int err = uv_tcp_init(&l->work.proactor->loop, &ls->tcp);
  if (err) {
    free(ls);                   /* Will never be closed */
  } else {
    if (l->dynamic_port) set_port(ai->ai_addr, l->dynamic_port);
    int flags = (ai->ai_family == AF_INET6) ? UV_TCP_IPV6ONLY : 0;
    err = uv_tcp_bind(&ls->tcp, ai->ai_addr, flags);
    if (!err) err = uv_listen((uv_stream_t*)&ls->tcp, l->backlog, on_connection);
    if (!err) {
      /* Get actual listening address */
      pn_netaddr_t *na = &l->addrs[l->addrs_len++];
      int len = sizeof(na->ss);
      uv_tcp_getsockname(&ls->tcp, (struct sockaddr*)(&na->ss), &len);
      if (na == l->addrs) {     /*  First socket, check for dynamic port bind */
        l->dynamic_port = check_dynamic_port(ai->ai_addr, pn_netaddr_sockaddr(na));
      } else {
        (na-1)->next = na;      /* Link into list */
      }
      /* Add to l->lsockets list */
      ls->parent = l;
      ls->next = l->lsockets;
      l->lsockets = ls;
    } else {
      uv_close((uv_handle_t*)&ls->tcp, on_close_lsocket); /* Freed by on_close_lsocket */
    }
  }
  return err;
}

#define ARRAY_LEN(A) (sizeof(A)/sizeof(*(A)))

/* Listen on all available addresses */
static void leader_listen_lh(pn_listener_t *l) {
  int err = leader_resolve(l->work.proactor, &l->addr, true);
  if (!err) {
    /* Allocate enough space for the pn_netaddr_t addresses */
    size_t len = 0;
    for (struct addrinfo *ai = l->addr.getaddrinfo.addrinfo; ai; ai = ai->ai_next) {
      ++len;
    }
    l->addrs = (pn_netaddr_t*)calloc(len, sizeof(pn_netaddr_t));

    /* Find the working addresses */
    for (struct addrinfo *ai = l->addr.getaddrinfo.addrinfo; ai; ai = ai->ai_next) {
      int err2 = lsocket(l, ai);
      if (err2) {
        err = err2;
      }
    }
    uv_freeaddrinfo(l->addr.getaddrinfo.addrinfo);
    l->addr.getaddrinfo.addrinfo = NULL;
    if (l->lsockets) {    /* Ignore errors if we got at least one good listening socket */
      err = 0;
    }
  }
  if (err) {
    listener_error_lh(l, err, "listening on");
  } else {
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_OPEN);
  }
}

void pn_listener_free(pn_listener_t *l) {
  if (l) {
    if (l->addrs) free(l->addrs);
    if (l->addr.getaddrinfo.addrinfo) { /* Interrupted after resolve */
      uv_freeaddrinfo(l->addr.getaddrinfo.addrinfo);
    }
    if (l->collector) pn_collector_free(l->collector);
    if (l->condition) pn_condition_free(l->condition);
    if (l->attachments) pn_free(l->attachments);
    while (l->lsockets) {
      lsocket_t *ls = l->lsockets;
      l->lsockets = ls->next;
      free(ls);
    }
    assert(!l->accept.front);
    uv_mutex_destroy(&l->lock);
    free(l);
  }
}

/* Process a listener, return true if it has events for a worker thread */
static bool leader_process_listener(pn_listener_t *l) {
  /* NOTE: l may be concurrently accessed by on_connection() */
  bool closed = false;
  uv_mutex_lock(&l->lock);

  /* Process accepted connections */
  for (pconnection_t *pc = pconnection_pop(&l->accept); pc; pc = pconnection_pop(&l->accept)) {
    int err = pconnection_init(pc);
    if (!err) err = uv_accept((uv_stream_t*)&pc->lsocket->tcp, (uv_stream_t*)&pc->tcp);
    if (!err) {
      pconnection_addresses(pc);
    } else {
      listener_error(l, err, "accepting from");
      pconnection_error(pc, err, "accepting from");
    }
    work_start(&pc->work);      /* Process events for the accepted/failed connection */
  }

  switch (l->state) {

   case L_UNINIT:
    l->state = L_LISTENING;
    leader_listen_lh(l);
    break;

   case L_LISTENING:
    break;

   case L_CLOSE:                /* Close requested, start closing lsockets */
    l->state = L_CLOSING;
    for (lsocket_t *ls = l->lsockets; ls; ls = ls->next) {
      uv_safe_close((uv_handle_t*)&ls->tcp, on_close_lsocket);
    }
    /* NOTE: Fall through in case we have 0 sockets - e.g. resolver error */

   case L_CLOSING:              /* Closing - can we send PN_LISTENER_CLOSE? */
    if (!l->lsockets) {
      l->state = L_CLOSED;
      pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_CLOSE);
    }
    break;

   case L_CLOSED:              /* Closed, has LISTENER_CLOSE has been processed? */
    if (!pn_collector_peek(l->collector)) {
      remove_active(l->work.proactor);
      closed = true;
    }
  }
  bool has_work = !closed && pn_collector_peek(l->collector);
  uv_mutex_unlock(&l->lock);

  if (closed) {
    pn_listener_free(l);
  }
  return has_work;
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
  work_notify(&pc->work);
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  pconnection_t *pc = (pconnection_t*)stream->data;
  if (nread > 0) {
    pn_connection_driver_read_done(&pc->driver, nread);
  } else if (nread < 0) {
    if (nread != UV_EOF) { /* hangup */
      pconnection_set_error(pc, nread, "on read from");
    }
    pn_connection_driver_close(&pc->driver);
  }
  work_notify(&pc->work);
}

static void on_write(uv_write_t* write, int err) {
  pconnection_t *pc = (pconnection_t*)write->data;
  size_t size = pc->writing;
  pc->writing = 0;
  if (err) {
    pconnection_set_error(pc, err, "on write to");
    pn_connection_driver_write_close(&pc->driver);
  } else if (!pn_connection_driver_write_closed(&pc->driver)) {
    pn_connection_driver_write_done(&pc->driver, size);
  }
  work_notify(&pc->work);
}

static void on_timeout(uv_timer_t *timer) {
  pn_proactor_t *p = (pn_proactor_t*)timer->data;
  uv_mutex_lock(&p->lock);
  if (p->timeout_state == TM_PENDING) { /* Only fire if still pending */
    p->timeout_state = TM_FIRED;
  }
  uv_stop(&p->loop);            /* UV does not always stop after on_timeout without this */
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
  pn_collector_put(p->collector, PN_CLASSCLASS(pn_proactor), p, t);
  p->batch_working = true;
  return &p->batch;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  uv_mutex_lock(&l->lock);
  pn_event_t *e = pn_collector_next(l->collector);
  uv_mutex_unlock(&l->lock);
  return pni_log_event(l, e);
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  assert(p->batch_working);
  return pni_log_event(p, pn_collector_next(p->collector));
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  return pn_connection_driver_next_event(&pc->driver);
}

/* Return the next event batch or NULL if no events are available */
static pn_event_batch_t *get_batch_lh(pn_proactor_t *p) {
  if (!p->batch_working) {       /* Can generate proactor events */
    if (p->need_inactive) {
      p->need_inactive = false;
      return proactor_batch_lh(p, PN_PROACTOR_INACTIVE);
    }
    if (p->need_interrupt) {
      p->need_interrupt = false;
      return proactor_batch_lh(p, PN_PROACTOR_INTERRUPT);
    }
    if (p->timeout_state == TM_FIRED) {
      p->timeout_state = TM_NONE;
      p->timeout_processed = true;
      return proactor_batch_lh(p, PN_PROACTOR_TIMEOUT);
    }
  }
  for (work_t *w = work_pop(&p->worker_q); w; w = work_pop(&p->worker_q)) {
    assert(w->working);
    switch (w->type) {
     case T_CONNECTION:
      return &((pconnection_t*)w)->batch;
     case T_LISTENER:
      return &((pn_listener_t*)w)->batch;
     default:
      break;
    }
  }
  return NULL;
}

/* Check wake state and generate WAKE event if needed */
static void check_wake(pconnection_t *pc) {
  uv_mutex_lock(&pc->lock);
  if (pc->wake == W_PENDING) {
    pn_connection_t *c = pc->driver.connection;
    pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
    pc->wake = W_NONE;
  }
  uv_mutex_unlock(&pc->lock);
}

/* Process a pconnection, return true if it has events for a worker thread */
static bool leader_process_pconnection(pconnection_t *pc) {
  /* Important to do the following steps in order */
  if (!pc->connected) {
    return leader_connect(pc);
  }
  if (pc->writing) {
    /* We can't do anything while a write request is pending */
    return false;
  }
  /* Must process INIT and BOUND events before we do any IO-related stuff  */
  if (pn_connection_driver_has_event(&pc->driver)) {
    return true;
  }
  if (pn_connection_driver_finished(&pc->driver)) {
    uv_mutex_lock(&pc->lock);
    pc->wake = W_CLOSED;        /* wake() is a no-op from now on */
    uv_mutex_unlock(&pc->lock);
      uv_safe_close((uv_handle_t*)&pc->tcp, on_close_pconnection);
  } else {
    /* Check for events that can be generated without blocking for IO */
    check_wake(pc);
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
      if (!err) {
        what = "write";
        if (wbuf.size > 0) {
          uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size);
          err = uv_write(&pc->write, (uv_stream_t*)&pc->tcp, &buf, 1, on_write);
          if (!err) {
            pc->writing = wbuf.size;
          }
        } else if (pn_connection_driver_write_closed(&pc->driver)) {
          uv_shutdown(&pc->shutdown, (uv_stream_t*)&pc->tcp, NULL);
        }
      }
      if (!err && rbuf.size > 0) {
        what = "read";
        err = uv_read_start((uv_stream_t*)&pc->tcp, alloc_read_buffer, on_read);
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
  if (pc->connected && !pc->writing) {           /* Can't detach while a write is pending */
    uv_read_stop((uv_stream_t*)&pc->tcp);
    uv_timer_stop(&pc->timer);
  }
}

static void on_proactor_disconnect(uv_handle_t* h, void* v) {
  if (h->type == UV_TCP) {
    switch (*(struct_type*)h->data) {
     case T_CONNECTION: {
       pconnection_t *pc = (pconnection_t*)h->data;
       pn_condition_t *cond = pc->work.proactor->disconnect_cond;
       if (cond) {
         pn_condition_copy(pn_transport_condition(pc->driver.transport), cond);
       }
       pn_connection_driver_close(&pc->driver);
       work_notify(&pc->work);
       break;
     }
     case T_LSOCKET: {
       pn_listener_t *l = ((lsocket_t*)h->data)->parent;
       if (l) {
         pn_condition_t *cond = l->work.proactor->disconnect_cond;
         if (cond) {
           pn_condition_copy(pn_listener_condition(l), cond);
         }
         pn_listener_close(l);
       }
       break;
     }
     default:
      break;
    }
  }
}

/* Process the leader_q and the UV loop, in the leader thread */
static pn_event_batch_t *leader_lead_lh(pn_proactor_t *p, uv_run_mode mode) {
  /* Set timeout timer if there was a request, let it count down while we process work */
  if (p->timeout_state == TM_REQUEST) {
    p->timeout_state = TM_PENDING;
    uv_timer_stop(&p->timer);
    uv_timer_start(&p->timer, on_timeout, p->timeout, 0);
  }
  /* If disconnect was requested, walk the socket list */
  if (p->disconnect) {
    p->disconnect = false;
    if (p->active) {
      uv_mutex_unlock(&p->lock);
      uv_walk(&p->loop, on_proactor_disconnect, NULL);
      uv_mutex_lock(&p->lock);
    } else {
      p->need_inactive = true;  /* Send INACTIVE right away, nothing to do. */
    }
  }
  pn_event_batch_t *batch = NULL;
  for (work_t *w = work_pop(&p->leader_q); w; w = work_pop(&p->leader_q)) {
    assert(!w->working);

    uv_mutex_unlock(&p->lock);  /* Unlock to process each item, may add more items to leader_q */
    bool has_work = false;
    switch (w->type) {
     case T_CONNECTION:
      has_work = leader_process_pconnection((pconnection_t*)w);
      break;
     case T_LISTENER:
      has_work = leader_process_listener((pn_listener_t*)w);
      break;
     default:
      break;
    }
    uv_mutex_lock(&p->lock);

    if (has_work && !w->working && w->next == work_unqueued) {
      if (w->type == T_CONNECTION) {
        pconnection_detach((pconnection_t*)w);
      }
      w->working = true;
      work_push(&p->worker_q, w);
    }
  }
  batch = get_batch_lh(p);      /* Check for work */
  if (!batch) {                 /* No work, run the UV loop */
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
  if (!batch) return;
  uv_mutex_lock(&p->lock);
  work_t *w = batch_work(batch);
  if (w) {
    assert(w->working);
    assert(w->next == work_unqueued);
    w->working = false;
    work_push(&p->leader_q, w);
  }
  pn_proactor_t *bp = batch_proactor(batch); /* Proactor events */
  if (bp == p) {
    p->batch_working = false;
    if (p->timeout_processed) {
      p->timeout_processed = false;
      check_for_inactive(p);
    }
  }
  uv_mutex_unlock(&p->lock);
  notify(p);
}

pn_listener_t *pn_event_listener(pn_event_t *e) {
  if (pn_event_class(e) == PN_CLASSCLASS(pn_listener)) {
    return (pn_listener_t*)pn_event_context(e);
  } else if (pn_event_class(e) == PN_CLASSCLASS(pn_listener_socket)) {
    return ((lsocket_t*)pn_event_context(e))->parent;
  } else {
    return NULL;
  }
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == PN_CLASSCLASS(pn_proactor)) {
    return (pn_proactor_t*)pn_event_context(e);
  }
  pn_listener_t *l = pn_event_listener(e);
  if (l) {
    return l->work.proactor;
  }
  pn_connection_t *c = pn_event_connection(e);
  if (c) {
    return pn_connection_proactor(pn_event_connection(e));
  }
  return NULL;
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  /* NOTE: pn_proactor_interrupt must be async-signal-safe so we cannot use
     locks to update shared proactor state here. Instead we use a dedicated
     uv_async, the on_interrupt() callback will set the interrupt flag in the
     safety of the leader thread.
   */
  uv_async_send(&p->interrupt);
}

void pn_proactor_disconnect(pn_proactor_t *p, pn_condition_t *cond) {
  uv_mutex_lock(&p->lock);
  if (!p->disconnect) {
    p->disconnect = true;
    if (cond) {
      pn_condition_copy(p->disconnect_cond, cond);
    } else {
      pn_condition_clear(p->disconnect_cond);
    }
    notify(p);
  }
  uv_mutex_unlock(&p->lock);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  uv_mutex_lock(&p->lock);
  p->timeout = t;
  // This timeout *replaces* any existing timeout
  p->timeout_state = TM_REQUEST;
  uv_mutex_unlock(&p->lock);
  notify(p);
}

void pn_proactor_cancel_timeout(pn_proactor_t *p) {
  uv_mutex_lock(&p->lock);
  if (p->timeout_state != TM_NONE) {
    p->timeout_state = TM_NONE;
    check_for_inactive(p);
    notify(p);
  }
  uv_mutex_unlock(&p->lock);
}

void pn_proactor_connect2(pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, const char *addr) {
  pconnection_t *pc = pconnection(p, c, t, false);
  assert(pc);                                  /* TODO aconway 2017-03-31: memory safety */
  add_active(p);
  pn_connection_open(pc->driver.connection);   /* Auto-open */
  parse_addr(&pc->addr, addr);
  work_start(&pc->work);
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog) {
  work_init(&l->work, p, T_LISTENER);
  parse_addr(&l->addr, addr);
  l->backlog = backlog;
  add_active(l->work.proactor);  /* Owned by proactor.  Track it for PN_PROACTOR_INACTIVE. */;
  work_start(&l->work);
}

static void on_proactor_free(uv_handle_t* h, void* v) {
  uv_safe_close(h, NULL);       /* Close the handle */
  if (h->type == UV_TCP) {      /* Put the corresponding work item on the leader_q for cleanup */
    work_t *w = NULL;
    switch (*(struct_type*)h->data) {
     case T_CONNECTION:
      w = (work_t*)h->data; break;
     case T_LSOCKET:
      w = &((lsocket_t*)h->data)->parent->work; break;
     default: break;
    }
    if (w && w->next == work_unqueued) {
      work_push(&w->proactor->leader_q, w); /* Save to be freed after all closed */
    }
  }
}

static void work_free(work_t *w) {
  switch (w->type) {
   case T_CONNECTION: pconnection_free((pconnection_t*)w); break;
   case T_LISTENER: pn_listener_free((pn_listener_t*)w); break;
   default: break;
  }
}

pn_proactor_t *pn_proactor() {
  uv_once(&global_init_once, global_init_fn);
  pn_proactor_t *p = (pn_proactor_t*)calloc(1, sizeof(pn_proactor_t));
  p->collector = pn_collector();
  if (!p->collector) {
    free(p);
    return NULL;
  }
  p->batch.next_event = &proactor_batch_next;
  uv_loop_init(&p->loop);
  uv_mutex_init(&p->lock);
  uv_cond_init(&p->cond);
  uv_async_init(&p->loop, &p->notify, NULL);
  uv_async_init(&p->loop, &p->interrupt, on_interrupt);
  p->interrupt.data = p;
  uv_timer_init(&p->loop, &p->timer);
  p->timer.data = p;
  p->disconnect_cond = pn_condition();
  return p;
}

void pn_proactor_free(pn_proactor_t *p) {
  /* Close all open handles */
  uv_walk(&p->loop, on_proactor_free, NULL);
  while (uv_loop_alive(&p->loop)) {
    uv_run(&p->loop, UV_RUN_DEFAULT); /* Finish closing the proactor handles */
  }
  /* Free all work items */
  for (work_t *w = work_pop(&p->leader_q); w; w = work_pop(&p->leader_q)) {
    work_free(w);
  }
  for (work_t *w = work_pop(&p->worker_q); w; w = work_pop(&p->worker_q)) {
    work_free(w);
  }
  uv_loop_close(&p->loop);
  uv_mutex_destroy(&p->lock);
  uv_cond_destroy(&p->cond);
  pn_collector_free(p->collector);
  pn_condition_free(p->disconnect_cond);
  free(p);
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->work.proactor : NULL;
}

void pn_connection_wake(pn_connection_t* c) {
  /* May be called from any thread */
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    bool notify = false;
    uv_mutex_lock(&pc->lock);
    if (pc->wake == W_NONE) {
      pc->wake = W_PENDING;
      notify = true;
    }
    uv_mutex_unlock(&pc->lock);
    if (notify) {
      work_notify(&pc->work);
    }
  }
}

void pn_proactor_release_connection(pn_connection_t *c) {
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    set_pconnection(c, NULL);
    pn_connection_driver_release_connection(&pc->driver);
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
    uv_mutex_init(&l->lock);
  }
  return l;
}

void pn_listener_close(pn_listener_t* l) {
  /* May be called from any thread */
  uv_mutex_lock(&l->lock);
  listener_close_lh(l);
  uv_mutex_unlock(&l->lock);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->work.proactor : NULL;
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

void pn_listener_accept2(pn_listener_t *l, pn_connection_t *c, pn_transport_t *t) {
  pconnection_t *pc = pconnection(l->work.proactor, c, t, true);
  assert(pc);
  add_active(l->work.proactor);
  uv_mutex_lock(&l->lock);
  /* Get the socket from the accept event that we are processing */
  pn_event_t *e = pn_collector_prev(l->collector);
  assert(pn_event_type(e) == PN_LISTENER_ACCEPT);
  assert(pn_event_listener(e) == l);
  pc->lsocket = (lsocket_t*)pn_event_context(e);
  pc->connected = 1;            /* Don't need to connect() */
  pconnection_push(&l->accept, pc);
  uv_mutex_unlock(&l->lock);
  work_notify(&l->work);
}

const pn_netaddr_t *pn_transport_local_addr(pn_transport_t *t) {
  pconnection_t *pc = get_pconnection(pn_transport_connection(t));
  return pc? &pc->local : NULL;
}

const pn_netaddr_t *pn_transport_remote_addr(pn_transport_t *t) {
  pconnection_t *pc = get_pconnection(pn_transport_connection(t));
  return pc ? &pc->remote : NULL;
}

const pn_netaddr_t *pn_listener_addr(pn_listener_t *l) {
  return l->addrs ? &l->addrs[0] : NULL;
}

pn_millis_t pn_proactor_now(void) {
  return (pn_millis_t) pn_proactor_now_64();
}

int64_t pn_proactor_now_64(void) {
  return uv_hrtime() / 1000000; // uv_hrtime returns time in nanoseconds
}

// Empty stubs for raw connection code
pn_raw_connection_t *pn_raw_connection(void) { return NULL; }
void pn_proactor_raw_connect(pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr) {}
void pn_listener_raw_accept(pn_listener_t *l, pn_raw_connection_t *rc) {}
void pn_raw_connection_wake(pn_raw_connection_t *conn) {}
void pn_raw_connection_close(pn_raw_connection_t *conn) {}
void pn_raw_connection_read_close(pn_raw_connection_t *conn) {}
void pn_raw_connection_write_close(pn_raw_connection_t *conn) {}
const struct pn_netaddr_t *pn_raw_connection_local_addr(pn_raw_connection_t *connection) { return NULL; }
const struct pn_netaddr_t *pn_raw_connection_remote_addr(pn_raw_connection_t *connection) { return NULL; }
