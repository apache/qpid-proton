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
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/listener.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/eventfd.h>
#include <limits.h>

// TODO: replace timerfd per connection with global lightweight timer mechanism.
// logging in general, listener events in particular
// SIGPIPE?
// Can some of the mutexes be spinlocks (any benefit over adaptive pthread mutex)?
//   Maybe futex is even better?
// See other "TODO" in code.
//
// Consider case of large number of wakes: proactor_do_epoll() could start by
// looking for pending wakes before a kernel call to epoll_wait(), or there
// could be several eventfds with random assignment of wakeables.


// ========================================================================
// First define a proactor mutex (pmutex) and timer mechanism (ptimer) to taste.
// ========================================================================

// In general all locks to be held singly and shortly (possibly as spin locks).
// Exception: psockets+proactor for pn_proactor_disconnect (convention: acquire
// psocket first to avoid deadlock).  TODO: revisit the exception and its
// awkwardness in the code (additional mutex? different type?).

typedef pthread_mutex_t pmutex;
static void pmutex_init(pthread_mutex_t *pm){
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
  if (pthread_mutex_init(pm, &attr)) {
    perror("pthread failure");
    abort();
  }
}

static void pmutex_finalize(pthread_mutex_t *m) { pthread_mutex_destroy(m); }
static inline void lock(pmutex *m) { pthread_mutex_lock(m); }
static inline void unlock(pmutex *m) { pthread_mutex_unlock(m); }

typedef struct psocket_t psocket_t;
typedef enum {
  WAKE,   /* see if any work to do in proactor/psocket context */
  PCONNECTION_IO,
  PCONNECTION_TIMER,
  LISTENER_IO,
  PROACTOR_TIMER } epoll_type_t;

// Data to use with epoll.
typedef struct epoll_extended_t {
  psocket_t *psocket;  // pconnection, listener, or NULL -> proactor
  int fd;
  epoll_type_t type;   // io/timer/wakeup
  uint32_t wanted;     // events to poll for
} epoll_extended_t;

/*
 * This timerfd logic assumes EPOLLONESHOT and there never being two
 * active timeout callbacks.  There can be multiple unclaimed expiries
 * processed in a single callback.
 */

typedef struct ptimer_t {
  pmutex mutex;
  int timerfd;
  epoll_extended_t epoll_io;
  int pending_count;
  int skip_count;
} ptimer_t;

static bool ptimer_init(ptimer_t *pt, psocket_t *ps) {
  pt->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (pt->timerfd < 0) return false;
  pmutex_init(&pt->mutex);
  pt->pending_count = 0;
  pt->skip_count = 0;
  epoll_type_t type = ps ? PCONNECTION_TIMER : PROACTOR_TIMER;
  pt->epoll_io.psocket = ps;
  pt->epoll_io.fd = pt->timerfd;
  pt->epoll_io.type = type;
  pt->epoll_io.wanted = EPOLLIN;
  return true;
}

static void ptimer_set(ptimer_t *pt, uint64_t t_millis) {
  // t_millis == 0 -> cancel
  lock(&pt->mutex);
  if (t_millis == 0 && pt->pending_count == 0) {
    unlock(&pt->mutex);
    return;  // nothing to cancel
  }
  struct itimerspec newt, oldt;
  memset(&newt, 0, sizeof(newt));
  newt.it_value.tv_sec = t_millis / 1000;
  newt.it_value.tv_nsec = (t_millis % 1000) * 1000000;

  timerfd_settime(pt->timerfd, 0, &newt, &oldt);
  if (oldt.it_value.tv_sec || oldt.it_value.tv_nsec) {
    // old value cancelled
    assert (pt->pending_count > 0);
    pt->pending_count--;
  } else if (pt->pending_count) {
    // cancel instance waiting on this lock
    pt->skip_count++;
  }
  if (t_millis)
    pt->pending_count++;
  assert(pt->pending_count >= 0);
  unlock(&pt->mutex);
}

// Callback bookkeeping. Return number of uncancelled expiry events.
static int ptimer_callback(ptimer_t *pt) {
  lock(&pt->mutex);
  uint64_t u_exp_count;
  ssize_t l = read(pt->timerfd, &u_exp_count, sizeof(uint64_t));
  (void)l; /* Silence compiler complaints in release build */
  assert(l == sizeof(uint64_t));
  assert(u_exp_count < INT_MAX);  // or test and log it?
  int exp_count = (int) u_exp_count;
  assert(exp_count >= pt->skip_count);
  assert(exp_count <= pt->pending_count);
  exp_count -= pt->skip_count;
  pt->skip_count = 0;
  pt->pending_count -= exp_count;
  unlock(&pt->mutex);
  return (int) exp_count;
}

static void ptimer_finalize(ptimer_t *pt) {
  if (pt->timerfd >= 0) close(pt->timerfd);
  pmutex_finalize(&pt->mutex);
}

pn_timestamp_t pn_i_now2(void)
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return ((pn_timestamp_t)now.tv_sec) * 1000 + (now.tv_nsec / 1000000);
}

// ========================================================================
// Proactor common code
// ========================================================================

const char *COND_NAME = "proactor";
const char *AMQP_PORT = "5672";
const char *AMQP_PORT_NAME = "amqp";
const char *AMQPS_PORT = "5671";
const char *AMQPS_PORT_NAME = "amqps";

PN_HANDLE(PN_PROACTOR)

// The number of times a connection event batch may be replenished for
// a thread between calls to wait().
// TODO: consider some instrumentation to determine an optimal number
// or perhaps switch to cpu time based limit.
#define HOG_MAX 3

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   Class definitions are for identification as pn_event_t context only.
*/
PN_STRUCT_CLASSDEF(pn_proactor, CID_pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener, CID_pn_listener)

static bool start_polling(epoll_extended_t *ee, int epollfd) {
  struct epoll_event ev;
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  return (epoll_ctl(epollfd, EPOLL_CTL_ADD, ee->fd, &ev) == 0);
}

static void stop_polling(epoll_extended_t *ee, int epollfd) {
  // TODO: check for error, return bool or just log?
  if (ee->fd == -1)
    return;
  struct epoll_event ev;
  ev.data.ptr = ee;
  ev.events = 0;
  epoll_ctl(epollfd, EPOLL_CTL_DEL, ee->fd, &ev);  // TODO: check for error
  ee->fd = -1;
}

