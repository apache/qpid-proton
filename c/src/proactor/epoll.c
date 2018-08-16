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

/* Enable POSIX features beyond c99 for modern pthread and standard strerror_r() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* Avoid GNU extensions, in particular the incompatible alternative strerror_r() */
#undef _GNU_SOURCE

#include "../core/log_private.h"
#include "./proactor-internal.h"

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
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
#include <time.h>

#include "./netaddr-internal.h" /* Include after socket/inet headers */

// TODO: replace timerfd per connection with global lightweight timer mechanism.
// logging in general
// SIGPIPE?
// Can some of the mutexes be spinlocks (any benefit over adaptive pthread mutex)?
//   Maybe futex is even better?
// See other "TODO" in code.
//
// Consider case of large number of wakes: proactor_do_epoll() could start by
// looking for pending wakes before a kernel call to epoll_wait(), or there
// could be several eventfds with random assignment of wakeables.


typedef char strerrorbuf[1024];      /* used for pstrerror message buffer */

/* Like strerror_r but provide a default message if strerror_r fails */
static void pstrerror(int err, strerrorbuf msg) {
  int e = strerror_r(err, msg, sizeof(strerrorbuf));
  if (e) snprintf(msg, sizeof(strerrorbuf), "unknown error %d", err);
}

/* Internal error, no recovery */
#define EPOLL_FATAL(EXPR, SYSERRNO)                                     \
  do {                                                                  \
    strerrorbuf msg;                                                    \
    pstrerror((SYSERRNO), msg);                                         \
    fprintf(stderr, "epoll proactor failure in %s:%d: %s: %s\n",        \
            __FILE__, __LINE__ , #EXPR, msg);                           \
    abort();                                                            \
  } while (0)

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
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
  if (pthread_mutex_init(pm, &attr)) {
    perror("pthread failure");
    abort();
  }
}

static void pmutex_finalize(pthread_mutex_t *m) { pthread_mutex_destroy(m); }
static inline void lock(pmutex *m) { pthread_mutex_lock(m); }
static inline void unlock(pmutex *m) { pthread_mutex_unlock(m); }

typedef struct acceptor_t acceptor_t;

typedef enum {
  WAKE,   /* see if any work to do in proactor/psocket context */
  PCONNECTION_IO,
  PCONNECTION_IO_2,
  PCONNECTION_TIMER,
  LISTENER_IO,
  CHAINED_EPOLL,
  PROACTOR_TIMER } epoll_type_t;

// Data to use with epoll.
typedef struct epoll_extended_t {
  struct psocket_t *psocket;  // pconnection, listener, or NULL -> proactor
  int fd;
  epoll_type_t type;   // io/timer/wakeup
  uint32_t wanted;     // events to poll for
  bool polling;
  pmutex barrier_mutex;
} epoll_extended_t;

/* epoll_ctl()/epoll_wait() do not form a memory barrier, so cached memory
   writes to struct epoll_extended_t in the EPOLL_ADD thread might not be
   visible to epoll_wait() thread. This function creates a memory barrier,
   called before epoll_ctl() and after epoll_wait()
*/
static void memory_barrier(epoll_extended_t *ee) {
  // Mutex lock/unlock has the side-effect of being a memory barrier.
  lock(&ee->barrier_mutex);
  unlock(&ee->barrier_mutex);
}

/*
 * This timerfd logic assumes EPOLLONESHOT and there never being two
 * active timeout callbacks.  There can be multiple (or zero)
 * unclaimed expiries processed in a single callback.
 *
 * timerfd_set() documentation implies a crisp relationship between
 * timer expiry count and oldt's return value, but a return value of
 * zero is ambiguous.  It can lead to no EPOLLIN, EPOLLIN + expected
 * read, or
 *
 *   event expiry (in kernel) -> EPOLLIN
 *   cancel/settime(0) (thread A) (number of expiries resets to zero)
 *   read(timerfd) -> -1, EAGAIN  (thread B servicing epoll event)
 *
 * The original implementation with counters to track expiry counts
 * was abandoned in favor of "in doubt" transitions and resolution
 * at shutdown.
 */

typedef struct ptimer_t {
  pmutex mutex;
  int timerfd;
  epoll_extended_t epoll_io;
  bool timer_active;
  bool in_doubt;  // 0 or 1 callbacks are possible
  bool shutting_down;
} ptimer_t;

static bool ptimer_init(ptimer_t *pt, struct psocket_t *ps) {
  pt->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  pmutex_init(&pt->mutex);
  pt->timer_active = false;
  pt->in_doubt = false;
  pt->shutting_down = false;
  epoll_type_t type = ps ? PCONNECTION_TIMER : PROACTOR_TIMER;
  pt->epoll_io.psocket = ps;
  pt->epoll_io.fd = pt->timerfd;
  pt->epoll_io.type = type;
  pt->epoll_io.wanted = EPOLLIN;
  pt->epoll_io.polling = false;
  return (pt->timerfd >= 0);
}

// Call with ptimer lock held
static void ptimer_set_lh(ptimer_t *pt, uint64_t t_millis) {
  struct itimerspec newt, oldt;
  memset(&newt, 0, sizeof(newt));
  newt.it_value.tv_sec = t_millis / 1000;
  newt.it_value.tv_nsec = (t_millis % 1000) * 1000000;

  timerfd_settime(pt->timerfd, 0, &newt, &oldt);
  if (pt->timer_active && oldt.it_value.tv_nsec == 0 && oldt.it_value.tv_sec == 0) {
    // EPOLLIN is possible but not assured
    pt->in_doubt = true;
  }
  pt->timer_active = t_millis;
}

static void ptimer_set(ptimer_t *pt, uint64_t t_millis) {
  // t_millis == 0 -> cancel
  lock(&pt->mutex);
  if ((t_millis == 0 && !pt->timer_active) || pt->shutting_down) {
    unlock(&pt->mutex);
    return;  // nothing to do
  }
  ptimer_set_lh(pt, t_millis);
  unlock(&pt->mutex);
}

/* Read from a timer or event FD */
static uint64_t read_uint64(int fd) {
  uint64_t result = 0;
  ssize_t n = read(fd, &result, sizeof(result));
  if (n != sizeof(result) && !(n < 0 && errno == EAGAIN)) {
    EPOLL_FATAL("timerfd or eventfd read error", errno);
  }
  return result;
}

// Callback bookkeeping. Return true if there is an expired timer.
static bool ptimer_callback(ptimer_t *pt) {
  lock(&pt->mutex);
  struct itimerspec current;
  if (timerfd_gettime(pt->timerfd, &current) == 0) {
    if (current.it_value.tv_nsec == 0 && current.it_value.tv_sec == 0)
      pt->timer_active = false;
  }
  uint64_t u_exp_count = read_uint64(pt->timerfd);
  if (!pt->timer_active) {
    // Expiry counter just cleared, timer not set, timerfd not armed
    pt->in_doubt = false;
  }
  unlock(&pt->mutex);
  return u_exp_count > 0;
}

// Return true if timerfd has and will have no pollable expiries in the current armed state
static bool ptimer_shutdown(ptimer_t *pt, bool currently_armed) {
  lock(&pt->mutex);
  if (currently_armed) {
    ptimer_set_lh(pt, 0);
    pt->shutting_down = true;
    if (pt->in_doubt)
      // Force at least one callback.  If two, second cannot proceed with unarmed timerfd.
      ptimer_set_lh(pt, 1);
  }
  else
    pt->shutting_down = true;
  bool rv = !pt->in_doubt;
  unlock(&pt->mutex);
  return rv;
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

const char *AMQP_PORT = "5672";
const char *AMQP_PORT_NAME = "amqp";

// The number of times a connection event batch may be replenished for
// a thread between calls to wait().  Some testing shows that
// increasing this value above 1 actually slows performance slightly
// and increases latency.
#define HOG_MAX 1

/* pn_proactor_t and pn_listener_t are plain C structs with normal memory management.
   Class definitions are for identification as pn_event_t context only.
*/
PN_STRUCT_CLASSDEF(pn_proactor, CID_pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener, CID_pn_listener)

static bool start_polling(epoll_extended_t *ee, int epollfd) {
  if (ee->polling)
    return false;
  ee->polling = true;
  struct epoll_event ev = {0};
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  memory_barrier(ee);
  return (epoll_ctl(epollfd, EPOLL_CTL_ADD, ee->fd, &ev) == 0);
}

static void stop_polling(epoll_extended_t *ee, int epollfd) {
  // TODO: check for error, return bool or just log?
  // TODO: is EPOLL_CTL_DEL ever needed beyond auto de-register when ee->fd is closed?
  if (ee->fd == -1 || !ee->polling || epollfd == -1)
    return;
  struct epoll_event ev = {0};
  ev.data.ptr = ee;
  ev.events = 0;
  memory_barrier(ee);
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, ee->fd, &ev) == -1)
    EPOLL_FATAL("EPOLL_CTL_DEL", errno);
  ee->fd = -1;
  ee->polling = false;
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
 * External wake operations, like pn_connection_wake() are built on top of
 * the internal wake mechanism.
 *
 * pn_proactor_interrupt() must be async-signal-safe so it has a dedicated
 * eventfd to allow a lock-free pn_proactor_interrupt() implementation.
 */

