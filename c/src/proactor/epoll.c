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

/*
 The epoll proactor works with multiple concurrent threads.  If there is no work to do,
 one thread temporarily becomes the poller thread, while other inbound threads suspend
 waiting for work.  The poller calls epoll_wait(), generates work lists, resumes suspended
 threads, and grabs work for itself or polls again.

 A serialized grouping of Proton events is a context (connection, listener, proactor).
 Each has multiple pollable fds that make it schedulable.  E.g. a connection could have a
 socket fd, timerfd, and (indirect) eventfd all signaled in a single epoll_wait().

 At the conclusion of each
      N = epoll_wait(..., N_MAX, timeout)

 there will be N epoll events and M wakes on the wake list.  M can be very large in a
 server with many active connections. The poller makes the contexts "runnable" if they are
 not already running.  A running context can only be made runnable once until it completes
 a chunk of work and calls unassign_thread().  (N + M - duplicates) contexts will be
 scheduled.  A new poller will occur when next_runnable() returns NULL.

 A running context, before it stops "working" must check to see if there were new incoming
 events that the poller posted to the context, but could not make it runnable since it was
 already running.  The context will know if it needs to put itself back on the wake list
 to be runnable later to process the pending events.

 Lock ordering - never add locks right to left:
    context -> sched -> wake
    ctimerq -> pconnection
    non-proactor-context -> proactor-context
    tslot -> sched
 */


/* Enable POSIX features beyond c99 for modern pthread and standard strerror_r() */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* Avoid GNU extensions, in particular the incompatible alternative strerror_r() */
#undef _GNU_SOURCE

#include "epoll-internal.h"
#include "proactor-internal.h"
#include "core/engine-internal.h"
#include "core/logger_private.h"
#include "core/util.h"

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/engine.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/raw_connection.h>

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
#include <alloca.h>

#include "./netaddr-internal.h" /* Include after socket/inet headers */

// TODO: replace timerfd per connection with global lightweight timer mechanism.
// logging in general
// SIGPIPE?
// Can some of the mutexes be spinlocks (any benefit over adaptive pthread mutex)?
//   Maybe futex is even better?
// See other "TODO" in code.
//
// Consider case of large number of wakes: next_event_batch() could start by
// looking for pending wakes before a kernel call to epoll_wait(), or there
// could be several eventfds with random assignment of wakeables.


/* Like strerror_r but provide a default message if strerror_r fails */
void pstrerror(int err, strerrorbuf msg) {
  int e = strerror_r(err, msg, sizeof(strerrorbuf));
  if (e) snprintf(msg, sizeof(strerrorbuf), "unknown error %d", err);
}

// ========================================================================
// First define a proactor mutex (pmutex) and timer mechanism (ptimer) to taste.
// ========================================================================

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
 *
 * TODO: review above in light of single poller thread.
 */

bool ptimer_init(ptimer_t *pt, epoll_type_t ep_type) {
  pmutex_init(&pt->mutex);
  pt->timer_active = false;
  pt->epoll_io.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  pt->epoll_io.type = ep_type;
  pt->epoll_io.wanted = EPOLLIN;
  pt->epoll_io.polling = false;
  return (pt->epoll_io.fd >= 0);
}

// Call with ptimer lock held
static void ptimer_set_lh(ptimer_t *pt, uint64_t t_millis) {
  struct itimerspec newt, oldt;
  memset(&newt, 0, sizeof(newt));
  newt.it_value.tv_sec = t_millis / 1000;
  newt.it_value.tv_nsec = (t_millis % 1000) * 1000000;

  timerfd_settime(pt->epoll_io.fd, 0, &newt, &oldt);
  pt->timer_active = t_millis != 0;
}