/*
 * The proactor maintains a number of serialization contexts: each
 * connection, each listener, the proactor itself.  The serialization
 * is presented to the application via each associated event batch.
 *
 * Multiple threads can be trying to do work on a single context
 * (i.e. socket IO is ready and wakeup at same time). Mutexes are used
 * to manage contention.  Some vars are only ever touched by one
 * "working" thread and are accessed without holding the mutex.
 *
 * Currently internal wakeups (via wake()/wake_notify()) are used to
 * force a context to check if it has work to do.  To minimize trips
 * through the kernel, wake() is a no-op if the context has a working
 * thread.  Conversely, a thread must never stop working without
 * checking if it has newly arrived work.
 *
 * External wake operations, like pn_connection_wake() and
 * pn_proactor_interrupt(), are built on top of the internal wake
 * mechanism.  The former coalesces multiple wakes until event
 * delivery, the latter does not.  The WAKEABLE implementation can be
 * modeled on whichever is more suited.
 */
typedef enum {
  PROACTOR,
  PCONNECTION,
  LISTENER,
  WAKEABLE } pcontext_type_t;

typedef struct pcontext_t {
  pmutex mutex;
  pn_proactor_t *proactor;  /* Immutable */
  void *owner;              /* Instance governed by the context */
  pcontext_type_t type;
  bool working;
  int wake_ops;             // unprocessed eventfd wake callback (convert to bool?)
  struct pcontext_t *next;         // wake list, guarded by proactor eventfd_mutex
} pcontext_t;

static void pcontext_init(pcontext_t *ctx, pcontext_type_t t, pn_proactor_t *p, void *o) {
  pmutex_init(&ctx->mutex);
  ctx->proactor = p;
  ctx->owner = o;
  ctx->type = t;
  ctx->working = false;
  ctx->wake_ops = 0;
  ctx->next = NULL;
}

static void pcontext_finalize(pcontext_t* ctx) {
  pmutex_finalize(&ctx->mutex);
}

/* common to connection and listener */
typedef struct psocket_t {
  pn_proactor_t *proactor;
  // Next 4 are protected by the proactor mutex
  struct psocket_t* next;   /* Protected by proactor.mutex */
  struct psocket_t* prev;   /* Protected by proactor.mutex */
  bool disconnecting;       /* pn_proactor_disconnect */
  int disconnect_ops;       /* ops remaining before disconnect complete */
  // Remaining protected by the pconnection/listener mutex
  int sockfd;
  epoll_extended_t epoll_io;
  bool is_conn;
  bool closing;
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
} psocket_t;

struct pn_proactor_t {
  pcontext_t context;
  int epollfd;
  ptimer_t timer;
  pn_collector_t *collector;
  psocket_t *psockets;          /* track in-use psockets for PN_PROACTOR_INACTIVE and final cleanup */
  epoll_extended_t epoll_wake;
  pn_event_t *cached_event;
  pn_event_batch_t batch;
  size_t interrupts;            /* total pending interrupts */
  size_t deferred_interrupts;   /* interrupts for current batch */
  size_t disconnects_pending;   /* unfinished proactor disconnects*/
  bool inactive;
  bool timer_expired;
  bool timer_cancelled;
  bool timer_armed;
  bool shutting_down;
  // wake subsystem
  int eventfd;
  pmutex eventfd_mutex;
  bool wakes_in_progress;
  pcontext_t *wake_list_first;
  pcontext_t *wake_list_last;
};

static void rearm(pn_proactor_t *p, epoll_extended_t *ee);

/*
 * Wake strategy with eventfd.
 *  - wakees can be in the list only once
 *  - wakers only write() if wakes_in_progress is false
 *  - wakees only read() if about to set wakes_in_progress to false
 * When multiple wakes are pending, the kernel cost is a single rearm().
 * Otherwise it is the trio of write/read/rearm.
 * Only the writes and reads need to be carefully ordered.
 *
 * Multiple eventfds could be used and shared amongst the pcontext_t's.
 */

// part1: call with ctx->owner lock held, return true if notify required by caller
static bool wake(pcontext_t *ctx) {
  bool notify = false;
  if (!ctx->wake_ops) {
    if (!ctx->working) {
      ctx->wake_ops++;
      pn_proactor_t *p = ctx->proactor;
      lock(&p->eventfd_mutex);
      if (!p->wake_list_first) {
        p->wake_list_first = p->wake_list_last = ctx;
      } else {
        p->wake_list_last->next = ctx;
        p->wake_list_last = ctx;
      }
      if (!p->wakes_in_progress) {
        // force a wakeup via the eventfd
        p->wakes_in_progress = true;
        notify = true;
      }
      unlock(&p->eventfd_mutex);
    }
  }
  return notify;
}

// part2: make OS call without lock held
static inline void wake_notify(pcontext_t *ctx) {
  uint64_t increment = 1;
  write(ctx->proactor->eventfd, &increment, sizeof(uint64_t));  // TODO: check for error
}

// call with no locks
static pcontext_t *wake_pop_front(pn_proactor_t *p) {
  pcontext_t *ctx = NULL;
  lock(&p->eventfd_mutex);
  assert(p->wakes_in_progress);
  if (p->wake_list_first) {
    ctx = p->wake_list_first;
    p->wake_list_first = ctx->next;
    if (!p->wake_list_first) p->wake_list_last = NULL;
    ctx->next = NULL;

    if (!p->wake_list_first) {
      /* Reset the eventfd until a future write.
       * Can the read system call be made without holding the lock?
       * Note that if the reads/writes happen out of order, the wake
       * mechanism will hang. */
      uint64_t ignored;
      read(p->eventfd, &ignored, sizeof(uint64_t)); // TODO: check for error
      p->wakes_in_progress = false;
    }
  }
  unlock(&p->eventfd_mutex);
  rearm(p, &p->epoll_wake);
  return ctx;
}

// call with owner lock held, once for each pop from the wake list
static inline void wake_done(pcontext_t *ctx) {
  assert(ctx->wake_ops > 0);
  ctx->wake_ops--;
}


static void psocket_init(psocket_t* ps, pn_proactor_t* p, bool is_conn, const char *host, const char *port) {
  ps->epoll_io.psocket = ps;
  ps->epoll_io.fd = -1;
  ps->epoll_io.type = is_conn ? PCONNECTION_IO : LISTENER_IO;
  ps->epoll_io.wanted = 0;
  ps->proactor = p;
  ps->next = NULL;
  ps->prev = NULL;
  ps->disconnecting = false;
  ps->disconnect_ops = 0;
  ps->is_conn = is_conn;
  ps->closing = false;
  ps->sockfd = -1;

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
  pcontext_t context;
  uint32_t new_events;
  int wake_count;
  bool server;                /* accept, not connect */
  bool tick_pending;
  bool timer_armed;
  ptimer_t timer;  // TODO: review one timerfd per connectoin
  // Following values only changed by (sole) working context:
  uint32_t current_arm;  // active epoll io events
  bool read_blocked;
  bool write_blocked;
  bool read_closed;
  bool write_closed;
  bool disconnected;
  int hog_count; // thread hogging limiter
  pn_event_t *cached_event;
  pn_event_batch_t batch;
  pn_connection_driver_t driver;
} pconnection_t;