/*
 * **** epollfd and epollfd_2 ****
 *
 * This implementation allows multiple threads to call epoll_wait()
 * concurrently (as opposed to having a single thread call
 * epoll_wait() and feed work to helper threads).  Unfortunately
 * with this approach, it is not possible to change the event
 * mask in one thread and be certain if zero or one callbacks occurred
 * on the previous event mask.  This can greatly complicate ordered
 * shutdown.  (See PROTON-1842)
 *
 * Currently, only pconnection sockets change between EPOLLIN,
 * EPOLLOUT, or both.  The rest use a constant EPOLLIN event mask.
 * Instead of trying to change the event mask for pconnection sockets,
 * if there is a missing attribute, it is added (EPOLLIN or EPOLLOUT)
 * as an event mask on the secondary or chained epollfd_2.  epollfd_2
 * is part of the epollfd fd set, so active events in epollfd_2 are
 * also seen in epollfd (but require a separate epoll_wait() and
 * rearm() to extract).
 *
 * Using this method and EPOLLONESHOT, it is possible to wait for all
 * outstanding armings on a socket to "resolve" via epoll_wait()
 * callbacks before freeing resources.
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
  struct pcontext_t *wake_next; // wake list, guarded by proactor eventfd_mutex
  bool closing;
  // Next 4 are protected by the proactor mutex
  struct pcontext_t* next;  /* Protected by proactor.mutex */
  struct pcontext_t* prev;  /* Protected by proactor.mutex */
  int disconnect_ops;           /* ops remaining before disconnect complete */
  bool disconnecting;           /* pn_proactor_disconnect */
} pcontext_t;

static void pcontext_init(pcontext_t *ctx, pcontext_type_t t, pn_proactor_t *p, void *o) {
  memset(ctx, 0, sizeof(*ctx));
  pmutex_init(&ctx->mutex);
  ctx->proactor = p;
  ctx->owner = o;
  ctx->type = t;
}

static void pcontext_finalize(pcontext_t* ctx) {
  pmutex_finalize(&ctx->mutex);
}

/* common to connection and listener */
typedef struct psocket_t {
  pn_proactor_t *proactor;
  // Remaining protected by the pconnection/listener mutex
  int sockfd;
  epoll_extended_t epoll_io;
  pn_listener_t *listener;      /* NULL for a connection socket */
  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
} psocket_t;

struct pn_proactor_t {
  pcontext_t context;
  int epollfd;
  int epollfd_2;
  ptimer_t timer;
  pn_collector_t *collector;
  pcontext_t *contexts;         /* in-use contexts for PN_PROACTOR_INACTIVE and cleanup */
  epoll_extended_t epoll_wake;
  epoll_extended_t epoll_interrupt;
  epoll_extended_t epoll_secondary;
  pn_event_batch_t batch;
  size_t disconnects_pending;   /* unfinished proactor disconnects*/
  // need_xxx flags indicate we should generate PN_PROACTOR_XXX on the next update_batch()
  bool need_interrupt;
  bool need_inactive;
  bool need_timeout;
  bool timeout_set; /* timeout has been set by user and not yet cancelled or generated event */
  bool timeout_processed;  /* timeout event dispatched in the most recent event batch */
  bool timer_armed; /* timer is armed in epoll */
  bool shutting_down;
  // wake subsystem
  int eventfd;
  pmutex eventfd_mutex;
  bool wakes_in_progress;
  pcontext_t *wake_list_first;
  pcontext_t *wake_list_last;
  // Interrupts have a dedicated eventfd because they must be async-signal safe.
  int interruptfd;
  // If the process runs out of file descriptors, disarm listening sockets temporarily and save them here.
  acceptor_t *overflow;
  pmutex overflow_mutex;
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
        p->wake_list_last->wake_next = ctx;
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
  if (ctx->proactor->eventfd == -1)
    return;
  uint64_t increment = 1;
  if (write(ctx->proactor->eventfd, &increment, sizeof(uint64_t)) != sizeof(uint64_t))
    EPOLL_FATAL("setting eventfd", errno);
}