void ptimer_set(ptimer_t *pt, uint64_t t_millis) {
  // t_millis == 0 -> cancel
  lock(&pt->mutex);
  if (t_millis == 0 && !pt->timer_active) {
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
bool ptimer_callback(ptimer_t *pt) {
  lock(&pt->mutex);
  struct itimerspec current;
  if (timerfd_gettime(pt->epoll_io.fd, &current) == 0) {
    if (current.it_value.tv_nsec == 0 && current.it_value.tv_sec == 0)
      pt->timer_active = false;
  }
  uint64_t u_exp_count = read_uint64(pt->epoll_io.fd);
  unlock(&pt->mutex);
  return u_exp_count > 0;
}

void ptimer_finalize(ptimer_t *pt) {
  if (pt->epoll_io.fd >= 0) close(pt->epoll_io.fd);
  pmutex_finalize(&pt->mutex);
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
PN_STRUCT_CLASSDEF(pn_proactor)
PN_STRUCT_CLASSDEF(pn_listener)

bool start_polling(epoll_extended_t *ee, int epollfd) {
  if (ee->polling)
    return false;
  ee->polling = true;
  struct epoll_event ev = {0};
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  memory_barrier(ee);
  return (epoll_ctl(epollfd, EPOLL_CTL_ADD, ee->fd, &ev) == 0);
}

void stop_polling(epoll_extended_t *ee, int epollfd) {
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

void rearm_polling(epoll_extended_t *ee, int epollfd) {
  struct epoll_event ev = {0};
  ev.data.ptr = ee;
  ev.events = ee->wanted | EPOLLONESHOT;
  memory_barrier(ee);
  if (epoll_ctl(epollfd, EPOLL_CTL_MOD, ee->fd, &ev) == -1)
    EPOLL_FATAL("arming polled file descriptor", errno);
}

static void rearm(pn_proactor_t *p, epoll_extended_t *ee) {
  rearm_polling(ee, p->epollfd);
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


// Fake thread for temporarily disabling the scheduling of a context.
static struct tslot_t *REWAKE_PLACEHOLDER = (struct tslot_t*) -1;

void pcontext_init(pcontext_t *ctx, pcontext_type_t t, pn_proactor_t *p) {
  memset(ctx, 0, sizeof(*ctx));
  pmutex_init(&ctx->mutex);
  ctx->proactor = p;
  ctx->type = t;
}

static void pcontext_finalize(pcontext_t* ctx) {
  pmutex_finalize(&ctx->mutex);
}

/*
 * Wake strategy with eventfd.
 *  - wakees can be in the list only once
 *  - wakers only use the eventfd if wakes_in_progress is false
 * There is a single rearm between wakes > 0 and wakes == 0
 *
 * There can potentially be many contexts with wakes pending.
 *
 * The wake list is in two parts.  The front is the chunk the
 * scheduler will process until the next epoll_wait().  sched_wake
 * indicates which chunk it is on. The ctx may already be running or
 * scheduled to run.
 *
 * The ctx must be actually running to absorb ctx->wake_pending.
 *
 * The wake list can keep growing while popping wakes.  The list between
 * sched_wake_first and sched_wake_last are protected by the sched
 * lock (for pop operations), sched_wake_last to wake_list_last are
 * protected by the eventfd mutex (for add operations).  Both locks
 * are needed to cross or reconcile the two portions of the list.
 */

// Call with sched lock held.
static void pop_wake(pcontext_t *ctx) {
  // every context on the sched_wake_list is either currently running,
  // or to be scheduled.  wake() will not "see" any of the wake_next
  // pointers until wake_pending and working have transitioned to 0
  // and false, when a context stops working.
  //
  // every context must transition as:
  //
  // !wake_pending .. wake() .. on wake_list .. on sched_wake_list .. working context .. !sched_wake && !wake_pending
  //
  // Intervening locks at each transition ensures wake_next has memory coherence throughout the wake cycle.
  pn_proactor_t *p = ctx->proactor;
  if (ctx == p->sched_wake_current)
    p->sched_wake_current = ctx->wake_next;
  if (ctx == p->sched_wake_first) {
    // normal code path
    if (ctx == p->sched_wake_last) {
      p->sched_wake_first = p->sched_wake_last = NULL;
    } else {
      p->sched_wake_first = ctx->wake_next;
    }
    if (!p->sched_wake_first)
      p->sched_wake_last = NULL;
  } else {
    // ctx is not first in a multi-element list
    pcontext_t *prev = NULL;
    for (pcontext_t *i = p->sched_wake_first; i != ctx; i = i->wake_next)
      prev = i;
    prev->wake_next = ctx->wake_next;
    if (ctx == p->sched_wake_last)
      p->sched_wake_last = prev;
  }
  ctx->on_wake_list = false;
}

// part1: call with ctx->owner lock held, return true if notify required by caller
// Note that this will return false if either there is a pending wake OR if we are already
// in the connection context that is to be woken (as we don't have to wake it up)
bool wake(pcontext_t *ctx) {
  bool notify = false;

  if (!ctx->wake_pending) {
    if (!ctx->working) {
      ctx->wake_pending = true;
      pn_proactor_t *p = ctx->proactor;
      lock(&p->eventfd_mutex);
      ctx->wake_next = NULL;
      ctx->on_wake_list = true;
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
void wake_notify(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  if (p->eventfd == -1)
    return;
  rearm(p, &p->epoll_wake);
}

// call with owner lock held, once for each pop from the wake list
void wake_done(pcontext_t *ctx) {
//  assert(ctx->wake_pending > 0);
  ctx->wake_pending = false;
}


/*
 * Scheduler/poller
*/

// Internal use only
/* How long to defer suspending */
static int pni_spins = 0;
/* Prefer immediate running by poller over warm running by suspended thread */
static bool pni_immediate = false;
/* Toggle use of warm scheduling */
static int pni_warm_sched = true;


// Call with sched lock
static void suspend_list_add_tail(pn_proactor_t *p, tslot_t *ts) {
  LL_ADD(p, suspend_list, ts);
}

// Call with sched lock
static void suspend_list_insert_head(pn_proactor_t *p, tslot_t *ts) {
  ts->suspend_list_next = p->suspend_list_head;
  ts->suspend_list_prev = NULL;
  if (p->suspend_list_head)
    p->suspend_list_head->suspend_list_prev = ts;
  else
    p->suspend_list_tail = ts;
  p->suspend_list_head = ts;
}

// Call with sched lock
static void suspend(pn_proactor_t *p, tslot_t *ts) {
  if (ts->state == NEW)
    suspend_list_add_tail(p, ts);
  else
    suspend_list_insert_head(p, ts);
  p->suspend_list_count++;
  ts->state = SUSPENDED;
  ts->scheduled = false;
  unlock(&p->sched_mutex);

  lock(&ts->mutex);
  if (pni_spins && !ts->scheduled) {
    // Medium length spinning tried here.  Raises cpu dramatically,
    // unclear throughput or latency benefit (not seen where most
    // expected, modest at other times).
    bool locked = true;
    for (volatile int i = 0; i < pni_spins; i++) {
      if (locked) {
        unlock(&ts->mutex);
        locked = false;
      }
      if ((i % 1000) == 0) {
        locked = (pthread_mutex_trylock(&ts->mutex) == 0);
      }
      if (ts->scheduled) break;
    }
    if (!locked)
      lock(&ts->mutex);
  }

  ts->suspended = true;
  while (!ts->scheduled) {
    pthread_cond_wait(&ts->cond, &ts->mutex);
  }
  ts->suspended = false;
  unlock(&ts->mutex);
  lock(&p->sched_mutex);
  assert(ts->state == PROCESSING);
}

// Call with no lock
static void resume(pn_proactor_t *p, tslot_t *ts) {
  lock(&ts->mutex);
  ts->scheduled = true;
  if (ts->suspended) {
    pthread_cond_signal(&ts->cond);
  }
  unlock(&ts->mutex);

}

// Call with sched lock
static void assign_thread(tslot_t *ts, pcontext_t *ctx) {
  assert(!ctx->runner);
  ctx->runner = ts;
  ctx->prev_runner = NULL;
  ctx->runnable = false;
  ts->context = ctx;
  ts->prev_context = NULL;
}

// call with sched lock
static bool rewake(pcontext_t *ctx) {
  // Special case wake() where context is unassigned and a popped wake needs to be put back on the list.
  // Should be rare.
  bool notify = false;
  pn_proactor_t *p = ctx->proactor;
  lock(&p->eventfd_mutex);
  assert(ctx->wake_pending);
  assert(!ctx->on_wake_list);
  ctx->wake_next = NULL;
  ctx->on_wake_list = true;
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
  return notify;
}

// Call with sched lock
bool unassign_thread(tslot_t *ts, tslot_state new_state) {
  pcontext_t *ctx = ts->context;
  bool notify = false;
  bool deleting = (ts->state == DELETING);
  ts->context = NULL;
  ts->state = new_state;
  if (ctx) {
    ctx->runner = NULL;
    ctx->prev_runner = ts;
  }

  // Check if context has unseen events/wake that need processing.

  if (ctx && !deleting) {
    pn_proactor_t *p = ctx->proactor;
    ts->prev_context = ts->context;
    if (ctx->sched_pending) {
      // Need a new wake
      if (ctx->sched_wake) {
        if (!ctx->on_wake_list) {
          // Remember it for next poller
          ctx->sched_wake = false;
          notify = rewake(ctx);     // back on wake list for poller to see
        }
        // else already scheduled
      } else {
        // bad corner case.  Block ctx from being scheduled again until a later post_wake()
        ctx->runner = REWAKE_PLACEHOLDER;
        unlock(&p->sched_mutex);
        lock(&ctx->mutex);
        notify = wake(ctx);
        unlock(&ctx->mutex);
        lock(&p->sched_mutex);
      }
    }
  }
  return notify;
}

// Call with sched lock
static void earmark_thread(tslot_t *ts, pcontext_t *ctx) {
  assign_thread(ts, ctx);
  ts->earmarked = true;
  ctx->proactor->earmark_count++;
}

// Call with sched lock
static void remove_earmark(tslot_t *ts) {
  pcontext_t *ctx = ts->context;
  ts->context = NULL;
  ctx->runner = NULL;
  ts->earmarked = false;
  ctx->proactor->earmark_count--;
}

// Call with sched lock
static void make_runnable(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  assert(p->n_runnables <= p->runnables_capacity);
  assert(!ctx->runnable);
  if (ctx->runner) return;

  ctx->runnable = true;
  // Track it as normal or warm or earmarked
  if (pni_warm_sched) {
    tslot_t *ts = ctx->prev_runner;
    if (ts && ts->prev_context == ctx) {
      if (ts->state == SUSPENDED || ts->state == PROCESSING) {
        if (p->n_warm_runnables < p->thread_capacity) {
          p->warm_runnables[p->n_warm_runnables++] = ctx;
          assign_thread(ts, ctx);
        }
        else
          p->runnables[p->n_runnables++] = ctx;
        return;
      }
      if (ts->state == UNUSED && !p->earmark_drain) {
        earmark_thread(ts, ctx);
        p->last_earmark = ts;
        return;
      }
    }
  }
  p->runnables[p->n_runnables++] = ctx;
}



void psocket_init(psocket_t* ps, pn_proactor_t* p, epoll_type_t type)
{
  ps->epoll_io.fd = -1;
  ps->epoll_io.type = type;
  ps->epoll_io.wanted = 0;
  ps->epoll_io.polling = false;
  ps->proactor = p;
}


/* Protects read/update of pn_connection_t pointer to it's pconnection_t
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
  return containerof(d, pconnection_t, driver);
}

static void set_pconnection(pn_connection_t* c, pconnection_t *pc) {
  lock(&driver_ptr_mutex);
  *pn_connection_driver_ptr(c) = pc ? &pc->driver : NULL;
  unlock(&driver_ptr_mutex);
}

static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool wake, bool topup);
static void write_flush(pconnection_t *pc);
static void listener_begin_close(pn_listener_t* l);
static bool poller_do_epoll(struct pn_proactor_t* p, tslot_t *ts, bool can_block);
static void poller_done(struct pn_proactor_t* p, tslot_t *ts);

static inline pconnection_t *psocket_pconnection(psocket_t* ps) {
  return ps->epoll_io.type == PCONNECTION_IO ? containerof(ps, pconnection_t, psocket) : NULL;
}

static inline pn_listener_t *psocket_listener(psocket_t* ps) {
  return ps->epoll_io.type == LISTENER_IO ? containerof(ps, acceptor_t, psocket)->listener : NULL;
}

static inline acceptor_t *psocket_acceptor(psocket_t* ps) {
  return ps->epoll_io.type == LISTENER_IO ? containerof(ps, acceptor_t, psocket) : NULL;
}

static inline pconnection_t *pcontext_pconnection(pcontext_t *c) {
  return c->type == PCONNECTION ? containerof(c, pconnection_t, context) : NULL;
}

static inline pn_listener_t *pcontext_listener(pcontext_t *c) {
  return c->type == LISTENER ? containerof(c, pn_listener_t, context) : NULL;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch);
static pn_event_t *proactor_batch_next(pn_event_batch_t *batch);
static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch);

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

static void psocket_error_str(psocket_t *ps, const char *msg, const char* what) {
  pconnection_t *pc = psocket_pconnection(ps);
  if (pc) {
    pn_connection_driver_t *driver = &pc->driver;
    pn_connection_driver_bind(driver); /* Bind so errors will be reported */
    pni_proactor_set_cond(pn_transport_condition(driver->transport), what, pc->host, pc->port, msg);
    pn_connection_driver_close(driver);
    return;
  }
  pn_listener_t *l = psocket_listener(ps);
  if (l) {
    pni_proactor_set_cond(l->condition, what, l->host, l->port, msg);
    listener_begin_close(l);
    return;
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

static void listener_accepted_append(pn_listener_t *listener, accepted_t item) {
  if (listener->pending_first+listener->pending_count >= listener->backlog) return;

  listener->pending_accepteds[listener->pending_first+listener->pending_count] = item;
  listener->pending_count++;
}

accepted_t *listener_accepted_next(pn_listener_t *listener) {
  if (!listener->pending_count) return NULL;

  listener->pending_count--;
  return &listener->pending_accepteds[listener->pending_first++];
}

static void acceptor_list_append(acceptor_t **start, acceptor_t *item) {
  assert(item->next == NULL);
  if (*start) {
    acceptor_t *end = *start;
    while (end->next)
      end = end->next;
    end->next = item;
  }
  else *start = item;
}

static acceptor_t *acceptor_list_next(acceptor_t **start) {
  acceptor_t *item = *start;
  if (*start) *start = (*start)->next;
  if (item) item->next = NULL;
  return item;
}

// Add an overflowing acceptor to the overflow list. Called with listener context lock held.
static void acceptor_set_overflow(acceptor_t *a) {
  a->overflowed = true;
  pn_proactor_t *p = a->psocket.proactor;
  lock(&p->overflow_mutex);
  acceptor_list_append(&p->overflow, a);
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
  acceptor_t *a = acceptor_list_next(&ovflw);
  while (a) {
    pn_listener_t *l = a->listener;
    lock(&l->context.mutex);
    bool rearming = !l->context.closing;
    bool notify = false;
    assert(!a->armed);
    assert(a->overflowed);
    a->overflowed = false;
    if (rearming) {
      rearm(p, &a->psocket.epoll_io);
      a->armed = true;
    }
    else notify = wake(&l->context);
    unlock(&l->context.mutex);
    if (notify) wake_notify(&l->context);
    a = acceptor_list_next(&ovflw);
  }
}

// Close an FD and rearm overflow listeners.  Call with no listener locks held.
int pclosefd(pn_proactor_t *p, int fd) {
  int err = close(fd);
  if (!err) proactor_rearm_overflow(p);
  return err;
}


// ========================================================================
// pconnection
// ========================================================================

static void pconnection_tick(pconnection_t *pc);

static const char *pconnection_setup(pconnection_t *pc, pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, bool server, const char *addr, size_t addrlen)
{
  memset(pc, 0, sizeof(*pc));

  if (pn_connection_driver_init(&pc->driver, c, t) != 0) {
    free(pc);
    return "pn_connection_driver_init failure";
  }
  if (!ctimerq_register(&p->ctimerq, pc)) {
    free(pc);
    return "timer queue register failure";
  }

  pcontext_init(&pc->context, PCONNECTION, p);
  psocket_init(&pc->psocket, p, PCONNECTION_IO);
  pni_parse_addr(addr, pc->addr_buf, addrlen+1, &pc->host, &pc->port);
  pc->new_events = 0;
  pc->wake_count = 0;
  pc->tick_pending = false;
  pc->queued_disconnect = false;
  pc->disconnect_condition = NULL;

  pc->current_arm = 0;
  pc->connected = false;
  pc->read_blocked = true;
  pc->write_blocked = true;
  pc->disconnected = false;
  pc->output_drained = false;
  pc->wbuf_completed = 0;
  pc->wbuf_remaining = 0;
  pc->wbuf_current = NULL;
  pc->hog_count = 0;
  pc->batch.next_event = pconnection_batch_next;

  if (server) {
    pn_transport_set_server(pc->driver.transport);
  }

  pmutex_init(&pc->rearm_mutex);

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
  return !pc->current_arm && !pc->context.wake_pending && !pc->tick_pending;
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
  ctimerq_deregister(&pc->context.proactor->ctimerq, pc);
  free(pc);
}


// call without lock
static void pconnection_cleanup(pconnection_t *pc) {
  assert(pconnection_is_final(pc));
  int fd = pc->psocket.epoll_io.fd;
  stop_polling(&pc->psocket.epoll_io, pc->psocket.proactor->epollfd);
  if (fd != -1)
    pclosefd(pc->psocket.proactor, fd);

  lock(&pc->context.mutex);
  bool can_free = proactor_remove(&pc->context);
  unlock(&pc->context.mutex);
  if (can_free)
    pconnection_final_free(pc);
  // else proactor_disconnect logic owns psocket and its final free
}

static void set_wbuf(pconnection_t *pc, const char *start, size_t sz) {
  pc->wbuf_completed = 0;
  pc->wbuf_current = start;
  pc->wbuf_remaining = sz;
}

// Never call with any locks held.
static void ensure_wbuf(pconnection_t *pc) {
  // next connection_driver call is the expensive output generator
  pn_bytes_t bytes = pn_connection_driver_write_buffer(&pc->driver);
  set_wbuf(pc, bytes.start, bytes.size);
  if (bytes.size == 0)
    pc->output_drained = true;
}

// Call with lock held or from forced_shutdown
static void pconnection_begin_close(pconnection_t *pc) {
  if (!pc->context.closing) {
    pc->context.closing = true;
    pc->tick_pending = false;
    if (pc->current_arm) {
      // Force EPOLLHUP callback(s)
      shutdown(pc->psocket.epoll_io.fd, SHUT_RDWR);
    }

    pn_connection_driver_close(&pc->driver);
  }
}

static void pconnection_forced_shutdown(pconnection_t *pc) {
  // Called by proactor_free, no competing threads, no epoll activity.
  pc->current_arm = 0;
  pc->new_events = 0;
  pconnection_begin_close(pc);
  // pconnection_process will never be called again.  Zero everything.
  pc->context.wake_pending = 0;
  pn_collector_release(pc->driver.collector);
  assert(pconnection_is_final(pc));
  pconnection_cleanup(pc);
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (!pc->driver.connection) return NULL;
  pn_event_t *e = pn_connection_driver_next_event(&pc->driver);
  if (!e) {
    pn_proactor_t *p = pc->context.proactor;
    bool idle_threads;
    lock(&p->sched_mutex);
    idle_threads = (p->suspend_list_head != NULL);
    unlock(&p->sched_mutex);
    if (idle_threads && !pc->write_blocked && !pc->read_blocked) {
      write_flush(pc);  // May generate transport event
      pconnection_process(pc, 0, false, true);
      e = pn_connection_driver_next_event(&pc->driver);
    }
    else {
      write_flush(pc);  // May generate transport event
      e = pn_connection_driver_next_event(&pc->driver);
      if (!e && pc->hog_count < HOG_MAX) {
        pconnection_process(pc, 0, false, true);
        e = pn_connection_driver_next_event(&pc->driver);
      }
    }
  }
  if (e) {
    pc->output_drained = false;
    pc->current_event_type = pn_event_type(e);
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
static int pconnection_rearm_check(pconnection_t *pc) {
  if (pconnection_rclosed(pc) && pconnection_wclosed(pc)) {
    return 0;
  }
  uint32_t wanted_now = (pc->read_blocked && !pconnection_rclosed(pc)) ? EPOLLIN : 0;
  if (!pconnection_wclosed(pc)) {
    if (pc->write_blocked)
      wanted_now |= EPOLLOUT;
    else {
      if (pc->wbuf_remaining > 0)
        wanted_now |= EPOLLOUT;
    }
  }
  if (!wanted_now) return 0;
  if (wanted_now == pc->current_arm) return 0;

  return wanted_now;
}

static inline void pconnection_rearm(pconnection_t *pc, int wanted_now) {
  lock(&pc->rearm_mutex);
  pc->current_arm = pc->psocket.epoll_io.wanted = wanted_now;
  rearm(pc->psocket.proactor, &pc->psocket.epoll_io);
  unlock(&pc->rearm_mutex);
  // Return immediately.  pc may have just been freed by another thread.
}

/* Only call when context switch is imminent.  Sched lock is highly contested. */
// Call with both context and sched locks.
static bool pconnection_sched_sync(pconnection_t *pc) {
  uint32_t sync_events = 0;
  uint32_t sync_args = pc->tick_pending << 1;
  if (pc->psocket.sched_io_events) {
    pc->new_events = pc->psocket.sched_io_events;
    pc->psocket.sched_io_events = 0;
    pc->current_arm = 0;  // or outside lock?
    sync_events = pc->new_events;
  }
  if (pc->context.sched_wake) {
    pc->context.sched_wake = false;
    wake_done(&pc->context);
    sync_args |= 1;
  }
  pc->context.sched_pending = false;

  if (sync_args || sync_events) {
    // Only replace if poller has found new work for us.
    pc->process_args = (1 << 2) | sync_args;
    pc->process_events = sync_events;
  }

  // Indicate if there are free proactor threads
  pn_proactor_t *p = pc->context.proactor;
  return p->poller_suspended || p->suspend_list_head;
}

/* Call with context lock and having done a write_flush() to "know" the value of wbuf_remaining */
static inline bool pconnection_work_pending(pconnection_t *pc) {
  if (pc->new_events || pc->wake_count || pc->tick_pending || pc->queued_disconnect)
    return true;
  if (!pc->read_blocked && !pconnection_rclosed(pc))
    return true;
  return (pc->wbuf_remaining > 0 && !pc->write_blocked);
}

/* Call with no locks. */
static void pconnection_done(pconnection_t *pc) {
  pn_proactor_t *p = pc->context.proactor;
  tslot_t *ts = pc->context.runner;
  write_flush(pc);
  bool notify = false;
  bool self_wake = false;
  lock(&pc->context.mutex);
  pc->context.working = false;  // So we can wake() ourself if necessary.  We remain the de facto
                                // working context while the lock is held.  Need sched_sync too to drain possible stale wake.
  pc->hog_count = 0;
  bool has_event = pconnection_has_event(pc);
  // Do as little as possible while holding the sched lock
  lock(&p->sched_mutex);
  pconnection_sched_sync(pc);
  unlock(&p->sched_mutex);

  if (has_event || pconnection_work_pending(pc)) {
    self_wake = true;
  } else if (pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->context.mutex);
      pconnection_cleanup(pc);
      // pc may be undefined now
      lock(&p->sched_mutex);
      notify = unassign_thread(ts, UNUSED);
      unlock(&p->sched_mutex);
      if (notify)
        wake_notify(&p->context);
      return;
    }
  }
  if (self_wake)
    notify = wake(&pc->context);

  int wanted = pconnection_rearm_check(pc);
  unlock(&pc->context.mutex);

  if (wanted) pconnection_rearm(pc, wanted);  // May free pc on another thread.  Return without touching pc again.
  lock(&p->sched_mutex);
  if (unassign_thread(ts, UNUSED))
    notify = true;
  unlock(&p->sched_mutex);
  if (notify) wake_notify(&p->context);
  return;
}

// Return true unless error
static bool pconnection_write(pconnection_t *pc) {
  size_t wbuf_size = pc->wbuf_remaining;
  ssize_t n = send(pc->psocket.epoll_io.fd, pc->wbuf_current, wbuf_size, MSG_NOSIGNAL);
  if (n > 0) {
    pc->wbuf_completed += n;
    pc->wbuf_remaining -= n;
    pc->io_doublecheck = false;
    if (pc->wbuf_remaining) {
      pc->write_blocked = true;
      pc->wbuf_current += n;
    }
    else {
      // write_done also calls pn_transport_pending(), so the transport knows all current output
      pn_connection_driver_write_done(&pc->driver, pc->wbuf_completed);
      pc->wbuf_completed = 0;
      pn_transport_t *t = pc->driver.transport;
      set_wbuf(pc, t->output_buf, t->output_pending);
      if (t->output_pending == 0)
        pc->output_drained = true;
      // TODO: revise transport API to allow similar efficient access to transport output
    }
  } else if (errno == EWOULDBLOCK) {
    pc->write_blocked = true;
  } else if (!(errno == EAGAIN || errno == EINTR)) {
    pc->wbuf_remaining = 0;
    return false;
  }
  return true;
}

// Never call with any locks held.
static void write_flush(pconnection_t *pc) {
  size_t prev_wbuf_remaining = 0;

  while(!pc->write_blocked && !pc->output_drained && !pconnection_wclosed(pc)) {
    if (pc->wbuf_remaining == 0) {
      ensure_wbuf(pc);
      if (pc->wbuf_remaining == 0)
        pc->output_drained = true;
    } else {
      // Check if we are doing multiple small writes in a row, possibly worth growing the transport output buffer.
      if (prev_wbuf_remaining
          && prev_wbuf_remaining == pc->wbuf_remaining         // two max outputs in a row
          && pc->wbuf_remaining < 131072) {
        ensure_wbuf(pc);  // second call -> unchanged wbuf or transport buffer size doubles and more bytes added
        prev_wbuf_remaining = 0;
      } else {
        prev_wbuf_remaining = pc->wbuf_remaining;
      }
    }
    if (pc->wbuf_remaining > 0) {
      if (!pconnection_write(pc)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "disconnected" : "on write to");
      }
      // pconnection_write side effect: wbuf may be replenished, and if not, output_drained may be set.
    }
    else {
      if (pn_connection_driver_write_closed(&pc->driver)) {
        shutdown(pc->psocket.epoll_io.fd, SHUT_WR);
        pc->write_blocked = true;
      }
    }
  }
}

static void pconnection_connected_lh(pconnection_t *pc);
static void pconnection_maybe_connect_lh(pconnection_t *pc);

static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool sched_wake, bool topup) {
  bool waking = false;
  bool tick_required = false;
  bool immediate_write = false;
  if (!topup) {
    pc->process_events = events;
    pc->process_args = (pc->tick_pending << 1) | sched_wake;
  }
  // Don't touch data exclusive to working thread (yet).
  lock(&pc->context.mutex);

  if (events) {
    pc->new_events = events;
    pc->current_arm = 0;
    events = 0;
  }
  if (sched_wake) wake_done(&pc->context);

  if (topup) {
    // Only called by the batch owner.  Does not loop, just "tops up"
    // once.  May be back depending on hog_count.
    assert(pc->context.working);
  }
  else {
    if (pc->context.working) {
      // Another thread is the working context.  Should be impossible with new scheduler.
      EPOLL_FATAL("internal epoll proactor error: two worker threads", 0);
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

  if (pc->new_events) {
    uint32_t update_events = pc->new_events;
    pc->current_arm = 0;
    pc->new_events = 0;
    if (!pc->context.closing) {
      if ((update_events & (EPOLLHUP | EPOLLERR)) && !pconnection_rclosed(pc) && !pconnection_wclosed(pc))
        pconnection_maybe_connect_lh(pc);
      else
        pconnection_connected_lh(pc); /* Non error event means we are connected */
      if (update_events & EPOLLOUT) {
        pc->write_blocked = false;
        if (pc->wbuf_remaining > 0)
          immediate_write = true;
      }
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

  if (immediate_write) {
    immediate_write = false;
    write_flush(pc);
  }

  if (!pconnection_rclosed(pc)) {
    pn_rwbytes_t rbuf = pn_connection_driver_read_buffer(&pc->driver);
    if (rbuf.size > 0 && !pc->read_blocked) {
      ssize_t n = read(pc->psocket.epoll_io.fd, rbuf.start, rbuf.size);
      if (n > 0) {
        pn_connection_driver_read_done(&pc->driver, n);
        pc->output_drained = false;
        pconnection_tick(pc);         /* check for tick changes. */
        tick_required = false;
        pc->io_doublecheck = false;
        if (!pn_connection_driver_read_closed(&pc->driver) && (size_t)n < rbuf.size)
          pc->read_blocked = true;
      }
      else if (n == 0) {
        pc->read_blocked = true;
        pn_connection_driver_read_close(&pc->driver);
      }
      else if (errno == EWOULDBLOCK)
        pc->read_blocked = true;
      else if (!(errno == EAGAIN || errno == EINTR)) {
        psocket_error(&pc->psocket, errno, pc->disconnected ? "disconnected" : "on read from");
      }
    }
  } else {
    pc->read_blocked = true;
  }

  if (tick_required) {
    pconnection_tick(pc);         /* check for tick changes. */
    tick_required = false;
    pc->output_drained = false;
  }

  if (topup) {
    // If there was anything new to topup, we have it by now.
    return NULL;  // caller already owns the batch
  }

  if (pconnection_has_event(pc)) {
    pc->output_drained = false;
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
  lock(&pc->context.proactor->sched_mutex);
  bool workers_free = pconnection_sched_sync(pc);
  unlock(&pc->context.proactor->sched_mutex);

  if (pconnection_work_pending(pc)) {
    goto retry;  // TODO: get rid of goto without adding more locking
  }

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

  if (workers_free && !pc->context.closing && !pc->io_doublecheck) {
    // check one last time for new io before context switch
    pc->io_doublecheck = true;
    pc->read_blocked = false;
    pc->write_blocked = false;
    pc->context.working = true;
    goto retry;
  }

  int wanted = pconnection_rearm_check(pc);  // holds rearm_mutex until pconnection_rearm() below

  unlock(&pc->context.mutex);
  if (wanted) pconnection_rearm(pc, wanted);  // May free pc on another thread.  Return right away.
  return NULL;
}

void configure_socket(int sock) {
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
    (void)getpeername(pc->psocket.epoll_io.fd, (struct sockaddr*)&pc->remote.ss, &len);
  }
}

/* multi-address connections may call pconnection_start multiple times with diffferent FDs  */
static void pconnection_start(pconnection_t *pc, int fd) {
  int efd = pc->psocket.proactor->epollfd;
  /* Get the local socket name now, get the peer name in pconnection_connected */
  socklen_t len = sizeof(pc->local.ss);
  (void)getsockname(fd, (struct sockaddr*)&pc->local.ss, &len);

  epoll_extended_t *ee = &pc->psocket.epoll_io;
  if (ee->polling) {     /* This is not the first attempt, stop polling and close the old FD */
    int fd = ee->fd;     /* Save fd, it will be set to -1 by stop_polling */
    stop_polling(ee, efd);
    pclosefd(pc->psocket.proactor, fd);
  }
  ee->fd = fd;
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
          pconnection_start(pc, fd);
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
    if (pc->psocket.epoll_io.fd < 0) {
      psocket_error(&pc->psocket, errno ? errno : ENOTCONN, "on connect");
    }
  }
  pc->disconnected = true;
}

int pgetaddrinfo(const char *host, const char *port, int flags, struct addrinfo **res)
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
bool wake_if_inactive(pn_proactor_t *p) {
  if (is_inactive(p)) {
    p->need_inactive = true;
    return wake(&p->context);
  }
  return false;
}

void pn_proactor_connect2(pn_proactor_t *p, pn_connection_t *c, pn_transport_t *t, const char *addr) {
  size_t addrlen = strlen(addr);
  pconnection_t *pc = (pconnection_t*) malloc(sizeof(pconnection_t)+addrlen);
  assert(pc); // TODO: memory safety
  const char *err = pconnection_setup(pc, p, c, t, false, addr, addrlen);
  if (err) {    /* TODO aconway 2017-09-13: errors must be reported as events */
    PN_LOG_DEFAULT(PN_SUBSYSTEM_EVENT, PN_LEVEL_ERROR, "pn_proactor_connect failure: %s", err);
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
    int gai_error = pgetaddrinfo(pc->host, pc->port, 0, &pc->addrinfo);
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
    uint64_t now = pn_proactor_now_64();
    uint64_t next = pn_transport_tick(t, now);
    if (next) {
      ctimerq_schedule_tick(&pc->context.proactor->ctimerq, pc, next, now);
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
  return (pn_event_class(e) == PN_CLASSCLASS(pn_listener)) ? (pn_listener_t*)pn_event_context(e) : NULL;
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
    pcontext_init(&l->context, LISTENER, unknown);
  }
  return l;
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog)
{
  // TODO: check listener not already listening for this or another proactor
  lock(&l->context.mutex);
  l->context.proactor = p;

  l->pending_accepteds = (accepted_t*)calloc(backlog, sizeof(accepted_t));
  assert(l->pending_accepteds);
  l->backlog = backlog;

  pni_parse_addr(addr, l->addr_buf, sizeof(l->addr_buf), &l->host, &l->port);

  struct addrinfo *addrinfo = NULL;
  int gai_err = pgetaddrinfo(l->host, l->port, AI_PASSIVE | AI_ALL, &addrinfo);
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
        configure_socket(fd);
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

          acceptor->listener = l;
          psocket_t *ps = &acceptor->psocket;
          psocket_init(ps, p, LISTENER_IO);
          ps->epoll_io.fd = fd;
          ps->epoll_io.wanted = EPOLLIN;
          ps->epoll_io.polling = false;
          start_polling(&ps->epoll_io, ps->proactor->epollfd);  // TODO: check for error
          l->active_count++;
          acceptor->armed = true;
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
    psocket_init(&l->acceptors[0].psocket, p, LISTENER_IO);
    l->acceptors[0].listener = l;
    if (gai_err) {
      psocket_gai_error(&l->acceptors[0].psocket, gai_err, "listen on");
    } else {
      psocket_error(&l->acceptors[0].psocket, errno, "listen on");
    }
  } else {
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_OPEN);
  }
  proactor_add(&l->context);
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
  return;
}

// call with lock held and context.working false
static inline bool listener_can_free(pn_listener_t *l) {
  return l->context.closing && l->close_dispatched && !l->context.wake_pending && !l->active_count;
}

static inline void listener_final_free(pn_listener_t *l) {
  pcontext_finalize(&l->context);
  free(l->acceptors);
  free(l->pending_accepteds);
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
      if (ps->epoll_io.fd >= 0) {
        if (a->armed) {
          shutdown(ps->epoll_io.fd, SHUT_RD);  // Force epoll event and callback
        } else {
          int fd = ps->epoll_io.fd;
          stop_polling(&ps->epoll_io, ps->proactor->epollfd);
          close(fd);
          l->active_count--;
        }
      }
    }
    /* Close all sockets waiting for a pn_listener_accept2() */
    accepted_t *a = listener_accepted_next(l);
    while (a) {
      close(a->accepted_fd);
      a->accepted_fd = -1;
      a = listener_accepted_next(l);
    }
    assert(!l->pending_count);

    unlock(&l->context.mutex);
    /* Remove all acceptors from the overflow list.  closing flag prevents re-insertion.*/
    proactor_rearm_overflow(pn_listener_proactor(l));
    lock(&l->context.mutex);
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_CLOSE);
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
  l->context.wake_pending = 0;
  l->close_dispatched = true;
  l->active_count = 0;
  assert(listener_can_free(l));
  pn_listener_free(l);
}

/* Accept a connection as part of listener_process(). Called with listener context lock held. */
/* Keep on accepting until we fill the backlog, would block or get an error */
static void listener_accept_lh(psocket_t *ps) {
  pn_listener_t *l = psocket_listener(ps);
  while (l->pending_first+l->pending_count < l->backlog) {
    int fd = accept(ps->epoll_io.fd, NULL, 0);
    if (fd >= 0) {
      accepted_t accepted = {.accepted_fd = fd} ;
      listener_accepted_append(l, accepted);
    } else {
      int err = errno;
      if (err == ENFILE || err == EMFILE) {
        acceptor_t *acceptor = psocket_acceptor(ps);
        acceptor_set_overflow(acceptor);
      } else if (err != EWOULDBLOCK) {
        psocket_error(ps, err, "accept");
      }
      return;
    }
  }
}

/* Process a listening socket */
static pn_event_batch_t *listener_process(pn_listener_t *l, int n_events, bool wake) {
  // TODO: some parallelization of the accept mechanism.
//  pn_listener_t *l = psocket_listener(ps);
//  acceptor_t *a = psocket_acceptor(ps);

  lock(&l->context.mutex);
  if (n_events) {
    for (size_t i = 0; i < l->acceptors_size; i++) {
      psocket_t *ps = &l->acceptors[i].psocket;
      if (ps->working_io_events) {
        uint32_t events = ps->working_io_events;
        ps->working_io_events = 0;
        if (l->context.closing) {
          l->acceptors[i].armed = false;
          int fd = ps->epoll_io.fd;
          stop_polling(&ps->epoll_io, ps->proactor->epollfd);
          close(fd);
          l->active_count--;
        } else {
          l->acceptors[i].armed = false;
          if (events & EPOLLRDHUP) {
            /* Calls listener_begin_close which closes all the listener's sockets */
            psocket_error(ps, errno, "listener epoll");
          } else if (!l->context.closing && events & EPOLLIN) {
            listener_accept_lh(ps);
          }
        }
      }
    }
  }
  if (wake) {
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
  if (!e && l->pending_count) {
    // empty collector means pn_collector_put() will not coalesce
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_ACCEPT);
    e = pn_collector_next(l->collector);
  }
  if (e && pn_event_type(e) == PN_LISTENER_CLOSE)
    l->close_dispatched = true;
  unlock(&l->context.mutex);
  return pni_log_event(l, e);
}

static void listener_done(pn_listener_t *l) {
  pn_proactor_t *p = l->context.proactor;
  tslot_t *ts = l->context.runner;
  lock(&l->context.mutex);

  // Just in case the app didn't accept all the pending accepts
  // Shuffle the list back to start at 0
  memmove(&l->pending_accepteds[0], &l->pending_accepteds[l->pending_first], l->pending_count * sizeof(accepted_t));
  l->pending_first = 0;

  if (!l->context.closing) {
    for (size_t i = 0; i < l->acceptors_size; i++) {
      acceptor_t *a = &l->acceptors[i];
      psocket_t *ps = &a->psocket;

      // Rearm acceptor when appropriate
      if (ps->epoll_io.polling && l->pending_count==0 && !a->overflowed) {
        if (!a->armed) {
          rearm(ps->proactor, &ps->epoll_io);
          a->armed = true;
        }
      }
    }
  }

  bool notify = false;
  l->context.working = false;

  lock(&p->sched_mutex);
  int n_events = 0;
  for (size_t i = 0; i < l->acceptors_size; i++) {
    psocket_t *ps = &l->acceptors[i].psocket;
    if (ps->sched_io_events) {
      ps->working_io_events = ps->sched_io_events;
      ps->sched_io_events = 0;
    }
    if (ps->working_io_events)
      n_events++;
  }

  if (l->context.sched_wake) {
    l->context.sched_wake = false;
    wake_done(&l->context);
  }
  unlock(&p->sched_mutex);

  if (!n_events && listener_can_free(l)) {
    unlock(&l->context.mutex);
    pn_listener_free(l);
    lock(&p->sched_mutex);
    notify = unassign_thread(ts, UNUSED);
    unlock(&p->sched_mutex);
    if (notify)
      wake_notify(&p->context);
    return;
  } else if (n_events || listener_has_event(l))
    notify = wake(&l->context);
  unlock(&l->context.mutex);

  lock(&p->sched_mutex);
  if (unassign_thread(ts, UNUSED))
    notify = true;
  unlock(&p->sched_mutex);
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
  pconnection_t *pc = (pconnection_t*) malloc(sizeof(pconnection_t));
  assert(pc); // TODO: memory safety
  const char *err = pconnection_setup(pc, pn_listener_proactor(l), c, t, true, "", 0);
  if (err) {
    PN_LOG_DEFAULT(PN_SUBSYSTEM_EVENT, PN_LEVEL_ERROR, "pn_listener_accept failure: %s", err);
    return;
  }
  // TODO: fuller sanity check on input args

  int err2 = 0;
  int fd = -1;
  bool notify = false;
  lock(&l->context.mutex);
  if (l->context.closing)
    err2 = EBADF;
  else {
    accepted_t *a = listener_accepted_next(l);
    if (a) {
      fd = a->accepted_fd;
      a->accepted_fd = -1;
    }
    else err2 = EWOULDBLOCK;
  }

  proactor_add(&pc->context);
  lock(&pc->context.mutex);
  if (fd >= 0) {
    configure_socket(fd);
    pconnection_start(pc, fd);
    pconnection_connected_lh(pc);
  }
  else
    psocket_error(&pc->psocket, err2, "pn_listener_accept");
  if (!l->context.working && listener_has_event(l))
    notify = wake(&l->context);
  unlock(&pc->context.mutex);
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}


// ========================================================================
// proactor
// ========================================================================

// Call with sched_mutex. Alloc calls are expensive but only used when thread_count changes.
static void grow_poller_bufs(pn_proactor_t* p) {
  // call if p->thread_count > p->thread_capacity
  assert(p->thread_count == 0 || p->thread_count > p->thread_capacity);
  do {
    p->thread_capacity += 8;
  } while (p->thread_count > p->thread_capacity);

  p->warm_runnables = (pcontext_t **) realloc(p->warm_runnables, p->thread_capacity * sizeof(pcontext_t *));
  p->resume_list = (tslot_t **) realloc(p->resume_list, p->thread_capacity * sizeof(tslot_t *));

  int old_cap = p->runnables_capacity;
  if (p->runnables_capacity == 0)
    p->runnables_capacity = 16;
  else if (p->runnables_capacity < p->thread_capacity)
    p->runnables_capacity = p->thread_capacity;
  if (p->runnables_capacity != old_cap) {
    p->runnables = (pcontext_t **) realloc(p->runnables, p->runnables_capacity * sizeof(pcontext_t *));
    p->kevents_capacity = p->runnables_capacity;
    size_t sz = p->kevents_capacity * sizeof(struct epoll_event);
    p->kevents = (struct epoll_event *) realloc(p->kevents, sz);
    memset(p->kevents, 0, sz);
  }
}

/* Set up an epoll_extended_t to be used for wakeup or interrupts */
 static void epoll_wake_init(epoll_extended_t *ee, int eventfd, int epollfd, bool always_set) {
  ee->fd = eventfd;
  ee->type = WAKE;
  if (always_set) {
    uint64_t increment = 1;
    if (write(eventfd, &increment, sizeof(uint64_t)) != sizeof(uint64_t))
      EPOLL_FATAL("setting eventfd", errno);
    // eventfd is set forever.  No reads, just rearms as needed.
    ee->wanted = 0;
  } else {
    ee->wanted = EPOLLIN;
  }
  ee->polling = false;
  start_polling(ee, epollfd);  // TODO: check for error
  if (always_set)
    ee->wanted = EPOLLIN;      // for all subsequent rearms
}

pn_proactor_t *pn_proactor() {
  if (getenv("PNI_EPOLL_NOWARM")) pni_warm_sched = false;
  if (getenv("PNI_EPOLL_IMMEDIATE")) pni_immediate = true;
  if (getenv("PNI_EPOLL_SPINS")) pni_spins = atoi(getenv("PNI_EPOLL_SPINS"));
  pn_proactor_t *p = (pn_proactor_t*)calloc(1, sizeof(*p));
  if (!p) return NULL;
  p->epollfd = p->eventfd = -1;
  pcontext_init(&p->context, PROACTOR, p);
  pmutex_init(&p->eventfd_mutex);
  pmutex_init(&p->sched_mutex);
  pmutex_init(&p->tslot_mutex);
  ptimer_init(&p->timer, PROACTOR_TIMER);

  if ((p->epollfd = epoll_create(1)) >= 0) {
    if ((p->eventfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
      if ((p->interruptfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
        if (p->timer.epoll_io.fd >= 0 && ctimerq_init(p))
          if ((p->collector = pn_collector()) != NULL) {
            p->batch.next_event = &proactor_batch_next;
            start_polling(&p->timer.epoll_io, p->epollfd);  // TODO: check for error
            start_polling(&p->ctimerq.timer.epoll_io, p->epollfd);  // TODO: check for error
            p->timer_armed = true;
            epoll_wake_init(&p->epoll_wake, p->eventfd, p->epollfd, true);
            epoll_wake_init(&p->epoll_interrupt, p->interruptfd, p->epollfd, false);
            p->tslot_map = pn_hash(PN_VOID, 0, 0.75);
            grow_poller_bufs(p);
            return p;
          }
      }
    }
  }
  if (p->epollfd >= 0) close(p->epollfd);
  if (p->eventfd >= 0) close(p->eventfd);
  if (p->interruptfd >= 0) close(p->interruptfd);
  ptimer_finalize(&p->timer);
  ctimerq_finalize(p);
  pmutex_finalize(&p->tslot_mutex);
  pmutex_finalize(&p->sched_mutex);
  pmutex_finalize(&p->eventfd_mutex);
  if (p->collector) pn_free(p->collector);
  assert(p->thread_count == 0);
  free (p);
  return NULL;
}

void pn_proactor_free(pn_proactor_t *p) {
  //  No competing threads, not even a pending timer
  p->shutting_down = true;
  close(p->epollfd);
  p->epollfd = -1;
  close(p->eventfd);
  p->eventfd = -1;
  close(p->interruptfd);
  p->interruptfd = -1;
  ptimer_finalize(&p->timer);
  ctimerq_finalize(p);
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
  pmutex_finalize(&p->tslot_mutex);
  pmutex_finalize(&p->sched_mutex);
  pmutex_finalize(&p->eventfd_mutex);
  pcontext_finalize(&p->context);
  for (pn_handle_t entry = pn_hash_head(p->tslot_map); entry; entry = pn_hash_next(p->tslot_map, entry)) {
    tslot_t *ts = (tslot_t *) pn_hash_value(p->tslot_map, entry);
    pmutex_finalize(&ts->mutex);
    free(ts);
  }
  pn_free(p->tslot_map);
  free(p->kevents);
  free(p->runnables);
  free(p->warm_runnables);
  free(p->resume_list);
  free(p);
}

pn_proactor_t *pn_event_proactor(pn_event_t *e) {
  if (pn_event_class(e) == PN_CLASSCLASS(pn_proactor)) return (pn_proactor_t*)pn_event_context(e);
  pn_listener_t *l = pn_event_listener(e);
  if (l) return l->acceptors[0].psocket.proactor;
  pn_connection_t *c = pn_event_connection(e);
  if (c) return pn_connection_proactor(c);
  return NULL;
}

static void proactor_add_event(pn_proactor_t *p, pn_event_type_t t) {
  pn_collector_put(p->collector, PN_CLASSCLASS(pn_proactor), p, t);
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
  if (e) {
    p->current_event_type = pn_event_type(e);
    if (p->current_event_type == PN_PROACTOR_TIMEOUT)
      p->timeout_processed = true;
  }
  unlock(&p->context.mutex);
  return pni_log_event(p, e);
}

static pn_event_batch_t *proactor_process(pn_proactor_t *p, bool timeout, bool interrupt, bool wake) {
  bool timer_fired = timeout && ptimer_callback(&p->timer) != 0;
  if (interrupt) {
    (void)read_uint64(p->interruptfd);
    rearm(p, &p->epoll_interrupt);
  }
  lock(&p->context.mutex);
  if (interrupt) {
    p->need_interrupt = true;
  }
  if (timeout) {
    p->timer_armed = false;
    if (timer_fired && p->timeout_set) {
      p->need_timeout = true;
    }
  }
  if (wake) {
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
  p->timer_armed = true;
  unlock(&p->context.mutex);
  if (rearm_timer)
    rearm(p, &p->timer.epoll_io);
  return NULL;
}

void proactor_add(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  lock(&p->context.mutex);
  if (p->contexts) {
    p->contexts->prev = ctx;
    ctx->next = p->contexts;
  }
  p->contexts = ctx;
  p->context_count++;
  unlock(&p->context.mutex);
}

// call with psocket's mutex held
// return true if safe for caller to free psocket
bool proactor_remove(pcontext_t *ctx) {
  pn_proactor_t *p = ctx->proactor;
  // Disassociate this context from scheduler
  if (!p->shutting_down) {
    lock(&p->sched_mutex);
    ctx->runner->state = DELETING;
    for (pn_handle_t entry = pn_hash_head(p->tslot_map); entry; entry = pn_hash_next(p->tslot_map, entry)) {
      tslot_t *ts = (tslot_t *) pn_hash_value(p->tslot_map, entry);
      if (ts->context == ctx)
        ts->context = NULL;
      if (ts->prev_context == ctx)
        ts->prev_context = NULL;
    }
    unlock(&p->sched_mutex);
  }

  lock(&p->context.mutex);
  bool can_free = true;
  if (ctx->disconnecting) {
    // No longer on contexts list
    --p->disconnects_pending;
    if (--ctx->disconnect_ops != 0) {
      // procator_disconnect() does the free
      can_free = false;
    }
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
    p->context_count--;
  }
  bool notify = wake_if_inactive(p);
  unlock(&p->context.mutex);
  if (notify) wake_notify(&p->context);
  return can_free;
}

static tslot_t *find_tslot(pn_proactor_t *p) {
  pthread_t tid = pthread_self();
  void *v = pn_hash_get(p->tslot_map, (uintptr_t) tid);
  if (v)
    return (tslot_t *) v;
  tslot_t *ts = (tslot_t *) calloc(1, sizeof(tslot_t));
  ts->state = NEW;
  pmutex_init(&ts->mutex);

  lock(&p->sched_mutex);
  // keep important tslot related info thread-safe when holding either the sched or tslot mutex
  p->thread_count++;
  pn_hash_put(p->tslot_map, (uintptr_t) tid, ts);
  unlock(&p->sched_mutex);
  return ts;
}

// Call with shed_lock held
// Caller must resume() return value if not null
static tslot_t *resume_one_thread(pn_proactor_t *p) {
  // If pn_proactor_get has an early return, we need to resume one suspended thread (if any)
  // to be the new poller.

  tslot_t *ts = p->suspend_list_head;
  if (ts) {
    LL_REMOVE(p, suspend_list, ts);
    p->suspend_list_count--;
    ts->state = PROCESSING;
  }
  return ts;
}

// Called with sched lock, returns with sched lock still held.
static pn_event_batch_t *process(pcontext_t *ctx) {
  bool ctx_wake = false;
  ctx->sched_pending = false;
  if (ctx->sched_wake) {
    // update the wake status before releasing the sched_mutex
    ctx->sched_wake = false;
    ctx_wake = true;
  }
  pn_proactor_t *p = ctx->proactor;
  pn_event_batch_t* batch = NULL;
  switch (ctx->type) {
  case PROACTOR: {
    bool timeout = p->sched_timeout;
    if (timeout) p->sched_timeout = false;
    bool intr = p->sched_interrupt;
    if (intr) p->sched_interrupt = false;
    unlock(&p->sched_mutex);
    batch = proactor_process(p, timeout, intr, ctx_wake);
    break;
  }
  case PCONNECTION: {
    pconnection_t *pc = pcontext_pconnection(ctx);
    uint32_t events = pc->psocket.sched_io_events;
    if (events) pc->psocket.sched_io_events = 0;
    unlock(&p->sched_mutex);
    batch = pconnection_process(pc, events, ctx_wake, false);
    break;
  }
  case CONN_TIMERQ: {
    connection_timerq_t *ctq = &p->ctimerq;
    bool timeout = ctq->sched_timeout;
    if (timeout) ctq->sched_timeout = false;
    unlock(&p->sched_mutex);
    batch = ctimerq_process(ctq, timeout);
    break;
  }
  case LISTENER: {
    pn_listener_t *l = pcontext_listener(ctx);
    int n_events = 0;
    for (size_t i = 0; i < l->acceptors_size; i++) {
      psocket_t *ps = &l->acceptors[i].psocket;
      if (ps->sched_io_events) {
        ps->working_io_events = ps->sched_io_events;
        ps->sched_io_events = 0;
      }
      if (ps->working_io_events)
        n_events++;
    }
    unlock(&p->sched_mutex);
    batch = listener_process(l, n_events, ctx_wake);
    break;
  }
  case RAW_CONNECTION: {
    unlock(&p->sched_mutex);
    batch = pni_raw_connection_process(ctx, ctx_wake);
    break;
  }
  default:
    assert(NULL);
  }
  lock(&p->sched_mutex);
  return batch;
}


// Call with both sched and wake locks
static void schedule_wake_list(pn_proactor_t *p) {
  // append wake_list_first..wake_list_last to end of sched_wake_last
  if (p->wake_list_first) {
    if (p->sched_wake_last)
      p->sched_wake_last->wake_next = p->wake_list_first;  // join them
    if (!p->sched_wake_first)
      p->sched_wake_first = p->wake_list_first;
    p->sched_wake_last = p->wake_list_last;
    if (!p->sched_wake_current)
      p->sched_wake_current = p->sched_wake_first;
    p->wake_list_first = p->wake_list_last = NULL;
  }
}

// Call with schedule lock held.  Called only by poller thread.
static pcontext_t *post_event(pn_proactor_t *p, struct epoll_event *evp) {
  epoll_extended_t *ee = (epoll_extended_t *) evp->data.ptr;
  pcontext_t *ctx = NULL;

  switch (ee->type) {
  case WAKE:
    if  (ee->fd == p->interruptfd) {        /* Interrupts have their own dedicated eventfd */
      p->sched_interrupt = true;
      ctx = &p->context;
      ctx->sched_pending = true;
    } else {
      // main eventfd wake
      lock(&p->eventfd_mutex);
      schedule_wake_list(p);
      ctx = p->sched_wake_current;
      unlock(&p->eventfd_mutex);
    }
    break;

  case PROACTOR_TIMER:
    p->sched_timeout = true;
    ctx = &p->context;
    ctx->sched_pending = true;
    break;

  case CTIMERQ_TIMER: {
    connection_timerq_t *ctq = &p->ctimerq;
    ctx = &ctq->context;
    ctq->sched_timeout = true;
    ctx->sched_pending = true;
    break;
  }
  case PCONNECTION_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    pconnection_t *pc = psocket_pconnection(ps);
    assert(pc);
    ctx = &pc->context;
    ps->sched_io_events = evp->events;
    ctx->sched_pending = true;
    break;
  }
  case LISTENER_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    pn_listener_t *l = psocket_listener(ps);
    assert(l);
    ctx = &l->context;
    ps->sched_io_events = evp->events;
    ctx->sched_pending = true;
    break;
  }
  case RAW_CONNECTION_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    ctx = pni_psocket_raw_context(ps);
    ps->sched_io_events = evp->events;
    ctx->sched_pending = true;
    break;
  }
  }
  if (ctx && !ctx->runnable && !ctx->runner)
    return ctx;
  return NULL;
}


static pcontext_t *post_wake(pn_proactor_t *p, pcontext_t *ctx) {
  ctx->sched_wake = true;
  ctx->sched_pending = true;
  if (!ctx->runnable && !ctx->runner)
    return ctx;
  return NULL;
}

// call with sched_lock held
static pcontext_t *next_drain(pn_proactor_t *p, tslot_t *ts) {
  // This should be called seldomly, best case once per thread removal on shutdown.
  // TODO: how to reduce?  Instrumented near 5 percent of earmarks, 1 in 2000 calls to do_epoll().

  for (pn_handle_t entry = pn_hash_head(p->tslot_map); entry; entry = pn_hash_next(p->tslot_map, entry)) {
    tslot_t *ts2 = (tslot_t *) pn_hash_value(p->tslot_map, entry);
    if (ts2->earmarked) {
      // undo the old assign thread and earmark.  ts2 may never come back
      pcontext_t *switch_ctx = ts2->context;
      remove_earmark(ts2);
      assign_thread(ts, switch_ctx);
      ts->earmark_override = ts2;
      ts->earmark_override_gen = ts2->generation;
      return switch_ctx;
    }
  }
  assert(false);
  return NULL;
}

// call with sched_lock held
static pcontext_t *next_runnable(pn_proactor_t *p, tslot_t *ts) {
  if (ts->context) {
    // Already assigned
    if (ts->earmarked) {
      ts->earmarked = false;
      if (--p->earmark_count == 0)
        p->earmark_drain = false;
    }
    return ts->context;
  }

  // warm pairing ?
  pcontext_t *ctx = ts->prev_context;
  if (ctx && (ctx->runnable)) { // or ctx->sched_wake too?
    assign_thread(ts, ctx);
    return ctx;
  }

  // check for an unassigned runnable context or unprocessed wake
  if (p->n_runnables) {
    // Any unclaimed runnable?
    while (p->n_runnables) {
      ctx = p->runnables[p->next_runnable++];
      if (p->n_runnables == p->next_runnable)
        p->n_runnables = 0;
      if (ctx->runnable) {
        assign_thread(ts, ctx);
        return ctx;
      }
    }
  }

  if (p->sched_wake_current) {
    ctx = p->sched_wake_current;
    pop_wake(ctx);  // updates sched_wake_current
    assert(!ctx->runnable && !ctx->runner);
    assign_thread(ts, ctx);
    return ctx;
  }

  if (p->earmark_drain) {
    ctx = next_drain(p, ts);
    if (p->earmark_count == 0)
      p->earmark_drain = false;
    return ctx;
  }

  return NULL;
}

static pn_event_batch_t *next_event_batch(pn_proactor_t* p, bool can_block) {
  lock(&p->tslot_mutex);
  tslot_t * ts = find_tslot(p);
  unlock(&p->tslot_mutex);
  ts->generation++;  // wrapping OK.  Just looking for any change

  lock(&p->sched_mutex);
  assert(ts->context == NULL || ts->earmarked);
  assert(ts->state == UNUSED || ts->state == NEW);
  ts->state = PROCESSING;

  // Process outstanding epoll events until we get a batch or need to block.
  while (true) {
    // First see if there are any contexts waiting to run and perhaps generate new Proton events,
    pcontext_t *ctx = next_runnable(p, ts);
    if (ctx) {
      ts->state = BATCHING;
      pn_event_batch_t *batch = process(ctx);
      if (batch) {
        unlock(&p->sched_mutex);
        return batch;
      }
      bool notify = unassign_thread(ts, PROCESSING);
      if (notify) {
        unlock(&p->sched_mutex);
        wake_notify(&p->context);
        lock(&p->sched_mutex);
      }
      continue;  // Long time may have passed.  Back to beginning.
    }

    // Poll or wait for a runnable context
    if (p->poller == NULL) {
      bool return_immediately;
      p->poller = ts;
      // Get new epoll events (if any) and mark the relevant contexts as runnable
      return_immediately = poller_do_epoll(p, ts, can_block);
      p->poller = NULL;
      if (return_immediately) {
        // Check if another thread is available to continue epoll-ing.
        tslot_t *res_ts = resume_one_thread(p);
        ts->state = UNUSED;
        unlock(&p->sched_mutex);
        if (res_ts) resume(p, res_ts);
        return NULL;
      }
      poller_done(p, ts);  // put suspended threads to work.
    } else if (!can_block) {
      ts->state = UNUSED;
      unlock(&p->sched_mutex);
      return NULL;
    } else {
      // TODO: loop while !poller_suspended, since new work coming
      suspend(p, ts);
    }
  } // while
}

// Call with sched lock.  Return true if !can_block and no new events to process.
static bool poller_do_epoll(struct pn_proactor_t* p, tslot_t *ts, bool can_block) {
  // As poller with lots to do, be mindful of hogging the sched lock.  Release when making kernel calls.
  int n_events;
  pcontext_t *ctx;

  while (true) {
    assert(p->n_runnables == 0);
    if (p->thread_count > p->thread_capacity)
      grow_poller_bufs(p);
    p->next_runnable = 0;
    p->n_warm_runnables = 0;
    p->last_earmark = NULL;

    bool unfinished_earmarks = p->earmark_count > 0;
    bool new_wakes = false;
    bool epoll_immediate = unfinished_earmarks || !can_block;
    assert(!p->sched_wake_first);
    if (!epoll_immediate) {
      lock(&p->eventfd_mutex);
      if (p->wake_list_first) {
        epoll_immediate = true;
        new_wakes = true;
      } else {
        p->wakes_in_progress = false;
      }
      unlock(&p->eventfd_mutex);
    }
    int timeout = (epoll_immediate) ? 0 : -1;
    p->poller_suspended = (timeout == -1);
    unlock(&p->sched_mutex);

    n_events = epoll_wait(p->epollfd, p->kevents, p->kevents_capacity, timeout);

    lock(&p->sched_mutex);
    p->poller_suspended = false;

    bool unpolled_work = false;
    if (p->earmark_count > 0) {
      p->earmark_drain = true;
      unpolled_work = true;
    }
    if (new_wakes) {
      lock(&p->eventfd_mutex);
      schedule_wake_list(p);
      unlock(&p->eventfd_mutex);
      unpolled_work = true;
    }

    if (n_events < 0) {
      if (errno != EINTR)
        perror("epoll_wait"); // TODO: proper log
      if (!can_block && !unpolled_work)
        return true;
      else
        continue;
    } else if (n_events == 0) {
      if (!can_block && !unpolled_work)
        return true;
      else {
        if (!epoll_immediate)
          perror("epoll_wait unexpected timeout"); // TODO: proper log
        if (!unpolled_work)
          continue;
      }
    }

    break;
  }

  // We have unpolled work or at least one new epoll event


  for (int i = 0; i < n_events; i++) {
    ctx = post_event(p, &p->kevents[i]);
    if (ctx)
      make_runnable(ctx);
  }
  if (n_events > 0)
    memset(p->kevents, 0, sizeof(struct epoll_event) * n_events);

  // The list of pending wakes can be very long.  Traverse part of it looking for warm pairings.
  pcontext_t *wctx = p->sched_wake_current;
  int max_runnables = p->runnables_capacity;
  while (wctx && p->n_runnables < max_runnables) {
    if (wctx->runner == REWAKE_PLACEHOLDER)
      wctx->runner = NULL;  // Allow context to run again.
    ctx = post_wake(p, wctx);
    if (ctx)
      make_runnable(ctx);
    pop_wake(wctx);
    wctx = wctx->wake_next;
  }
  p->sched_wake_current = wctx;
  // More wakes than places on the runnables list
  while (wctx) {
    if (wctx->runner == REWAKE_PLACEHOLDER)
      wctx->runner = NULL;  // Allow context to run again.
    wctx->sched_wake = true;
    wctx->sched_pending = true;
    if (wctx->runnable || wctx->runner)
      pop_wake(wctx);
    wctx = wctx->wake_next;
  }

  if (pni_immediate && !ts->context) {
    // Poller gets to run if possible
    pcontext_t *pctx;
    if (p->n_runnables) {
      assert(p->next_runnable == 0);
      pctx = p->runnables[0];
      if (++p->next_runnable == p->n_runnables)
        p->n_runnables = 0;
    } else if (p->n_warm_runnables) {
      pctx = p->warm_runnables[--p->n_warm_runnables];
      tslot_t *ts2 = pctx->runner;
      ts2->prev_context = ts2->context = NULL;
      pctx->runner = NULL;
    } else if (p->last_earmark) {
      pctx = p->last_earmark->context;
      remove_earmark(p->last_earmark);
      if (p->earmark_count == 0)
        p->earmark_drain = false;
    } else {
      pctx = NULL;
    }
    if (pctx) {
      assign_thread(ts, pctx);
    }
  }
  return false;
}

// Call with sched lock, but only from poller context.
static void poller_done(struct pn_proactor_t* p, tslot_t *ts) {
  // Create a list of available threads to put to work.
  // ts is the poller thread
  int resume_list_count = 0;
  tslot_t **resume_list2 = NULL;

  if (p->suspend_list_count) {
    int max_resumes = p->n_warm_runnables + p->n_runnables;
    max_resumes = pn_min(max_resumes, p->suspend_list_count);
    if (max_resumes) {
      resume_list2 = (tslot_t **) alloca(max_resumes * sizeof(tslot_t *));
      for (int i = 0; i < p->n_warm_runnables ; i++) {
        pcontext_t *ctx = p->warm_runnables[i];
        tslot_t *tsp = ctx->runner;
        if (tsp->state == SUSPENDED) {
          resume_list2[resume_list_count++] = tsp;
          LL_REMOVE(p, suspend_list, tsp);
          p->suspend_list_count--;
          tsp->state = PROCESSING;
        }
      }

      int can_use = p->suspend_list_count;
      if (!ts->context)
        can_use++;
      // Run as many unpaired runnable contexts as possible and allow for a new poller.
      int new_runners = pn_min(p->n_runnables + 1, can_use);
      if (!ts->context)
        new_runners--;  // poller available and does not need resume

      // Rare corner case on startup.  New inbound threads can make the suspend_list too big for resume list.
      new_runners = pn_min(new_runners, p->thread_capacity - resume_list_count);

      for (int i = 0; i < new_runners; i++) {
        tslot_t *tsp = p->suspend_list_head;
        assert(tsp);
        resume_list2[resume_list_count++] = tsp;
        LL_REMOVE(p, suspend_list, tsp);
        p->suspend_list_count--;
        tsp->state = PROCESSING;
      }
    }
  }
  p->poller = NULL;

  if (resume_list_count) {
    // Allows a new poller to run concurrently.  Touch only stack vars.
    unlock(&p->sched_mutex);
    for (int i = 0; i < resume_list_count; i++) {
      resume(p, resume_list2[i]);
    }
    lock(&p->sched_mutex);
  }
}

pn_event_batch_t *pn_proactor_wait(struct pn_proactor_t* p) {
  return next_event_batch(p, true);
}

pn_event_batch_t *pn_proactor_get(struct pn_proactor_t* p) {
  return next_event_batch(p, false);
}

// Call with no locks
static inline void check_earmark_override(pn_proactor_t *p, tslot_t *ts) {
  if (!ts || !ts->earmark_override)
    return;
  if (ts->earmark_override->generation == ts->earmark_override_gen) {
    // Other (overridden) thread not seen since this thread started and finished the event batch.
    // Thread is perhaps gone forever, which may leave us short of a poller thread
    lock(&p->sched_mutex);
    tslot_t *res_ts = resume_one_thread(p);
    unlock(&p->sched_mutex);
    if (res_ts) resume(p, res_ts);
  }
  ts->earmark_override = NULL;
}

void pn_proactor_done(pn_proactor_t *p, pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (pc) {
    tslot_t *ts = pc->context.runner;
    pconnection_done(pc);
    // pc possibly freed/invalid
    check_earmark_override(p, ts);
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    tslot_t *ts = l->context.runner;
    listener_done(l);
    // l possibly freed/invalid
    check_earmark_override(p, ts);
    return;
  }
  praw_connection_t *rc = pni_batch_raw_connection(batch);
  if (rc) {
    tslot_t *ts = pni_raw_connection_context(rc)->runner;
    pni_raw_connection_done(rc);
    // rc possibly freed/invalid
    check_earmark_override(p, ts);
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
    lock(&p->sched_mutex);
    tslot_t *ts = p->context.runner;
    if (unassign_thread(ts, UNUSED))
      notify = true;
    unlock(&p->sched_mutex);

    if (notify)
      wake_notify(&p->context);
    if (rearm_timer)
      rearm(p, &p->timer.epoll_io);
    check_earmark_override(p, ts);
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
    p->context_count--;
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
    bool ctx_notify = false;
    pmutex *ctx_mutex = NULL;
    // TODO: Need to extend this for raw connections too
    pconnection_t *pc = pcontext_pconnection(ctx);
    if (pc) {
      ctx_mutex = &pc->context.mutex;
      lock(ctx_mutex);
      if (!ctx->closing) {
        ctx_notify = true;
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
        ctx_notify = true;
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
      if (ctx_notify)
        wake_notify(ctx);
    }
    unlock(&p->context.mutex);
    unlock(ctx_mutex);

    // Unsafe to touch ctx after lock release, except if we are the designated final_free
    if (do_free) {
      if (pc) pconnection_final_free(pc);
      else listener_final_free(pcontext_listener(ctx));
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
  return (pn_millis_t) pn_proactor_now_64();
}

int64_t pn_proactor_now_64(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