struct pn_listener_t {
  psocket_t psocket;
  pcontext_t context;
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_t *cached_event;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *listener_context;
  size_t backlog;
  int available_accepts;
  int pending_accepts;
  bool close_dispatched;
  bool armed;
};


static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool timeout, bool topup);
static void listener_begin_close(pn_listener_t* l);
static void proactor_add(psocket_t *ps);
static bool proactor_remove(psocket_t *ps);

static inline pconnection_t *as_pconnection(psocket_t* ps) {
  return ps->is_conn ? (pconnection_t*)ps : NULL;
}

static inline pn_listener_t *as_listener(psocket_t* ps) {
  return ps->is_conn ? NULL: (pn_listener_t*)ps;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch);
static pn_event_t *proactor_batch_next(pn_event_batch_t *batch);
static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch);

static inline pn_proactor_t *batch_proactor(pn_event_batch_t *batch) {
  return (batch->next_event == proactor_batch_next) ?
    (pn_proactor_t*)((char*)batch - offsetof(pn_proactor_t, batch)) : NULL;
}

static inline pn_listener_t *batch_listener(pn_event_batch_t *batch) {
  return (batch->next_event == listener_batch_next) ?
    (pn_listener_t*)((char*)batch - offsetof(pn_listener_t, batch)) : NULL;
}

static inline pconnection_t *batch_pconnection(pn_event_batch_t *batch) {
  return (batch->next_event == pconnection_batch_next) ?
    (pconnection_t*)((char*)batch - offsetof(pconnection_t, batch)) : NULL;
}

static inline bool pconnection_has_event(pconnection_t *pc) {
  return (pc->cached_event || (pc->cached_event = pn_connection_driver_next_event(&pc->driver)));
}

static inline bool listener_has_event(pn_listener_t *l) {
  return (l->cached_event || (l->cached_event = pn_collector_next(l->collector)));
}

static inline bool proactor_has_event(pn_proactor_t *p) {
  return (p->cached_event || (p->cached_event = pn_collector_next(p->collector)));
}

static pn_event_t *log_event(void* p, pn_event_t *e) {
  if (e) {
    pn_logf("[%p]:(%s)", (void*)p, pn_event_type_name(pn_event_type(e)));
  }
  return e;
}

static void psocket_error(psocket_t *ps, int err, const char* what) {
  if (ps->is_conn) {
    pn_connection_driver_t *driver = &as_pconnection(ps)->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pn_connection_driver_errorf(driver, COND_NAME, "%s %s:%s: %s",
                                what, fixstr(ps->host), fixstr(ps->port),
                                strerror(err));
    pn_connection_driver_close(driver);
  } else {
    pn_listener_t *l = as_listener(ps);
    pn_condition_format(l->condition, COND_NAME, "%s %s:%s: %s",
                        what, fixstr(ps->host), fixstr(ps->port),
                        strerror(err));
    listener_begin_close(l);
  }
}

static void rearm(pn_proactor_t *p, epoll_extended_t *ee) {
  struct epoll_event ev;
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  epoll_ctl(p->epollfd, EPOLL_CTL_MOD, ee->fd, &ev);  // TODO: check for error
}

// ========================================================================
// pconnection
// ========================================================================

/* Make a pn_class for pconnection_t since it is attached to a pn_connection_t record */
#define CID_pconnection CID_pn_object
#define pconnection_inspect NULL
#define pconnection_initialize NULL
#define pconnection_hashcode NULL
#define pconnection_compare NULL

static void pconnection_finalize(void *vp_pconnection) {
  pconnection_t *pc = (pconnection_t*)vp_pconnection;
  pcontext_finalize(&pc->context);
}


static const pn_class_t pconnection_class = PN_CLASS(pconnection);


static void pconnection_tick(pconnection_t *pc);

static pconnection_t *new_pconnection_t(pn_proactor_t *p, pn_connection_t *c, bool server, const char *host, const char *port) {
  pconnection_t *pc = (pconnection_t*) pn_class_new(&pconnection_class, sizeof(pconnection_t));
  if (!pc) return NULL;
  if (pn_connection_driver_init(&pc->driver, c, NULL) != 0) {
    return NULL;
  }
  if (!ptimer_init(&pc->timer, &pc->psocket)) {
    perror("timer setup failure");
    abort();
  }
  pcontext_init(&pc->context, PCONNECTION, p, pc);
  psocket_init(&pc->psocket, p,  true, host, port);
  pc->new_events = 0;
  pc->wake_count = 0;
  pc->tick_pending = false;
  pc->timer_armed = false;

  pc->current_arm = 0;
  pc->read_blocked = true;
  pc->write_blocked = true;
  pc->read_closed = true;
  pc->write_closed = true;
  pc->disconnected = false;
  pc->hog_count = 0;;
  pc->cached_event = NULL;
  pc->batch.next_event = pconnection_batch_next;

  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }
  pn_record_t *r = pn_connection_attachments(pc->driver.connection);
  pn_record_def(r, PN_PROACTOR, &pconnection_class);
  pn_record_set(r, PN_PROACTOR, pc);
  pn_decref(pc);                /* Will be deleted when the connection is */
  return pc;
}

// Call with lock held and closing == true (i.e. pn_connection_driver_finished() == true), timer cancelled.
// Return true when all possible outstanding epoll events associated with this pconnection have been processed.
static inline bool pconnection_is_final(pconnection_t *pc) {
  return !pc->current_arm && !pc->timer.pending_count && !pc->context.wake_ops;
}

static void pconnection_final_free(pconnection_t *pc) {
  pn_incref(pc);                /* Make sure we don't do a circular free */
  pn_connection_driver_destroy(&pc->driver);
  pn_decref(pc);
  /* Now pc is freed iff the connection is, otherwise remains till the pn_connection_t is freed. */
}

// call without lock, but only if pconnection_is_final() is true
static void pconnection_cleanup(pconnection_t *pc) {
  if (pc->psocket.sockfd != -1)
    close(pc->psocket.sockfd);
  stop_polling(&pc->timer.epoll_io, pc->psocket.proactor->epollfd);
  ptimer_finalize(&pc->timer);
  lock(&pc->context.mutex);
  bool can_free = proactor_remove(&pc->psocket);
  unlock(&pc->context.mutex);
  if (can_free)
    pconnection_final_free(pc);
  // else proactor_disconnect logic owns psocket and its final free
}