// call with no locks
static pcontext_t *wake_pop_front(pn_proactor_t *p) {
  pcontext_t *ctx = NULL;
  lock(&p->eventfd_mutex);
  assert(p->wakes_in_progress);
  if (p->wake_list_first) {
    ctx = p->wake_list_first;
    p->wake_list_first = ctx->wake_next;
    if (!p->wake_list_first) p->wake_list_last = NULL;
    ctx->wake_next = NULL;

    if (!p->wake_list_first) {
      /* Reset the eventfd until a future write.
       * Can the read system call be made without holding the lock?
       * Note that if the reads/writes happen out of order, the wake
       * mechanism will hang. */
      (void)read_uint64(p->eventfd);
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


static void psocket_init(psocket_t* ps, pn_proactor_t* p, pn_listener_t *listener, const char *addr)
{
  ps->epoll_io.psocket = ps;
  ps->epoll_io.fd = -1;
  ps->epoll_io.type = listener ? LISTENER_IO : PCONNECTION_IO;
  ps->epoll_io.wanted = 0;
  ps->epoll_io.polling = false;
  ps->proactor = p;
  ps->listener = listener;
  ps->sockfd = -1;
  pni_parse_addr(addr, ps->addr_buf, sizeof(ps->addr_buf), &ps->host, &ps->port);
}

typedef struct pconnection_t {
  psocket_t psocket;
  pcontext_t context;
  uint32_t new_events;
  uint32_t new_events_2;
  int wake_count;
  bool server;                /* accept, not connect */
  bool tick_pending;
  bool timer_armed;
  bool queued_disconnect;     /* deferred from pn_proactor_disconnect() */
  pn_condition_t *disconnect_condition;
  ptimer_t timer;  // TODO: review one timerfd per connection
  // Following values only changed by (sole) working context:
  uint32_t current_arm;  // active epoll io events
  uint32_t current_arm_2;  // secondary active epoll io events
  bool connected;
  bool read_blocked;
  bool write_blocked;
  bool disconnected;
  int hog_count; // thread hogging limiter
  pn_event_batch_t batch;
  pn_connection_driver_t driver;
  struct pn_netaddr_t local, remote; /* Actual addresses */
  struct addrinfo *addrinfo;         /* Resolved address list */
  struct addrinfo *ai;               /* Current connect address */
  pmutex rearm_mutex;                /* protects pconnection_rearm from out of order arming*/
  epoll_extended_t epoll_io_2;
  epoll_extended_t *rearm_target;    /* main or secondary epollfd */
} pconnection_t;

/* Protects read/update of pn_connnection_t pointer to it's pconnection_t
 *
 * Global because pn_connection_wake()/pn_connection_proactor() navigate from
 * the pn_connection_t before we know the proactor or driver. Critical sections
 * are small: only get/set of the pn_connection_t driver pointer.
 *
 * TODO: replace mutex with atomic load/store
 */
static pthread_mutex_t driver_ptr_mutex = PTHREAD_MUTEX_INITIALIZER;

static pconnection_t *get_pconnection(pn_connection_t* c) {
  if (!c) return NULL;
  lock(&driver_ptr_mutex);
  pn_connection_driver_t *d = *pn_connection_driver_ptr(c);
  unlock(&driver_ptr_mutex);
  if (!d) return NULL;
  return (pconnection_t*)((char*)d-offsetof(pconnection_t, driver));
}

static void set_pconnection(pn_connection_t* c, pconnection_t *pc) {
  lock(&driver_ptr_mutex);
  *pn_connection_driver_ptr(c) = pc ? &pc->driver : NULL;
  unlock(&driver_ptr_mutex);
}

/*
 * A listener can have mutiple sockets (as specified in the addrinfo).  They
 * are armed separately.  The individual psockets can be part of at most one
 * list: the global proactor overflow retry list or the per-listener list of
 * pending accepts (valid inbound socket obtained, but pn_listener_accept not
 * yet called by the application).  These lists will be small and quick to
 * traverse.
 */

struct acceptor_t{
  psocket_t psocket;
  int accepted_fd;
  bool armed;
  bool overflowed;
  acceptor_t *next;              /* next listener list member */
  struct pn_netaddr_t addr;      /* listening address */
};

struct pn_listener_t {
  acceptor_t *acceptors;          /* Array of listening sockets */
  size_t acceptors_size;
  int active_count;               /* Number of listener sockets registered with epoll */
  pcontext_t context;
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *listener_context;
  acceptor_t *pending_acceptors;  /* list of those with a valid inbound fd*/
  int pending_count;
  bool unclaimed;                 /* attach event dispatched but no pn_listener_attach() call yet */
  size_t backlog;
  bool close_dispatched;
  pmutex rearm_mutex;             /* orders rearms/disarms, nothing else */
};

static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool timeout, bool topup, bool is_io_2);
static void write_flush(pconnection_t *pc);
static void listener_begin_close(pn_listener_t* l);
static void proactor_add(pcontext_t *ctx);
static bool proactor_remove(pcontext_t *ctx);

static inline pconnection_t *psocket_pconnection(psocket_t* ps) {
  return ps->listener ? NULL : (pconnection_t*)ps;
}

static inline pn_listener_t *psocket_listener(psocket_t* ps) {
  return ps->listener;
}

static inline acceptor_t *psocket_acceptor(psocket_t* ps) {
  return !ps->listener ? NULL : (acceptor_t *)ps;
}

static inline pconnection_t *pcontext_pconnection(pcontext_t *c) {
  return c->type == PCONNECTION ?
    (pconnection_t*)((char*)c - offsetof(pconnection_t, context)) : NULL;
}

static inline pn_listener_t *pcontext_listener(pcontext_t *c) {
  return c->type == LISTENER ?
    (pn_listener_t*)((char*)c - offsetof(pn_listener_t, context)) : NULL;
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
  return pn_connection_driver_has_event(&pc->driver);
}

static inline bool listener_has_event(pn_listener_t *l) {
  return pn_collector_peek(l->collector) || (l->pending_count && !l->unclaimed);
}

static inline bool proactor_has_event(pn_proactor_t *p) {
  return pn_collector_peek(p->collector);
}

static pn_event_t *log_event(void* p, pn_event_t *e) {
  if (e) {
    pn_logf("[%p]:(%s)", (void*)p, pn_event_type_name(pn_event_type(e)));
  }
  return e;
}

static void psocket_error_str(psocket_t *ps, const char *msg, const char* what) {
  if (!ps->listener) {
    pn_connection_driver_t *driver = &psocket_pconnection(ps)->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pni_proactor_set_cond(pn_transport_condition(driver->transport), what, ps->host, ps->port, msg);
    pn_connection_driver_close(driver);
  } else {
    pn_listener_t *l = psocket_listener(ps);
    pni_proactor_set_cond(l->condition, what, ps->host, ps->port, msg);
    listener_begin_close(l);
  }
}

static void psocket_error(psocket_t *ps, int err, const char* what) {
  strerrorbuf msg;
  pstrerror(err, msg);
  psocket_error_str(ps, msg, what);
}

static void psocket_gai_error(psocket_t *ps, int gai_err, const char* what) {
  psocket_error_str(ps, gai_strerror(gai_err), what);
}

static void rearm(pn_proactor_t *p, epoll_extended_t *ee) {
  struct epoll_event ev = {0};
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  memory_barrier(ee);
  if (epoll_ctl(p->epollfd, EPOLL_CTL_MOD, ee->fd, &ev) == -1)
    EPOLL_FATAL("arming polled file descriptor", errno);
}

// Only used by pconnection_t if two separate epoll interests in play
static void rearm_2(pn_proactor_t *p, epoll_extended_t *ee) {
  // Delay registration until first use.  It's not OK to register or arm
  // with an event mask of 0 (documented below).  It is OK to leave a
  // disabled event registered until the next EPOLLONESHOT.
  if (!ee->polling) {
    ee->fd = ee->psocket->sockfd;
    start_polling(ee, p->epollfd_2);
  } else {
    struct epoll_event ev = {0};
    ev.data.ptr = ee;
    ev.events = ee->wanted | EPOLLONESHOT;
    memory_barrier(ee);
    if (epoll_ctl(p->epollfd_2, EPOLL_CTL_MOD, ee->fd, &ev) == -1)
      EPOLL_FATAL("arming polled file descriptor (secondary)", errno);
  }
}

static void listener_list_append(acceptor_t **start, acceptor_t *item) {
  assert(item->next == NULL);
  if (*start) {
    acceptor_t *end = *start;
    while (end->next)
      end = end->next;
    end->next = item;
  }
  else *start = item;
}

static acceptor_t *listener_list_next(acceptor_t **start) {
  acceptor_t *item = *start;
  if (*start) *start = (*start)->next;
  if (item) item->next = NULL;
  return item;
}

// Add an overflowing listener to the overflow list. Called with listener context lock held.
static void listener_set_overflow(acceptor_t *a) {
  a->overflowed = true;
  pn_proactor_t *p = a->psocket.proactor;
  lock(&p->overflow_mutex);
  listener_list_append(&p->overflow, a);
  unlock(&p->overflow_mutex);
}

/* TODO aconway 2017-06-08: we should also call proactor_rearm_overflow after a fixed delay,
   even if the proactor has not freed any file descriptors, since other parts of the process
   might have*/

// Activate overflowing listeners, called when there may be available file descriptors.
static void proactor_rearm_overflow(pn_proactor_t *p) {
  lock(&p->overflow_mutex);
  acceptor_t* ovflw = p->overflow;
  p->overflow = NULL;
  unlock(&p->overflow_mutex);
  acceptor_t *a = listener_list_next(&ovflw);
  while (a) {
    pn_listener_t *l = a->psocket.listener;
    lock(&l->context.mutex);
    bool rearming = !l->context.closing;
    bool notify = false;
    assert(!a->armed);
    assert(a->overflowed);
    a->overflowed = false;
    if (rearming) {
      lock(&l->rearm_mutex);
      a->armed = true;
    }
    else notify = wake(&l->context);
    unlock(&l->context.mutex);
    if (rearming) {
      rearm(p, &a->psocket.epoll_io);
      unlock(&l->rearm_mutex);
    }
    if (notify) wake_notify(&l->context);
    a = listener_list_next(&ovflw);
  }
}

// Close an FD and rearm overflow listeners.  Call with no listener locks held.
static int pclosefd(pn_proactor_t *p, int fd) {
  int err = close(fd);
  if (!err) proactor_rearm_overflow(p);
  return err;
}


// ========================================================================
// pconnection
// ========================================================================

static void pconnection_tick(pconnection_t *pc);

static const char *pconnection_setup(pconnection_t *pc, pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, bool server, const char *addr)
{
  memset(pc, 0, sizeof(*pc));

  if (pn_connection_driver_init(&pc->driver, c, t) != 0) {
    free(pc);
    return "pn_connection_driver_init failure";
  }

  pcontext_init(&pc->context, PCONNECTION, p, pc);
  psocket_init(&pc->psocket, p, NULL, addr);
  pc->new_events = 0;
  pc->new_events_2 = 0;
  pc->wake_count = 0;
  pc->tick_pending = false;
  pc->timer_armed = false;
  pc->queued_disconnect = false;
  pc->disconnect_condition = NULL;

  pc->current_arm = 0;
  pc->current_arm_2 = 0;
  pc->connected = false;
  pc->read_blocked = true;
  pc->write_blocked = true;
  pc->disconnected = false;
  pc->hog_count = 0;
  pc->batch.next_event = pconnection_batch_next;

  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }

  if (!ptimer_init(&pc->timer, &pc->psocket)) {
    psocket_error(&pc->psocket, errno, "timer setup");
    pc->disconnected = true;    /* Already failed */
  }
  pmutex_init(&pc->rearm_mutex);

  epoll_extended_t *ee = &pc->epoll_io_2;
  ee->psocket = &pc->psocket;
  ee->fd = -1;
  ee->type = PCONNECTION_IO_2;
  ee->wanted = 0;
  ee->polling = false;

  /* Set the pconnection_t backpointer last.
     Connections that were released by pn_proactor_release_connection() must not reveal themselves
     to be re-associated with a proactor till setup is complete.
   */
  set_pconnection(pc->driver.connection, pc);

  return NULL;
}

// Call with lock held and closing == true (i.e. pn_connection_driver_finished() == true), timer cancelled.
// Return true when all possible outstanding epoll events associated with this pconnection have been processed.
static inline bool pconnection_is_final(pconnection_t *pc) {
  return !pc->current_arm && !pc->current_arm_2 && !pc->timer_armed && !pc->context.wake_ops;
}

static void pconnection_final_free(pconnection_t *pc) {
  // Ensure any lingering pconnection_rearm is all done.
  lock(&pc->rearm_mutex);  unlock(&pc->rearm_mutex);

  if (pc->driver.connection) {
    set_pconnection(pc->driver.connection, NULL);
  }
  if (pc->addrinfo) {
    freeaddrinfo(pc->addrinfo);
  }
  pmutex_finalize(&pc->rearm_mutex);
  pn_condition_free(pc->disconnect_condition);
  pn_connection_driver_destroy(&pc->driver);
  pcontext_finalize(&pc->context);
  free(pc);
}

// call without lock, but only if pconnection_is_final() is true
static void pconnection_cleanup(pconnection_t *pc) {
  stop_polling(&pc->psocket.epoll_io, pc->psocket.proactor->epollfd);
  if (pc->psocket.sockfd != -1)
    pclosefd(pc->psocket.proactor, pc->psocket.sockfd);
  stop_polling(&pc->timer.epoll_io, pc->psocket.proactor->epollfd);
  ptimer_finalize(&pc->timer);
  lock(&pc->context.mutex);
  bool can_free = proactor_remove(&pc->context);
  unlock(&pc->context.mutex);
  if (can_free)
    pconnection_final_free(pc);
  // else proactor_disconnect logic owns psocket and its final free
}

// Call with lock held or from forced_shutdown
static void pconnection_begin_close(pconnection_t *pc) {
  if (!pc->context.closing) {
    pc->context.closing = true;
    if (pc->current_arm || pc->current_arm_2) {
      // Force EPOLLHUP callback(s)
      shutdown(pc->psocket.sockfd, SHUT_RDWR);
    }

    pn_connection_driver_close(&pc->driver);
    if (ptimer_shutdown(&pc->timer, pc->timer_armed))
      pc->timer_armed = false;  // disarmed in the sense that the timer will never fire again
    else if (!pc->timer_armed) {
      // In doubt.  One last callback to collect
      rearm(pc->psocket.proactor, &pc->timer.epoll_io);
      pc->timer_armed = true;
    }
  }
}

static void pconnection_forced_shutdown(pconnection_t *pc) {
  // Called by proactor_free, no competing threads, no epoll activity.
  pc->current_arm = 0;
  pc->new_events = 0;
  pc->current_arm_2 = 0;
  pc->new_events_2 = 0;
  pconnection_begin_close(pc);
  // pconnection_process will never be called again.  Zero everything.
  pc->timer_armed = false;
  pc->context.wake_ops = 0;
  pn_connection_t *c = pc->driver.connection;
  pn_collector_release(pn_connection_collector(c));
  assert(pconnection_is_final(pc));
  pconnection_cleanup(pc);
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (!pc->driver.connection) return NULL;
  pn_event_t *e = pn_connection_driver_next_event(&pc->driver);
  if (!e) {
    write_flush(pc);  // May generate transport event
    e = pn_connection_driver_next_event(&pc->driver);
    if (!e && pc->hog_count < HOG_MAX) {
      if (pconnection_process(pc, 0, false, true, false)) {
        e = pn_connection_driver_next_event(&pc->driver);
      }
    }
  }
  return e;
}

/* Shortcuts */
static inline bool pconnection_rclosed(pconnection_t  *pc) {
  return pn_connection_driver_read_closed(&pc->driver);
}

static inline bool pconnection_wclosed(pconnection_t  *pc) {
  return pn_connection_driver_write_closed(&pc->driver);
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
  if (pc->current_arm && pc->current_arm_2) return false;  // Maxed out
  if (pconnection_rclosed(pc) && pconnection_wclosed(pc)) {
    return false;
  }
  uint32_t wanted_now = (pc->read_blocked && !pconnection_rclosed(pc)) ? EPOLLIN : 0;
  if (!pconnection_wclosed(pc)) {
    if (pc->write_blocked)
      wanted_now |= EPOLLOUT;
    else {
      pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
      if (wbuf.size > 0)
        wanted_now |= EPOLLOUT;
    }
  }
  if (!wanted_now) return false;

  uint32_t have_now = pc->current_arm ?  pc->current_arm : pc->current_arm_2;
  uint32_t needed = wanted_now & ~have_now;
  if (!needed) return false;

  lock(&pc->rearm_mutex);      /* unlocked in pconnection_rearm... */
  // Always favour main epollfd
  if (!pc->current_arm) {
    pc->current_arm = pc->psocket.epoll_io.wanted = needed;
    pc->rearm_target = &pc->psocket.epoll_io;
  } else {
    pc->current_arm_2 = pc->epoll_io_2.wanted = needed;
    pc->rearm_target = &pc->epoll_io_2;
  }
  return true;                     /* ... so caller MUST call pconnection_rearm */
}

/* Call without lock */
static inline void pconnection_rearm(pconnection_t *pc) {
  if (pc->rearm_target == &pc->psocket.epoll_io) {
    rearm(pc->psocket.proactor, pc->rearm_target);
  } else {
    rearm_2(pc->psocket.proactor, pc->rearm_target);
  }
  pc->rearm_target = NULL;
  unlock(&pc->rearm_mutex);
  // Return immediately.  pc may have just been freed by another thread.
}

static inline bool pconnection_work_pending(pconnection_t *pc) {
  if (pc->new_events || pc->new_events_2 || pc->wake_count || pc->tick_pending || pc->queued_disconnect)
    return true;
  if (!pc->read_blocked && !pconnection_rclosed(pc))
    return true;
  pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
  return (wbuf.size > 0 && !pc->write_blocked);
}

static void pconnection_done(pconnection_t *pc) {
  bool notify = false;
  lock(&pc->context.mutex);
  pc->context.working = false;  // So we can wake() ourself if necessary.  We remain the de facto
                                // working context while the lock is held.
  pc->hog_count = 0;
  if (pconnection_has_event(pc) || pconnection_work_pending(pc)) {
    notify = wake(&pc->context);
  } else if (pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->context.mutex);
      pconnection_cleanup(pc);
      return;
    }
  }
  bool rearm = pconnection_rearm_check(pc);
  unlock(&pc->context.mutex);
  if (notify) wake_notify(&pc->context);
  if (rearm) pconnection_rearm(pc);  // May free pc on another thread.  Return.
  return;
}

// Return true unless error
static bool pconnection_write(pconnection_t *pc, pn_bytes_t wbuf) {
  ssize_t n = send(pc->psocket.sockfd, wbuf.start, wbuf.size, MSG_NOSIGNAL);
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

static void write_flush(pconnection_t *pc) {
  if (!pc->write_blocked && !pconnection_wclosed(pc)) {
    pn_bytes_t wbuf = pn_connection_driver_write_buffer(&pc->driver);
    if (wbuf.size > 0) {
      if (!pconnection_write(pc, wbuf)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "disconnected" : "on write to");
      }
    }
    else {
      if (pn_connection_driver_write_closed(&pc->driver)) {
        shutdown(pc->psocket.sockfd, SHUT_WR);
        pc->write_blocked = true;
      }
    }
  }
}

static void pconnection_connected_lh(pconnection_t *pc);
static void pconnection_maybe_connect_lh(pconnection_t *pc);

/*
 * May be called concurrently from multiple threads:
 *   pn_event_batch_t loop (topup is true)
 *   timer (timeout is true)
 *   socket io (events != 0) from PCONNECTION_IO
 *      and PCONNECTION_IO_2 event masks (possibly simultaneously)
 *   one or more wake()
 * Only one thread becomes (or always was) the working thread.
 */