// Call with lock held or from forced_shutdown
void pconnection_begin_close(pconnection_t *pc) {
  if (!pc->psocket.closing) {
    pc->psocket.closing = true;
    pc->read_closed = pc->write_closed = true;
    stop_polling(&pc->psocket.epoll_io, pc->psocket.proactor->epollfd);
    pc->current_arm = 0;
    pn_connection_driver_close(&pc->driver);
    ptimer_set(&pc->timer, 0);
  }
}

static void pconnection_forced_shutdown(pconnection_t *pc) {
  // Called by proactor_free, no competing threads, no epoll activity.
  pconnection_begin_close(pc);
  // pconnection_process will never be called again.  Zero everything.
  pc->timer.pending_count = 0;
  pc->context.wake_ops = 0;
  pn_connection_t *c = pc->driver.connection;
  pn_collector_t *col = pn_connection_collector(c);
  if (pc->cached_event != NULL) {
    pn_collector_pop(col);
    pc->cached_event = NULL;
  }
  pn_collector_release(col);
  assert(pconnection_is_final(pc));
  pconnection_cleanup(pc);
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  pn_event_t *e = NULL;
  if (pconnection_has_event(pc))
    e = pc->cached_event;
  else if (pc->hog_count < HOG_MAX) {
    pconnection_process(pc, 0, false, true);  // top up
    if (pconnection_has_event(pc))
      e = pc->cached_event;
  }
  pc->cached_event = NULL;
  return e;
}

/* Call only from working context (no competitor for pc->current_arm or
   connection driver).  If true returned, caller must do
   pconnection_rearm().

   Never rearm(0 | EPOLLONESHOT), since this really means
   rearm(EPOLLHUP | EPOLLERR | EPOLLONESHOT) and leaves doubt that the
   EPOLL_CTL_DEL can prevent a parallel HUP/ERR error notification during
   close/shutdown.  Let read()/write() return 0 or -1 to trigger cleanup logic.
 */
static bool pconnection_rearm_check(pconnection_t *pc) {
  if (pc->read_closed && pc->write_closed) return false;

  uint32_t wanted_now = (pc->read_blocked && !pc->read_closed) ? EPOLLIN : 0;
  if (!pc->write_closed) {
    if (pc->write_blocked)
      wanted_now |= EPOLLOUT;
    else {
      pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
      if (wbuf.size > 0)
        wanted_now |= EPOLLOUT;
    }
  }
  if (!wanted_now || pc->current_arm == wanted_now) return false;

  pc->psocket.epoll_io.wanted = wanted_now;
  pc->current_arm = wanted_now;
  return true;
}

/* Call without lock */
static inline void pconnection_rearm(pconnection_t *pc) {
  rearm(pc->psocket.proactor, &pc->psocket.epoll_io);
}

static inline bool pconnection_work_pending(pconnection_t *pc) {
  if (pc->new_events || pc->wake_count || pc->tick_pending)
    return true;
  if (!pc->read_blocked && !pc->read_closed)
    return true;
  pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
  return (wbuf.size > 0 && !pc->write_blocked);
}

static void pconnection_done(pconnection_t *pc) {
  bool notify = false;
  lock(&pc->context.mutex);
  pc->context.working = false;  // So we can wake() ourself if necessary.  We remain the defacto
                                // working context while the lock is held.
  pc->hog_count = 0;
  if (pconnection_has_event(pc) || pconnection_work_pending(pc)) {
    notify = wake(&pc->context);
  } else if (!pc->read_closed && pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->context.mutex);
      pconnection_cleanup(pc);
      return;
    }
  }
  bool rearm = pconnection_rearm_check(pc);
  unlock(&pc->context.mutex);
  if (rearm) pconnection_rearm(pc);
  if (notify) wake_notify(&pc->context);
}

static pconnection_t *get_pconnection(pn_connection_t* c) {
  if (!c) return NULL;
  pn_record_t *r = pn_connection_attachments(c);
  return (pconnection_t*) pn_record_get(r, PN_PROACTOR);
}

// Return true unless error
static bool pconnection_write(pconnection_t *pc, pn_bytes_t wbuf) {
  ssize_t n = write(pc->psocket.sockfd, wbuf.start, wbuf.size);
  if (n > 0) {
    pn_connection_driver_write_done(&pc->driver, n);
    if ((size_t) n < wbuf.size) pc->write_blocked = true;
  } else if (errno == EWOULDBLOCK) {
    pc->write_blocked = true;
  } else if (!(errno == EAGAIN || errno == EINTR)) {
    return false;
  }
  return true;
}

/*
 * May be called concurrently from multiple threads:
 *   pn_event_batch_t loop (topup is true)
 *   timer (timeout is true)
 *   socket io (events != 0)
 *   one or more wake()
 * Only one thread becomes (or always was) the working thread.
 */