static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool timeout, bool topup, bool is_io_2) {
  bool inbound_wake = !(events | timeout | topup);
  bool rearm_timer = false;
  bool timer_fired = false;
  bool waking = false;
  bool tick_required = false;

  // Don't touch data exclusive to working thread (yet).

  if (timeout) {
    rearm_timer = true;
    timer_fired = ptimer_callback(&pc->timer) != 0;
  }
  lock(&pc->context.mutex);

  if (events) {
    if (is_io_2)
      pc->new_events_2 = events;
    else
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

  if (rearm_timer)
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

 retry:

  if (pc->queued_disconnect) {  // From pn_proactor_disconnect()
    pc->queued_disconnect = false;
    if (!pc->context.closing) {
      if (pc->disconnect_condition) {
        pn_condition_copy(pn_transport_condition(pc->driver.transport), pc->disconnect_condition);
      }
      pn_connection_driver_close(&pc->driver);
    }
  }

  if (pconnection_has_event(pc)) {
    unlock(&pc->context.mutex);
    return &pc->batch;
  }
  bool closed = pconnection_rclosed(pc) && pconnection_wclosed(pc);
  if (pc->wake_count) {
    waking = !closed;
    pc->wake_count = 0;
  }
  if (pc->tick_pending) {
    pc->tick_pending = false;
    tick_required = !closed;
  }

  uint32_t update_events = 0;
  if (pc->new_events) {
    update_events = pc->new_events;
    pc->current_arm = 0;
    pc->new_events = 0;
  }
  if (pc->new_events_2) {
    update_events |= pc->new_events_2;
    pc->current_arm_2 = 0;
    pc->new_events_2 = 0;
  }
  if (update_events) {
    if (!pc->context.closing) {
      if ((update_events & (EPOLLHUP | EPOLLERR)) && !pconnection_rclosed(pc) && !pconnection_wclosed(pc))
        pconnection_maybe_connect_lh(pc);
      else
        pconnection_connected_lh(pc); /* Non error event means we are connected */
      if (update_events & EPOLLOUT)
        pc->write_blocked = false;
      if (update_events & EPOLLIN)
        pc->read_blocked = false;
    }
  }

  if (pc->context.closing && pconnection_is_final(pc)) {
    unlock(&pc->context.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

  unlock(&pc->context.mutex);
  pc->hog_count++; // working context doing work

  if (waking) {
    pn_connection_t *c = pc->driver.connection;
    pn_collector_put(pn_connection_collector(c), PN_OBJECT, c, PN_CONNECTION_WAKE);
    waking = false;
  }

  // read... tick... write
  // perhaps should be: write_if_recent_EPOLLOUT... read... tick... write

  if (!pconnection_rclosed(pc)) {
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    if (rbuf.size > 0 && !pc->read_blocked) {
      ssize_t n = read(pc->psocket.sockfd, rbuf.start, rbuf.size);

      if (n > 0) {
        pn_connection_driver_read_done(&pc->driver, n);
        pconnection_tick(pc);         /* check for tick changes. */
        tick_required = false;
        if (!pn_connection_driver_read_closed(&pc->driver) && (size_t)n < rbuf.size)
          pc->read_blocked = true;
      }
      else if (n == 0) {
        pn_connection_driver_read_close(&pc->driver);
      }
      else if (errno == EWOULDBLOCK)
        pc->read_blocked = true;
      else if (!(errno == EAGAIN || errno == EINTR)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "disconnected" : "on read from");
      }
    }
  }

  if (tick_required) {
    pconnection_tick(pc);         /* check for tick changes. */
    tick_required = false;
  }

  if (topup) {
    // If there was anything new to topup, we have it by now.
    return NULL;  // caller already owns the batch
  }

  if (pconnection_has_event(pc)) {
    return &pc->batch;
  }

  write_flush(pc);

  lock(&pc->context.mutex);
  if (pc->context.closing && pconnection_is_final(pc)) {
    unlock(&pc->context.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

  // Never stop working while work remains.  hog_count exception to this rule is elsewhere.
  if (pconnection_work_pending(pc))
    goto retry;  // TODO: get rid of goto without adding more locking

  pc->context.working = false;
  pc->hog_count = 0;
  if (pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->context.mutex);
      pconnection_cleanup(pc);
      return NULL;
    }
  }

  if (!pc->timer_armed && !pc->timer.shutting_down && pc->timer.timerfd >= 0) {
    pc->timer_armed = true;
    rearm(pc->psocket.proactor, &pc->timer.epoll_io);
  }
  bool rearm_pc = pconnection_rearm_check(pc);  // holds rearm_mutex until pconnection_rearm() below

  unlock(&pc->context.mutex);
  if (rearm_pc) pconnection_rearm(pc);  // May free pc on another thread.  Return right away.
  return NULL;
}

static void configure_socket(int sock) {
  int flags = fcntl(sock, F_GETFL);
  flags |= O_NONBLOCK;
  (void)fcntl(sock, F_SETFL, flags); // TODO: check for error

  int tcp_nodelay = 1;
  (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*) &tcp_nodelay, sizeof(tcp_nodelay));
}

/* Called with context.lock held */
void pconnection_connected_lh(pconnection_t *pc) {
  if (!pc->connected) {
    pc->connected = true;
    if (pc->addrinfo) {
      freeaddrinfo(pc->addrinfo);
      pc->addrinfo = NULL;
    }
    pc->ai = NULL;
    socklen_t len = sizeof(pc->remote.ss);
    (void)getpeername(pc->psocket.sockfd, (struct sockaddr*)&pc->remote.ss, &len);
  }
}

/* multi-address connections may call pconnection_start multiple times with diffferent FDs  */
static void pconnection_start(pconnection_t *pc) {
  int efd = pc->psocket.proactor->epollfd;
  /* Start timer, a no-op if the timer has already started. */
  start_polling(&pc->timer.epoll_io, efd);  // TODO: check for error

  /* Get the local socket name now, get the peer name in pconnection_connected */
  socklen_t len = sizeof(pc->local.ss);
  (void)getsockname(pc->psocket.sockfd, (struct sockaddr*)&pc->local.ss, &len);

  epoll_extended_t *ee = &pc->psocket.epoll_io;
  if (ee->polling) {     /* This is not the first attempt, stop polling and close the old FD */
    int fd = ee->fd;     /* Save fd, it will be set to -1 by stop_polling */
    stop_polling(ee, efd);
    pclosefd(pc->psocket.proactor, fd);
  }
  ee->fd = pc->psocket.sockfd;
  pc->current_arm = ee->wanted = EPOLLIN | EPOLLOUT;
  start_polling(ee, efd);  // TODO: check for error
}

/* Called on initial connect, and if connection fails to try another address */
static void pconnection_maybe_connect_lh(pconnection_t *pc) {
  errno = 0;
  if (!pc->connected) {         /* Not yet connected */
    while (pc->ai) {            /* Have an address */
      struct addrinfo *ai = pc->ai;
      pc->ai = pc->ai->ai_next; /* Move to next address in case this fails */
      int fd = socket(ai->ai_family, SOCK_STREAM, 0);
      if (fd >= 0) {
        configure_socket(fd);
        if (!connect(fd, ai->ai_addr, ai->ai_addrlen) || errno == EINPROGRESS) {
          pc->psocket.sockfd = fd;
          pconnection_start(pc);
          return;               /* Async connection started */
        } else {
          close(fd);
        }
      }
      /* connect failed immediately, go round the loop to try the next addr */
    }
    freeaddrinfo(pc->addrinfo);
    pc->addrinfo = NULL;
    /* If there was a previous attempted connection, let the poller discover the
       errno from its socket, otherwise set the current error. */
    if (pc->psocket.sockfd < 1) {
      psocket_error(&pc->psocket, errno ? errno : ENOTCONN, "on connect");
    }
  }
  pc->disconnected = true;
}

static int pgetaddrinfo(const char *host, const char *port, int flags, struct addrinfo **res)
{
  struct addrinfo hints = { 0 };
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | flags;
  return getaddrinfo(host, port, &hints, res);
}

static inline bool is_inactive(pn_proactor_t *p) {
  return (!p->contexts && !p->disconnects_pending && !p->timeout_set && !p->shutting_down);
}

/* If inactive set need_inactive and return true if the proactor needs a wakeup */
static bool wake_if_inactive(pn_proactor_t *p) {
  if (is_inactive(p)) {
    p->need_inactive = true;
    return wake(&p->context);
  }
  return false;
}

void pn_proactor_connect2(pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, const char *addr) {
  pconnection_t *pc = (pconnection_t*) calloc(1, sizeof(pconnection_t));
  assert(pc); // TODO: memory safety
  const char *err = pconnection_setup(pc, p, c, t, false, addr);
  if (err) {    /* TODO aconway 2017-09-13: errors must be reported as events */
    pn_logf("pn_proactor_connect failure: %s", err);
    return;
  }
  // TODO: check case of proactor shutting down

  lock(&pc->context.mutex);
  proactor_add(&pc->context);
  pn_connection_open(pc->driver.connection); /* Auto-open */

  bool notify = false;
  bool notify_proactor = false;

  if (pc->disconnected) {
    notify = wake(&pc->context);    /* Error during initialization */
  } else {
    int gai_error = pgetaddrinfo(pc->psocket.host, pc->psocket.port, 0, &pc->addrinfo);
    if (!gai_error) {
      pn_connection_open(pc->driver.connection); /* Auto-open */
      pc->ai = pc->addrinfo;
      pconnection_maybe_connect_lh(pc); /* Start connection attempts */
      if (pc->disconnected) notify = wake(&pc->context);
    } else {
      psocket_gai_error(&pc->psocket, gai_error, "connect to ");
      notify = wake(&pc->context);
      lock(&p->context.mutex);
      notify_proactor = wake_if_inactive(p);
      unlock(&p->context.mutex);
    }
  }
  /* We need to issue INACTIVE on immediate failure */
  unlock(&pc->context.mutex);
  if (notify) wake_notify(&pc->context);
  if (notify_proactor) wake_notify(&p->context);
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
    if (!pc->context.closing) {
      pc->wake_count++;
      notify = wake(&pc->context);
    }
    unlock(&pc->context.mutex);
  }
  if (notify) wake_notify(&pc->context);
}

void pn_proactor_release_connection(pn_connection_t *c) {
  bool notify = false;
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    set_pconnection(c, NULL);
    lock(&pc->context.mutex);
    pn_connection_driver_release_connection(&pc->driver);
    pconnection_begin_close(pc);
    notify = wake(&pc->context);
    unlock(&pc->context.mutex);
  }
  if (notify) wake_notify(&pc->context);
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
    pmutex_init(&l->rearm_mutex);
  }
  return l;
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog)
{
  // TODO: check listener not already listening for this or another proactor
  lock(&l->context.mutex);
  l->context.proactor = p;;
  l->backlog = backlog;

  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
  pni_parse_addr(addr, addr_buf, PN_MAX_ADDR, &host, &port);

  struct addrinfo *addrinfo = NULL;
  int gai_err = pgetaddrinfo(host, port, AI_PASSIVE | AI_ALL, &addrinfo);
  if (!gai_err) {
    /* Count addresses, allocate enough space for sockets */
    size_t len = 0;
    for (struct addrinfo *ai = addrinfo; ai; ai = ai->ai_next) {
      ++len;
    }
    assert(len > 0);            /* guaranteed by getaddrinfo */
    l->acceptors = (acceptor_t*)calloc(len, sizeof(acceptor_t));
    assert(l->acceptors);      /* TODO aconway 2017-05-05: memory safety */
    l->acceptors_size = 0;
    uint16_t dynamic_port = 0;  /* Record dynamic port from first bind(0) */
    /* Find working listen addresses */
    for (struct addrinfo *ai = addrinfo; ai; ai = ai->ai_next) {
      if (dynamic_port) set_port(ai->ai_addr, dynamic_port);
      int fd = socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol);
      static int on = 1;
      if (fd >= 0) {
        if (!setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) &&
            /* We listen to v4/v6 on separate sockets, don't let v6 listen for v4 */
            (ai->ai_family != AF_INET6 ||
             !setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on))) &&
            !bind(fd, ai->ai_addr, ai->ai_addrlen) &&
            !listen(fd, backlog))
        {
          acceptor_t *acceptor = &l->acceptors[l->acceptors_size++];
          /* Get actual address */
          socklen_t len = pn_netaddr_socklen(&acceptor->addr);
          (void)getsockname(fd, (struct sockaddr*)(&acceptor->addr.ss), &len);
          if (acceptor == l->acceptors) { /* First acceptor, check for dynamic port */
            dynamic_port = check_dynamic_port(ai->ai_addr, pn_netaddr_sockaddr(&acceptor->addr));
          } else {              /* Link addr to previous addr */
            (acceptor-1)->addr.next = &acceptor->addr;
          }

          acceptor->accepted_fd = -1;
          psocket_t *ps = &acceptor->psocket;
          psocket_init(ps, p, l, addr);
          ps->sockfd = fd;
          ps->epoll_io.fd = fd;
          ps->epoll_io.wanted = EPOLLIN;
          ps->epoll_io.polling = false;
          lock(&l->rearm_mutex);
          start_polling(&ps->epoll_io, ps->proactor->epollfd);  // TODO: check for error
          l->active_count++;
          acceptor->armed = true;
          unlock(&l->rearm_mutex);
        } else {
          close(fd);
        }
      }
    }
  }
  if (addrinfo) {
    freeaddrinfo(addrinfo);
  }
  bool notify = wake(&l->context);

  if (l->acceptors_size == 0) { /* All failed, create dummy socket with an error */
    l->acceptors = (acceptor_t*)realloc(l->acceptors, sizeof(acceptor_t));
    l->acceptors_size = 1;
    memset(l->acceptors, 0, sizeof(acceptor_t));
    psocket_init(&l->acceptors[0].psocket, p, l, addr);
    l->acceptors[0].accepted_fd = -1;
    if (gai_err) {
      psocket_gai_error(&l->acceptors[0].psocket, gai_err, "listen on");
    } else {
      psocket_error(&l->acceptors[0].psocket, errno, "listen on");
    }
  } else {
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_OPEN);
  }
  proactor_add(&l->context);
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
  return;
}

// call with lock held and context.working false
static inline bool listener_can_free(pn_listener_t *l) {
  return l->context.closing && l->close_dispatched && !l->context.wake_ops && !l->active_count;
}

static inline void listener_final_free(pn_listener_t *l) {
  pcontext_finalize(&l->context);
  pmutex_finalize(&l->rearm_mutex);
  free(l->acceptors);
  free(l);
}

void pn_listener_free(pn_listener_t *l) {
  /* Note at this point either the listener has never been used (freed by user)
     or it has been closed, so all its sockets are closed.
  */
  if (l) {
    bool can_free = true;
    if (l->collector) pn_collector_free(l->collector);
    if (l->condition) pn_condition_free(l->condition);
    if (l->attachments) pn_free(l->attachments);
    lock(&l->context.mutex);
    if (l->context.proactor) {
      can_free = proactor_remove(&l->context);
    }
    unlock(&l->context.mutex);
    if (can_free)
      listener_final_free(l);
  }
}

/* Always call with lock held so it can be unlocked around overflow processing. */
static void listener_begin_close(pn_listener_t* l) {
  if (!l->context.closing) {
    l->context.closing = true;

    /* Close all listening sockets */
    for (size_t i = 0; i < l->acceptors_size; ++i) {
      acceptor_t *a = &l->acceptors[i];
      psocket_t *ps = &a->psocket;
      if (ps->sockfd >= 0) {
        lock(&l->rearm_mutex);
        if (a->armed) {
          shutdown(ps->sockfd, SHUT_RD);  // Force epoll event and callback
        } else {
          stop_polling(&ps->epoll_io, ps->proactor->epollfd);
          close(ps->sockfd);
          ps->sockfd = -1;
          l->active_count--;
        }
        unlock(&l->rearm_mutex);
      }
    }
    /* Close all sockets waiting for a pn_listener_accept2() */
    if (l->unclaimed) l->pending_count++;
    acceptor_t *a = listener_list_next(&l->pending_acceptors);
    while (a) {
      close(a->accepted_fd);
      a->accepted_fd = -1;
      l->pending_count--;
      a = listener_list_next(&l->pending_acceptors);
    }
    assert(!l->pending_count);

    unlock(&l->context.mutex);
    /* Remove all acceptors from the overflow list.  closing flag prevents re-insertion.*/
    proactor_rearm_overflow(pn_listener_proactor(l));
    lock(&l->context.mutex);
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_CLOSE);
  }
}

void pn_listener_close(pn_listener_t* l) {
  bool notify = false;
  lock(&l->context.mutex);
  if (!l->context.closing) {
    listener_begin_close(l);
    notify = wake(&l->context);
  }
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}

static void listener_forced_shutdown(pn_listener_t *l) {
  // Called by proactor_free, no competing threads, no epoll activity.
  lock(&l->context.mutex); // needed because of interaction with proactor_rearm_overflow
  listener_begin_close(l);
  unlock(&l->context.mutex);
  // pconnection_process will never be called again.  Zero everything.
  l->context.wake_ops = 0;
  l->close_dispatched = true;
  l->active_count = 0;
  assert(listener_can_free(l));
  pn_listener_free(l);
}

/* Accept a connection as part of listener_process(). Called with listener context lock held. */
static void listener_accept_lh(psocket_t *ps) {
  pn_listener_t *l = psocket_listener(ps);
  acceptor_t *acceptor = psocket_acceptor(ps);
  assert(acceptor->accepted_fd < 0); /* Shouldn't already have an accepted_fd */
  acceptor->accepted_fd = accept(ps->sockfd, NULL, 0);
  if (acceptor->accepted_fd >= 0) {
    //    acceptor_t *acceptor = listener_list_next(pending_acceptors);
    listener_list_append(&l->pending_acceptors, acceptor);
    l->pending_count++;
  } else {
    int err = errno;
    if (err == ENFILE || err == EMFILE) {
      listener_set_overflow(acceptor);
    } else {
      psocket_error(ps, err, "accept");
    }
  }
}