static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool timeout, bool topup) {
  bool inbound_wake = !(events | timeout | topup);
  bool timer_unarmed = false;
  bool timer_fired = false;
  bool waking = false;
  bool tick_required = false;

  // Don't touch data exclusive to working thread (yet).

  if (timeout) {
    timer_unarmed = true;
    timer_fired = (ptimer_callback(&pc->timer) != 0);
  }
  lock(&pc->context.mutex);

  if (events) {
    pc->new_events = events;
    events = 0;
  }
  else if (timer_fired) {
    pc->tick_pending = true;
    timer_fired = false;
  }
  else if (inbound_wake) {
    wake_done(&pc->context);
    inbound_wake = false;
  }

  if (timer_unarmed)
    pc->timer_armed = false;

  if (topup) {
    // Only called by the batch owner.  Does not loop, just "tops up"
    // once.  May be back depending on hog_count.
    assert(pc->context.working);
  }
  else {
    if (pc->context.working) {
      // Another thread is the working context.
      unlock(&pc->context.mutex);
      return NULL;
    }
    pc->context.working = true;
  }

  // Confirmed as working thread.  Review state and unlock ASAP.

  if (pc->psocket.closing && pconnection_is_final(pc)) {
    unlock(&pc->context.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

 retry:

  if (pconnection_has_event(pc)) {
    unlock(&pc->context.mutex);
    return &pc->batch;
  }

  if (pc->wake_count) {
    waking = true;
    pc->wake_count = 0;
  }
  if (pc->tick_pending) {
    pc->tick_pending = false;
    if (!(pc->read_closed && pc->write_closed))
      tick_required = true;
  }

  if (pc->new_events) {
    if ((pc->new_events & (EPOLLHUP | EPOLLERR)) && !pc->read_closed && !pc->write_closed)
      pc->disconnected = true;
    if (pc->new_events & EPOLLOUT)
      pc->write_blocked = false;
    if (pc->new_events & EPOLLIN)
      pc->read_blocked = false;
    pc->current_arm = 0;
    pc->new_events = 0;
  }
  bool unarmed = (pc->current_arm == 0);
  if (!pc->timer_armed) {
    pc->timer_armed = true;  // about to rearm outside the lock
    timer_unarmed = true;    // so we remember
  }

  unlock(&pc->context.mutex);
  pc->hog_count++; // working context doing work

  if (timer_unarmed) {
    rearm(pc->psocket.proactor, &pc->timer.epoll_io);
    timer_unarmed = false;
  }
  if (waking) {
    pn_connection_t *c = pc->driver.connection;
    pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
    waking = false;
  }

  // read... tick... write
  // perhaps should be: write_if_recent_EPOLLOUT... read... tick... write

  if (!pc->read_closed) {
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    if (rbuf.size == 0) {
      if (pn_connection_driver_read_closed(&pc->driver))
        pc->read_closed = true;
    }
    else if (!pc->read_blocked) {
      ssize_t n = read(pc->psocket.sockfd, rbuf.start, rbuf.size);

      if (n > 0) {
        pn_connection_driver_read_done(&pc->driver, n);
        pconnection_tick(pc);         /* check for tick changes. */
        tick_required = false;
        if (pn_connection_driver_read_closed(&pc->driver)) {
          // No more blocks on read in case peer doesn't send shutdown.
          pc->read_closed = true;
        }
        else if ((size_t) n < rbuf.size)
          pc->read_blocked = true;
      }
      else if (n == 0) {
        pn_connection_driver_read_close(&pc->driver);
        pc->read_closed = true;
      }
      else if (errno == EWOULDBLOCK)
        pc->read_blocked = true;
      else if (!(errno == EAGAIN || errno == EINTR)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "Disconnected" : "on read from");
        pc->read_closed = pc->write_closed = true;
      }
    }
  }

  if (tick_required) {
    pconnection_tick(pc);         /* check for tick changes. */
    tick_required = false;
  }

  while (!pc->write_blocked && !pc->write_closed) {
    pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
    if (wbuf.size > 0) {
      if (!pconnection_write(pc, wbuf)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "disconnected" : "on write to");
        pc->read_closed = pc->write_closed = true;
      }
    }
    else {
      if (pn_connection_driver_write_closed(&pc->driver)) {
        shutdown(pc->psocket.sockfd, SHUT_WR);
        pc->write_closed = true;
        pc->write_blocked = true;
      }
      else
        break;  /* nothing to write until next read/wake/timeout */
    }
  }

  if (topup) {
    // If there was anything new to topup, we have it by now.
    if (unarmed && pconnection_rearm_check(pc))
      pconnection_rearm(pc);
    return NULL;  // caller already owns the batch
  }

  if (pconnection_has_event(pc)) {
    if (unarmed && pconnection_rearm_check(pc))
      pconnection_rearm(pc);
    return &pc->batch;
  }

  lock(&pc->context.mutex);
  if (pc->psocket.closing && pconnection_is_final(pc)) {
    unlock(&pc->context.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

  // Never stop working while work remains.  hog_count exception to this rule is elsewhere.
  if (pconnection_work_pending(pc))
    goto retry;  // TODO: get rid of goto without adding more locking

  pc->context.working = false;
  pc->hog_count = 0;
  bool rearm = pconnection_rearm_check(pc);

  unlock(&pc->context.mutex);
  if (rearm) pconnection_rearm(pc);
  return NULL;
}


static void configure_socket(int sock) {
  int flags = fcntl(sock, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(sock, F_SETFL, flags);

  int tcp_nodelay = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*) &tcp_nodelay, sizeof(tcp_nodelay));
}

void pconnection_start(pconnection_t *pc) {
  int efd = pc->psocket.proactor->epollfd;
  start_polling(&pc->timer.epoll_io, efd);  // TODO: check for error

  pc->read_closed = false;
  pc->write_closed = false;
  epoll_extended_t *ee = &pc->psocket.epoll_io;
  ee->fd = pc->psocket.sockfd;
  ee->wanted = EPOLLIN | EPOLLOUT;
  start_polling(ee, efd);  // TODO: check for error
}

void pn_proactor_connect(pn_proactor_t *p, pn_connection_t *c, const char *addr) {
  char *buf = strdup(addr);
  assert(buf); // TODO: memory safety
  char *scheme, *user, *pass, *host, *port, *path;
  pni_parse_url(buf, &scheme, &user, &pass, &host, &port, &path);
  pconnection_t *pc = new_pconnection_t(p, c, false, host, port);
  assert(pc); // TODO: memory safety
  // TODO: check case of proactor shutting down
  lock(&pc->context.mutex);
  proactor_add(&pc->psocket);
  pn_connection_open(pc->driver.connection); /* Auto-open */

  struct addrinfo *ai = NULL;
  int fd = -1;
  if (!getaddrinfo(host, port, 0, &ai)) {
    fd = socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol);
    if (fd >= 0) {
      configure_socket(fd);
      if (!connect(fd, ai->ai_addr, ai->ai_addrlen) || errno == EINPROGRESS) {
        pc->psocket.sockfd = fd;
        pconnection_start(pc);
        unlock(&pc->context.mutex);
        freeaddrinfo(ai);
        free(buf);
        return;
      }
    }
  }

  psocket_error(&pc->psocket, errno, "connect to ");
  bool notify = wake(&pc->context);
  if (ai) freeaddrinfo(ai);
  if (fd != -1) close (fd);
  unlock(&pc->context.mutex);
  if (notify) wake_notify(&pc->context);
  free(buf);
  return;
}


static void pconnection_tick(pconnection_t *pc) {
  pn_transport_t *t = pc->driver.transport;
  if (pn_transport_get_idle_timeout(t) || pn_transport_get_remote_idle_timeout(t)) {
    ptimer_set(&pc->timer, 0);
    uint64_t now = pn_i_now2();
    uint64_t next = pn_transport_tick(t, now);
    if (next) {
      ptimer_set(&pc->timer, next - now);
    }
  }
}

void pn_connection_wake(pn_connection_t* c) {
  bool notify = false;
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    lock(&pc->context.mutex);
    if (!pc->psocket.closing) {
      pc->wake_count++;
      notify = wake(&pc->context);
    }
    unlock(&pc->context.mutex);
  }
  if (notify) wake_notify(&pc->context);
}

void pn_proactor_release_connection(pn_connection_t *c) {
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    pn_connection_driver_release_connection(&pc->driver);
  }
}

// ========================================================================
// listener
// ========================================================================