/* Process a listening socket */
static pn_event_batch_t *listener_process(psocket_t *ps, uint32_t events) {
  // TODO: some parallelization of the accept mechanism.
  pn_listener_t *l = psocket_listener(ps);
  acceptor_t *a = psocket_acceptor(ps);
  lock(&l->context.mutex);
  if (events) {
    a->armed = false;
    if (l->context.closing) {
      lock(&l->rearm_mutex);
      stop_polling(&ps->epoll_io, ps->proactor->epollfd);
      unlock(&l->rearm_mutex);
      close(ps->sockfd);
      ps->sockfd = -1;
      l->active_count--;
    }
    else {
      if (events & EPOLLRDHUP) {
        /* Calls listener_begin_close which closes all the listener's sockets */
        psocket_error(ps, errno, "listener epoll");
      } else if (!l->context.closing && events & EPOLLIN) {
        listener_accept_lh(ps);
      }
    }
  } else {
    wake_done(&l->context); // callback accounting
  }
  pn_event_batch_t *lb = NULL;
  if (!l->context.working) {
    l->context.working = true;
    if (listener_has_event(l))
      lb = &l->batch;
    else {
      l->context.working = false;
      if (listener_can_free(l)) {
        unlock(&l->context.mutex);
        pn_listener_free(l);
        return NULL;
      }
    }
  }
  unlock(&l->context.mutex);
  return lb;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  lock(&l->context.mutex);
  pn_event_t *e = pn_collector_next(l->collector);
  if (!e && l->pending_count && !l->unclaimed) {
    // empty collector means pn_collector_put() will not coalesce
    pn_collector_put(l->collector, pn_listener__class(), l, PN_LISTENER_ACCEPT);
    l->unclaimed = true;
    l->pending_count--;
    e = pn_collector_next(l->collector);
  }
  if (e && pn_event_type(e) == PN_LISTENER_CLOSE)
    l->close_dispatched = true;
  unlock(&l->context.mutex);
  return log_event(l, e);
}

static void listener_done(pn_listener_t *l) {
  bool notify = false;
  lock(&l->context.mutex);
  l->context.working = false;

  if (listener_can_free(l)) {
    unlock(&l->context.mutex);
    pn_listener_free(l);
    return;
  } else if (listener_has_event(l))
    notify = wake(&l->context);
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->acceptors[0].psocket.proactor : NULL;
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

void pn_listener_accept2(pn_listener_t *l, pn_connection_t *c, pn_transport_t *t) {
  pconnection_t *pc = (pconnection_t*) calloc(1, sizeof(pconnection_t));
  assert(pc); // TODO: memory safety
  const char *err = pconnection_setup(pc, pn_listener_proactor(l), c, t, true, "");
  if (err) {
    pn_logf("pn_listener_accept failure: %s", err);
    return;
  }
  // TODO: fuller sanity check on input args

  int err2 = 0;
  int fd = -1;
  psocket_t *rearming_ps = NULL;
  bool notify = false;
  lock(&l->context.mutex);
  if (l->context.closing)
    err2 = EBADF;
  else if (l->unclaimed) {
    l->unclaimed = false;
    acceptor_t *a = listener_list_next(&l->pending_acceptors);
    assert(a);
    assert(!a->armed);
    fd = a->accepted_fd;
    a->accepted_fd = -1;
    lock(&l->rearm_mutex);
    rearming_ps = &a->psocket;
    a->armed = true;
  }
  else err2 = EWOULDBLOCK;

  proactor_add(&pc->context);
  lock(&pc->context.mutex);
  pc->psocket.sockfd = fd;
  if (fd >= 0) {
    configure_socket(fd);
    pconnection_start(pc);
    pconnection_connected_lh(pc);
  }
  else
    psocket_error(&pc->psocket, err2, "pn_listener_accept");
  if (!l->context.working && listener_has_event(l))
    notify = wake(&l->context);
  unlock(&pc->context.mutex);
  unlock(&l->context.mutex);
  if (rearming_ps) {
    rearm(rearming_ps->proactor, &rearming_ps->epoll_io);
    unlock(&l->rearm_mutex);
  }
  if (notify) wake_notify(&l->context);
}


// ========================================================================
// proactor
// ========================================================================

/* Set up an epoll_extended_t to be used for wakeup or interrupts */
static void epoll_wake_init(epoll_extended_t *ee, int eventfd, int epollfd) {
  ee->psocket = NULL;
  ee->fd = eventfd;
  ee->type = WAKE;
  ee->wanted = EPOLLIN;
  ee->polling = false;
  start_polling(ee, epollfd);  // TODO: check for error
}

/* Set up the epoll_extended_t to be used for secondary socket events */
static void epoll_secondary_init(epoll_extended_t *ee, int epoll_fd_2, int epollfd) {
  ee->psocket = NULL;
  ee->fd = epoll_fd_2;
  ee->type = CHAINED_EPOLL;
  ee->wanted = EPOLLIN;
  ee->polling = false;
  start_polling(ee, epollfd);  // TODO: check for error
}

pn_proactor_t *pn_proactor() {
  pn_proactor_t *p = (pn_proactor_t*)calloc(1, sizeof(*p));
  if (!p) return NULL;
  p->epollfd = p->eventfd = p->timer.timerfd = -1;
  pcontext_init(&p->context, PROACTOR, p, p);
  pmutex_init(&p->eventfd_mutex);
  ptimer_init(&p->timer, 0);

  if ((p->epollfd = epoll_create(1)) >= 0 && (p->epollfd_2 = epoll_create(1)) >= 0) {
    if ((p->eventfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
      if ((p->interruptfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
        if (p->timer.timerfd >= 0)
          if ((p->collector = pn_collector()) != NULL) {
            p->batch.next_event = &proactor_batch_next;
            start_polling(&p->timer.epoll_io, p->epollfd);  // TODO: check for error
            p->timer_armed = true;
            epoll_wake_init(&p->epoll_wake, p->eventfd, p->epollfd);
            epoll_wake_init(&p->epoll_interrupt, p->interruptfd, p->epollfd);
            epoll_secondary_init(&p->epoll_secondary, p->epollfd_2, p->epollfd);
            return p;
          }
      }
    }
  }
  if (p->epollfd >= 0) close(p->epollfd);
  if (p->epollfd_2 >= 0) close(p->epollfd_2);
  if (p->eventfd >= 0) close(p->eventfd);
  if (p->interruptfd >= 0) close(p->interruptfd);
  ptimer_finalize(&p->timer);
  if (p->collector) pn_free(p->collector);
  free (p);
  return NULL;
}

void pn_proactor_free(pn_proactor_t *p) {
  //  No competing threads, not even a pending timer
  p->shutting_down = true;
  close(p->epollfd);
  p->epollfd = -1;
  close(p->epollfd_2);
  p->epollfd_2 = -1;
  close(p->eventfd);
  p->eventfd = -1;
  close(p->interruptfd);
  p->interruptfd = -1;
  ptimer_finalize(&p->timer);
  while (p->contexts) {
    pcontext_t *ctx = p->contexts;
    p->contexts = ctx->next;
    switch (ctx->type) {
     case PCONNECTION:
      pconnection_forced_shutdown(pcontext_pconnection(ctx));
      break;
     case LISTENER:
      listener_forced_shutdown(pcontext_listener(ctx));
      break;
     default:
      break;
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
  if (l) return l->acceptors[0].psocket.proactor;
  pn_connection_t *c = pn_event_connection(e);
  if (c) return pn_connection_proactor(c);
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

  if (p->need_timeout) {
    p->need_timeout = false;
    p->timeout_set = false;
    proactor_add_event(p, PN_PROACTOR_TIMEOUT);
    return true;
  }
  if (p->need_interrupt) {
    p->need_interrupt = false;
    proactor_add_event(p, PN_PROACTOR_INTERRUPT);
    return true;
  }
  if (p->need_inactive) {
    p->need_inactive = false;
    proactor_add_event(p, PN_PROACTOR_INACTIVE);
    return true;
  }
  return false;
}

static pn_event_t *proactor_batch_next(pn_event_batch_t *batch) {
  pn_proactor_t *p = batch_proactor(batch);
  lock(&p->context.mutex);
  proactor_update_batch(p);
  pn_event_t *e = pn_collector_next(p->collector);
  if (e && pn_event_type(e) == PN_PROACTOR_TIMEOUT)
    p->timeout_processed = true;
  unlock(&p->context.mutex);
  return log_event(p, e);
}

static pn_event_batch_t *proactor_process(pn_proactor_t *p, pn_event_type_t event) {
  bool timer_fired = (event == PN_PROACTOR_TIMEOUT) && ptimer_callback(&p->timer) != 0;
  lock(&p->context.mutex);
  if (event == PN_PROACTOR_INTERRUPT) {
    p->need_interrupt = true;
  } else if (event == PN_PROACTOR_TIMEOUT) {
    p->timer_armed = false;
    if (timer_fired && p->timeout_set) {
      p->need_timeout = true;
    }
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
  bool rearm_timer = !p->timer_armed && !p->timer.shutting_down;
  p->timer_armed = true;
  unlock(&p->context.mutex);
  if (rearm_timer)
    rearm(p, &p->timer.epoll_io);
  return NULL;
}

static pn_event_batch_t *proactor_chained_epoll_wait(pn_proactor_t *p) {
  // process one ready pconnection socket event from the secondary/chained epollfd_2
  struct epoll_event ev = {0};
  int n = epoll_wait(p->epollfd_2, &ev, 1, 0);
  if (n < 0) {
    if (errno != EINTR)
      perror("epoll_wait"); // TODO: proper log
  } else if (n > 0) {
    assert(n == 1);
    rearm(p, &p->epoll_secondary);
    epoll_extended_t *ee = (epoll_extended_t *) ev.data.ptr;
    memory_barrier(ee);
    assert(ee->type == PCONNECTION_IO_2);
    pconnection_t *pc = psocket_pconnection(ee->psocket);
    return pconnection_process(pc, ev.events, false, false, true);
  }
  rearm(p, &p->epoll_secondary);
  return NULL;
}

static void proactor_add(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  lock(&p->context.mutex);
  if (p->contexts) {
    p->contexts->prev = ctx;
    ctx->next = p->contexts;
  }
  p->contexts = ctx;
  unlock(&p->context.mutex);
}

// call with psocket's mutex held
// return true if safe for caller to free psocket
static bool proactor_remove(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  lock(&p->context.mutex);
  bool can_free = true;
  if (ctx->disconnecting) {
    // No longer on contexts list
    if (--ctx->disconnect_ops == 0) {
      --p->disconnects_pending;
    }
    else                  // procator_disconnect() still processing
      can_free = false;   // this psocket
  }
  else {
    // normal case
    if (ctx->prev)
      ctx->prev->next = ctx->next;
    else {
      p->contexts = ctx->next;
      ctx->next = NULL;
      if (p->contexts)
        p->contexts->prev = NULL;
    }
    if (ctx->next) {
      ctx->next->prev = ctx->prev;
    }
  }
  bool notify = wake_if_inactive(p);
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
  return can_free;
}

static pn_event_batch_t *process_inbound_wake(pn_proactor_t *p, epoll_extended_t *ee) {
  if  (ee->fd == p->interruptfd) {        /* Interrupts have their own dedicated eventfd */
    (void)read_uint64(p->interruptfd);
    rearm(p, &p->epoll_interrupt);
    return proactor_process(p, PN_PROACTOR_INTERRUPT);
  }
  pcontext_t *ctx = wake_pop_front(p);
  if (ctx) {
    switch (ctx->type) {
     case PROACTOR:
      return proactor_process(p, PN_EVENT_NONE);
     case PCONNECTION:
      return pconnection_process((pconnection_t *) ctx->owner, 0, false, false, false);
     case LISTENER:
      return listener_process(&((pn_listener_t *) ctx->owner)->acceptors[0].psocket, 0);
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
    struct epoll_event ev = {0};
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
    memory_barrier(ee);

    if (ee->type == WAKE) {
      batch = process_inbound_wake(p, ee);
    } else if (ee->type == PROACTOR_TIMER) {
      batch = proactor_process(p, PN_PROACTOR_TIMEOUT);
    } else if (ee->type == CHAINED_EPOLL) {
      batch = proactor_chained_epoll_wait(p);  // expect a PCONNECTION_IO_2
    } else {
      pconnection_t *pc = psocket_pconnection(ee->psocket);
      if (pc) {
        if (ee->type == PCONNECTION_IO) {
          batch = pconnection_process(pc, ev.events, false, false, false);
        } else {
          assert(ee->type == PCONNECTION_TIMER);
          batch = pconnection_process(pc, 0, true, false, false);
        }
      }
      else {
        // TODO: can any of the listener processing be parallelized like IOCP?
        batch = listener_process(ee->psocket, ev.events);
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
    bool rearm_timer = !p->timer_armed && !p->shutting_down;
    p->timer_armed = true;
    p->context.working = false;
    if (p->timeout_processed) {
      p->timeout_processed = false;
      if (wake_if_inactive(p))
        notify = true;
    }
    proactor_update_batch(p);
    if (proactor_has_event(p))
      if (wake(&p->context))
        notify = true;
    unlock(&p->context.mutex);
    if (notify)
      wake_notify(&p->context);
    if (rearm_timer)
      rearm(p, &p->timer.epoll_io);
    return;
  }
}

void pn_proactor_interrupt(pn_proactor_t *p) {
  if (p->interruptfd == -1)
    return;
  uint64_t increment = 1;
  if (write(p->interruptfd, &increment, sizeof(uint64_t)) != sizeof(uint64_t))
    EPOLL_FATAL("setting eventfd", errno);
}

void pn_proactor_set_timeout(pn_proactor_t *p, pn_millis_t t) {
  bool notify = false;
  lock(&p->context.mutex);
  p->timeout_set = true;
  if (t == 0) {
    ptimer_set(&p->timer, 0);
    p->need_timeout = true;
    notify = wake(&p->context);
  } else {
    ptimer_set(&p->timer, t);
  }
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
}

void pn_proactor_cancel_timeout(pn_proactor_t *p) {
  lock(&p->context.mutex);
  p->timeout_set = false;
  p->need_timeout = false;
  ptimer_set(&p->timer, 0);
  bool notify = wake_if_inactive(p);
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->psocket.proactor : NULL;
}

void pn_proactor_disconnect(pn_proactor_t *p, pn_condition_t *cond) {
  bool notify = false;

  lock(&p->context.mutex);
  // Move the whole contexts list into a disconnecting state
  pcontext_t *disconnecting_pcontexts = p->contexts;
  p->contexts = NULL;
  // First pass: mark each pcontext as disconnecting and update global pending count.
  pcontext_t *ctx = disconnecting_pcontexts;
  while (ctx) {
    ctx->disconnecting = true;
    ctx->disconnect_ops = 2;   // Second pass below and proactor_remove(), in any order.
    p->disconnects_pending++;
    ctx = ctx->next;
  }
  notify = wake_if_inactive(p);
  unlock(&p->context.mutex);
  if (!disconnecting_pcontexts) {
    if (notify) wake_notify(&p->context);
    return;
  }

  // Second pass: different locking, close the pcontexts, free them if !disconnect_ops
  pcontext_t *next = disconnecting_pcontexts;
  while (next) {
    ctx = next;
    next = ctx->next;           /* Save next pointer in case we free ctx */
    bool do_free = false;
    bool ctx_notify = true;
    pmutex *ctx_mutex = NULL;
    pconnection_t *pc = pcontext_pconnection(ctx);
    if (pc) {
      ctx_mutex = &pc->context.mutex;
      lock(ctx_mutex);
      if (!ctx->closing) {
        if (ctx->working) {
          // Must defer
          pc->queued_disconnect = true;
          if (cond) {
            if (!pc->disconnect_condition)
              pc->disconnect_condition = pn_condition();
            pn_condition_copy(pc->disconnect_condition, cond);
          }
        }
        else {
          // No conflicting working context.
          if (cond) {
            pn_condition_copy(pn_transport_condition(pc->driver.transport), cond);
          }
          pn_connection_driver_close(&pc->driver);
        }
      }
    } else {
      pn_listener_t *l = pcontext_listener(ctx);
      assert(l);
      ctx_mutex = &l->context.mutex;
      lock(ctx_mutex);
      if (!ctx->closing) {
        if (cond) {
          pn_condition_copy(pn_listener_condition(l), cond);
        }
        listener_begin_close(l);
      }
    }

    lock(&p->context.mutex);
    if (--ctx->disconnect_ops == 0) {
      do_free = true;
      ctx_notify = false;
      notify = wake_if_inactive(p);
    } else {
      // If initiating the close, wake the pcontext to do the free.
      if (ctx_notify)
        ctx_notify = wake(ctx);
    }
    unlock(&p->context.mutex);
    unlock(ctx_mutex);

    if (do_free) {
      if (pc) pconnection_final_free(pc);
      else listener_final_free(pcontext_listener(ctx));
    } else {
      if (ctx_notify)
        wake_notify(ctx);
    }
  }
  if (notify)
    wake_notify(&p->context);
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
  return l->acceptors_size > 0 ? &l->acceptors[0].addr : NULL;
}

pn_millis_t pn_proactor_now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec*1000 + t.tv_nsec/1000000;
}