pn_listener_t *pn_event_listener(pn_event_t *e) {
  return (pn_event_class(e) == pn_listener__class()) ? (pn_listener_t*)pn_event_context(e) : NULL;
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
    pn_proactor_t *unknown = NULL;  // won't know until pn_proactor_listen
    pcontext_init(&l->context, LISTENER, unknown, l);
  }
  return l;
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog)
{
  char *buf = strdup(addr);
  assert(buf);  // TODO:  memory safety
  char *scheme, *user, *pass, *host, *port, *path;
  pni_parse_url(buf, &scheme, &user, &pass, &host, &port, &path);
  // TODO: check listener not already listening for this or another proactor
  lock(&l->context.mutex);
  l->context.proactor = p;;
  psocket_init(&l->psocket, p, false, host, port);
  l->backlog = backlog;
  proactor_add(&l->psocket);
  /* Always put an OPEN event for symmetry, even if we immediately close with err */
  pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_OPEN);
  bool notify = wake(&l->context);

  struct addrinfo *ai = NULL;
  int fd = -1;
  if (!getaddrinfo(host, port, 0, &ai)) {
    fd = socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol);
    if (fd >= 0) {
      int yes = 1;
      if (!setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        if (!bind(fd, ai->ai_addr, ai->ai_addrlen))
          if (!listen(fd, backlog)) {
            l->psocket.sockfd = fd;
            l->psocket.epoll_io.fd = fd;
            l->psocket.epoll_io.wanted = EPOLLIN;
            start_polling(&l->psocket.epoll_io, l->psocket.proactor->epollfd);  // TODO: check for error
            unlock(&l->context.mutex);
            if (notify) wake_notify(&l->context);
            free(buf);
            return;
          }
    }
  }

  psocket_error(&l->psocket, errno, "listen on");
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
  if (ai) freeaddrinfo(ai);
  free(buf);
  return;
}

// call with lock held
static inline bool listener_can_free(pn_listener_t *l) {
  return l->psocket.closing && l->close_dispatched &&
    !l->context.wake_ops;
}

static inline void listener_final_free(pn_listener_t *l) {
  pcontext_finalize(&l->context);
  free(l);
}

void pn_listener_free(pn_listener_t *l) {
  // TODO: do we need a QPID DeletionManager equivalent to be safe from inbound connection (accept) epoll events?
  if (l) {
    if (l->collector) pn_collector_free(l->collector);
    if (!l->condition) pn_condition_free(l->condition);
    if (!l->attachments) pn_free(l->attachments);
    lock(&l->context.mutex);
    bool can_free = proactor_remove(&l->psocket);
    unlock(&l->context.mutex);
    if (can_free) {
      listener_final_free(l);
      return;
    } // else... proactor_disconnect logic has assumed ownership
  }
}

static void listener_begin_close(pn_listener_t* l) {
  if (!l->psocket.closing) {
    l->psocket.closing = true;
    if (l->psocket.sockfd >= 0) {
      stop_polling(&l->psocket.epoll_io, l->psocket.proactor->epollfd);
      close(l->psocket.sockfd);
    }
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_CLOSE);
    l->pending_accepts = l->available_accepts = 0;
  }
}

void pn_listener_close(pn_listener_t* l) {
  bool notify = false;
  lock(&l->context.mutex);
  if (!l->psocket.closing) {
    listener_begin_close(l);
    notify = wake(&l->context);
  }
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}

static void listener_forced_shutdown(pn_listener_t *l) {
  // Called by proactor_free, no competing threads, no epoll activity.
  listener_begin_close(l);
  // pconnection_process will never be called again.  Zero everything.
  l->context.wake_ops = 0;
  l->close_dispatched = true;
  assert(listener_can_free(l));
  pn_listener_free(l);
}


static pn_event_batch_t *listener_process(pn_listener_t *l, uint32_t events) {
  // TODO: some parallelization of the accept mechanism.
  lock(&l->context.mutex);
  if (events) {
    l->armed = false;
    if (events & EPOLLRDHUP)
      psocket_error(&l->psocket, errno, "listener epoll");  // includes listener_begin_close
    else if (!l->psocket.closing && events & EPOLLIN) {
      l->available_accepts++;
      l->pending_accepts++;
    }
  } else {
    wake_done(&l->context); // callback accounting
  }
  pn_event_batch_t *lb = NULL;
  if (!l->context.working) {
    l->context.working = true;
    if (listener_has_event(l) || l->available_accepts)
      lb = &l->batch;
    else
      l->context.working = false;
  }
  unlock(&l->context.mutex);
  return lb;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  lock(&l->context.mutex);
  pn_event_t *e = NULL;
  if (!listener_has_event(l) && l->available_accepts && !l->psocket.closing) {
    l->available_accepts--;
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
  }
  if (listener_has_event(l))
    e = l->cached_event;
  l->cached_event = NULL;
  if (e && pn_event_type(e) == PN_LISTENER_CLOSE)
    l->close_dispatched = true;
  unlock(&l->context.mutex);
  return log_event(l, e);
}

static void listener_done(pn_listener_t *l) {
  bool notify = false;
  lock(&l->context.mutex);
  l->context.working = false;

  if (l->close_dispatched) {
    if (listener_can_free(l)) {
      unlock(&l->context.mutex);
      pn_listener_free(l);
      return;
    }
  } else {
    if (listener_has_event(l) || l->available_accepts)
      notify = wake(&l->context);
    else {
      if (!l->psocket.closing && !l->armed) {
        rearm(l->psocket.proactor, &l->psocket.epoll_io);
        l->armed = true;
      }
    }
  }
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->psocket.proactor : NULL;
}

pn_condition_t* pn_listener_condition(pn_listener_t* l) {
  return l->condition;
}

void *pn_listener_get_context(pn_listener_t *l) {
  return l->listener_context;
}

void pn_listener_set_context(pn_listener_t *l, void *context) {
  l->listener_context = context;
}

pn_record_t *pn_listener_attachments(pn_listener_t *l) {
  return l->attachments;
}

void pn_listener_accept(pn_listener_t *l, pn_connection_t *c) {
  // TODO: fuller sanity check on input args
  pconnection_t *pc = new_pconnection_t(l->psocket.proactor, c, true, l->psocket.host, l->psocket.port);
  assert(pc);  // TODO: memory safety
  int err = 0;

  lock(&l->context.mutex);
  proactor_add(&pc->psocket);
  if (l->psocket.closing)
    err = EBADF;
  else {
    if (l->pending_accepts == 0)
      err = EAGAIN;
  }

  if (err) {
    psocket_error(&l->psocket, errno, "listener state on accept");
    unlock(&l->context.mutex);
    return;
  }
  l->pending_accepts--;

  int newfd = accept(l->psocket.sockfd, NULL, 0);
  if (newfd < 0) {
    err = errno;
    psocket_error(&pc->psocket, err, "failed initialization on accept");
    psocket_error(&l->psocket, err, "accept");
  } else {
    lock(&pc->context.mutex);
    configure_socket(newfd);
    pc->psocket.sockfd = newfd;
    pconnection_start(pc);
    unlock(&pc->context.mutex);
  }

  unlock(&l->context.mutex);
}


// ========================================================================
// proactor
// ========================================================================

pn_proactor_t *pn_proactor() {
  pn_proactor_t *p = (pn_proactor_t*)calloc(1, sizeof(*p));
  if (!p) return NULL;
  p->epollfd = p->eventfd = p->timer.timerfd = -1;
  pcontext_init(&p->context, PROACTOR, p, p);
  pmutex_init(&p->eventfd_mutex);
  ptimer_init(&p->timer, 0);

  if ((p->epollfd = epoll_create(1)) >= 0)
    if ((p->eventfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
      if (p->timer.timerfd >= 0)
        if ((p->collector = pn_collector()) != NULL) {
          p->batch.next_event = &proactor_batch_next;
          start_polling(&p->timer.epoll_io, p->epollfd);  // TODO: check for error
          p->timer_armed = true;

          p->epoll_wake.psocket = NULL;
          p->epoll_wake.fd = p->eventfd;
          p->epoll_wake.type = WAKE;
          p->epoll_wake.wanted = EPOLLIN;
          start_polling(&p->epoll_wake, p->epollfd);  // TODO: check for error
          return p;
        }
    }

  if (p->epollfd >= 0) close(p->epollfd);
  if (p->eventfd >= 0) close(p->eventfd);
  ptimer_finalize(&p->timer);
  if (p->collector) pn_free(p->collector);
  free (p);
  return NULL;
}

void pn_proactor_free(pn_proactor_t *p) {
  //  No competing threads, not even a pending timer
  close(p->epollfd);
  close(p->eventfd);
  ptimer_finalize(&p->timer);
  while (p->psockets) {
    psocket_t *ps = p->psockets;
    p->psockets = ps->next;
    pconnection_t *pc = as_pconnection(ps);
    if (pc) {
      pconnection_forced_shutdown(pc);
    } else {
      pn_listener_t *l = as_listener(ps);
      if (l)
        listener_forced_shutdown(l);
    }
  }

  pn_collector_free(p->collector);
  pmutex_finalize(&p->eventfd_mutex);
  pcontext_finalize(&p->context);
  free(p);
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == pn_proactor__class()) return (pn_proactor_t*)pn_event_context(e);
  pn_listener_t *l = pn_event_listener(e);
  if (l) return l->psocket.proactor;
  pn_connection_t *c = pn_event_connection(e);
  if (c) return pn_connection_proactor(pn_event_connection(e));
  return NULL;
}

static void proactor_add_event(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, pn_proactor__class(), p, t);
}

// Call with lock held.  Leave unchanged if events pending.
// There can be multiple interrupts but only one inside the collector to avoid coalescing.
// Return true if there is an event in the collector.
static bool proactor_update_batch(pn_proactor_t *p) {
  if (proactor_has_event(p))
    return true;
  if (p->deferred_interrupts > 0) {
    // drain these first
    --p->deferred_interrupts;
    --p->interrupts;
    proactor_add_event(p, PN_PROACTOR_INTERRUPT);
    return true;
  }

  if (p->timer_expired) {
    p->timer_expired = false;
    proactor_add_event(p, PN_PROACTOR_TIMEOUT);
    return true;
  }

  int ec = 0;
  if (p->interrupts > 0) {
    --p->interrupts;
    proactor_add_event(p, PN_PROACTOR_INTERRUPT);
    ec++;
    if (p->interrupts > 0)
      p->deferred_interrupts = p->interrupts;
  }
  if (p->inactive && ec == 0) {
    p->inactive = false;
    ec++;
    proactor_add_event(p, PN_PROACTOR_INACTIVE);
  }
  return ec > 0;
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  pn_event_t *e = NULL;
  lock(&p->context.mutex);
  proactor_update_batch(p);
  if (proactor_has_event(p))
    e = p->cached_event;
  unlock(&p->context.mutex);
  p->cached_event = NULL;
  return log_event(p, e);
}

static pn_event_batch_t *proactor_process(pn_proactor_t *p, bool timeout) {
  bool timer_fired = timeout && ptimer_callback(&p->timer) != 0;
  lock(&p->context.mutex);
  if (timeout) {
    p->timer_armed = false;
    if (timer_fired && !p->timer_cancelled)
      p->timer_expired = true;
  } else {
    wake_done(&p->context);
  }
  if (!p->context.working) {       /* Can generate proactor events */
    if (proactor_update_batch(p)) {
      p->context.working = true;
      unlock(&p->context.mutex);
      return &p->batch;
    }
  }
  bool rearm_timer = !p->timer_armed;
  unlock(&p->context.mutex);
  if (rearm_timer)
    rearm(p, &p->timer.epoll_io);
  return NULL;
}

static void proactor_add(psocket_t *ps) {
  pn_proactor_t *p = ps->proactor;
  lock(&p->context.mutex);
  if (p->psockets) {
    p->psockets->prev = ps;
    ps->next = p->psockets;
    p->psockets = ps;
  }
  else p->psockets = ps;
  unlock(&p->context.mutex);
}

// call with psocket's mutex held
// return true if safe for caller to free psocket
static bool proactor_remove(psocket_t *ps) {
  pn_proactor_t *p = ps->proactor;
  lock(&p->context.mutex);
  bool notify = false;
  bool can_free = true;
  if (ps->disconnecting) {
    // No longer on psockets list
    if (--ps->disconnect_ops == 0) {
      if (--p->disconnects_pending == 0 && !p->psockets) {
        p->inactive = true;
        notify = wake(&p->context);
      }
    }
    else                  // procator_disconnect() still processing
      can_free = false;   // this psocket
  }
  else {
    // normal case
    if (ps->prev)
      ps->prev->next = ps->next;
    else {
      p->psockets = ps->next;
      ps->next = NULL;
      if (p->psockets)
        p->psockets->prev = NULL;
    }
    if (ps->next)
      ps->next->prev = ps->prev;

    if (!p->psockets && !p->disconnects_pending && !p->shutting_down) {
      p->inactive = true;
      notify = wake(&p->context);
    }
  }
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
  return can_free;
}

static pn_event_batch_t *process_inbound_wake(pn_proactor_t *p) {
  pcontext_t *ctx = wake_pop_front(p);
  if (ctx) {
    switch (ctx->type) {
    case PROACTOR:
      return proactor_process(p, false);
    case PCONNECTION:
      return pconnection_process((pconnection_t *) ctx->owner, 0, false, false);
    case LISTENER:
      return listener_process((pn_listener_t *) ctx->owner, 0);
    default:
      assert(ctx->type == WAKEABLE); // TODO: implement or remove
    }
  }
  return NULL;
}

static pn_event_batch_t *proactor_do_epoll(struct pn_proactor_t* p, bool can_block) {
  int timeout = can_block ? -1 : 0;
  while(true) {
    pn_event_batch_t *batch = NULL;
    struct epoll_event ev;
    int n = epoll_wait(p->epollfd, &ev, 1, timeout);

    if (n < 0) {
      if (errno != EINTR)
        perror("epoll_wait"); // TODO: proper log
      if (!can_block)
        return NULL;
      else
        continue;
    } else if (n == 0) {
      if (!can_block)
        return NULL;
      else {
        perror("epoll_wait unexpected timeout"); // TODO: proper log
        continue;
      }
    }
    assert(n == 1);
    epoll_extended_t *ee = (epoll_extended_t *) ev.data.ptr;

    if (ee->type == WAKE) {
      batch = process_inbound_wake(p);
    } else if (ee->type == PROACTOR_TIMER) {
      batch = proactor_process(p, true);
    } else {
      pconnection_t *pc = as_pconnection(ee->psocket);
      if (pc) {
        if (ee->type == PCONNECTION_IO) {
          batch = pconnection_process(pc, ev.events, false, false);
        } else {
          assert(ee->type == PCONNECTION_TIMER);
          batch = pconnection_process(pc, 0, true, false);
        }
      }
      else {
        pn_listener_t *l = as_listener(ee->psocket);
        // TODO: can any of the listener processing be parallelized like IOCP?
        batch = listener_process(l, ev.events);
      }
    }

    if (batch) return batch;
    // No Proton event generated.  epoll_wait() again.
  }
}

pn_event_batch_t *pn_proactor_wait(struct pn_proactor_t* p) {
  return proactor_do_epoll(p, true);
}

pn_event_batch_t *pn_proactor_get(struct pn_proactor_t* p) {
  return proactor_do_epoll(p, false);
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) {
    pconnection_done(pc);
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    listener_done(l);
    return;
  }
  pn_proactor_t *bp = batch_proactor(batch);
  if (bp == p) {
    bool notify = false;
    lock(&p->context.mutex);
    bool rearm_timer = !p->timer_armed;
    p->context.working = false;
    proactor_update_batch(p);
    if (proactor_has_event(p))
      notify = wake(&p->context);
    unlock(&p->context.mutex);
    if (notify) wake_notify(&p->context);
    if (rearm_timer)
      rearm(p, &p->timer.epoll_io);
    return;
  }
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  lock(&p->context.mutex);
  ++p->interrupts;
  bool notify = wake(&p->context);
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  bool notify = false;
  lock(&p->context.mutex);
  p->timer_cancelled = false;
  if (t == 0) {
    ptimer_set(&p->timer, 0);
    p->timer_expired = true;
    notify = wake(&p->context);
  } else {
    ptimer_set(&p->timer, t);
  }
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
}

void pn_proactor_cancel_timeout(pn_proactor_t *p) {
  lock(&p->context.mutex);
  p->timer_cancelled = true;  // stays cancelled until next set_timeout()
  p->timer_expired = false;
  ptimer_set(&p->timer, 0);
  unlock(&p->context.mutex);
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->psocket.proactor : NULL;
}

void pn_proactor_disconnect(pn_proactor_t *p, pn_condition_t *cond) {
  lock(&p->context.mutex);
  // Move the whole psockets list into a disconnecting state
  psocket_t *disconnecting_psockets = p->psockets;
  p->psockets = NULL;
  // First pass: mark each psocket as disconnecting and update global pending count.
  psocket_t *ps = disconnecting_psockets;
  while (ps) {
    ps->disconnecting = true;
    ps->disconnect_ops = 2;   // Second pass below and proactor_remove(), in any order.
    p->disconnects_pending++;
    ps = ps->next;
  }
  unlock(&p->context.mutex);
  if (!disconnecting_psockets)
    return;

  // Second pass: different locking, close the psockets, free them if !disconnect_ops
  bool notify = false;
  for (ps = disconnecting_psockets; ps; ps = ps->next) {
    bool do_free = false;
    pmutex *ps_mutex = NULL;
    pconnection_t *pc = as_pconnection(ps);
    if (pc) {
      ps_mutex = &pc->context.mutex;
      lock(ps_mutex);
      if (!ps->closing) {
        if (cond) {
          pn_condition_copy(pn_transport_condition(pc->driver.transport), cond);
        }
        pn_connection_driver_close(&pc->driver);
      }
    } else {
      pn_listener_t *l = as_listener(ps);
      assert(l);
      ps_mutex = &l->context.mutex;
      lock(ps_mutex);
      if (!ps->closing) {
        if (cond) {
          pn_condition_copy(pn_listener_condition(l), cond);
        }
        pn_listener_close(l);
      }
    }

    lock(&p->context.mutex);
    if (--ps->disconnect_ops == 0) {
      do_free = true;
      if (--p->disconnects_pending == 0 && !p->psockets) {
        p->inactive = true;
        notify = wake(&p->context);
      }
    }
    unlock(&p->context.mutex);
    unlock(ps_mutex);

    if (do_free) {
      if (pc) pconnection_final_free(pc);
      else listener_final_free(as_listener(ps));
    }
  }
  if (notify)
    wake_notify(&p->context);
}


const struct sockaddr_storage *pn_proactor_addr_sockaddr(const pn_proactor_addr_t *addr) {
  assert(false);
  return NULL;
}

const struct pn_proactor_addr_t *pn_proactor_addr_local(pn_transport_t *t) {
  assert(false);
  return NULL;
}

const struct pn_proactor_addr_t *pn_proactor_addr_remote(pn_transport_t *t) {
  assert(false);
  return NULL;
}

size_t pn_proactor_addr_str(const struct pn_proactor_addr_t* addr, char *buf, size_t len) {
  struct sockaddr_storage *sa = (struct sockaddr_storage*)addr;
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  int err = getnameinfo((struct sockaddr *)sa, sizeof(*sa), host, sizeof(host), port, sizeof(port),
                        NI_NUMERICHOST | NI_NUMERICSERV);
  if (!err) {
    return snprintf(buf, len, "%s:%s", host, port); /* FIXME aconway 2017-03-29: ipv6 format? */
  } else {
    if (buf) *buf = '\0';
    return 0;
  }
}

