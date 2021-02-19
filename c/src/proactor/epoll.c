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

 A serialized grouping of Proton events is a task (connection, listener, proactor).
 Each has multiple pollable fds that make it schedulable.  E.g. a connection could have a
 socket fd, (indirect) timerfd, and (indirect) eventfd all signaled in a single epoll_wait().

 At the conclusion of each
      N = epoll_wait(..., N_MAX, timeout)

 there will be N epoll events and M tasks on a ready list.  M can be very large in a
 server with many active connections. The poller makes the tasks "runnable" if they are
 not already running.  A running task cannot be made runnable again until it completes
 a chunk of work and calls unassign_thread().  (N + M - duplicates) tasks will be
 scheduled.  A new poller will occur when next_runnable() returns NULL.

 A task may have its own dedicated kernel file descriptor (socket, timerfd) which can be seen
 by the poller to make the task runnable.  A task may aslso be scheduled to run by placing it
 on a ready queue which is monitored by the poller via two eventfds.

 Lock ordering - never add locks right to left:
    task -> sched -> ready
    non-proactor-task -> proactor-task
    tslot -> sched

 TODO: document role of sched_pending and how sched_XXX (i.e. sched_interrupt)
 transitions from "private to the scheduler" to "visible to the task".
 TODO: document task.working duration can be long: from xxx_process() to xxx_done() or null batch.
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

// TODO: logging in general
// SIGPIPE?
// Can some of the mutexes be spinlocks (any benefit over adaptive pthread mutex)?
//   Maybe futex is even better?
// See other "TODO" in code.
//
// Consider case of large number of ready tasks: next_event_batch() could start by
// looking for ready tasks before a kernel call to epoll_wait(), or there
// could be several eventfds with random assignment of wakeables.


/* Like strerror_r but provide a default message if strerror_r fails */
void pstrerror(int err, strerrorbuf msg) {
  int e = strerror_r(err, msg, sizeof(strerrorbuf));
  if (e) snprintf(msg, sizeof(strerrorbuf), "unknown error %d", err);
}

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

/* Read from an event FD */
static uint64_t read_uint64(int fd) {
  uint64_t result = 0;
  ssize_t n = read(fd, &result, sizeof(result));
  if (n != sizeof(result) && !(n < 0 && errno == EAGAIN)) {
    EPOLL_FATAL("eventfd read error", errno);
  }
  return result;
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
 * The proactor maintains a number of serialization tasks: each
 * connection, each listener, the proactor itself.  The serialization
 * is presented to the application via each associated event batch.
 *
 * A task will only ever run in a single thread at a time.
 *
 * Other threads of excution (including user threads) can interact with a
 * particular task (i.e. connection wake or proactor interrupt). Mutexes are
 * needed here for shared access to task data.  schedule()/notify_poller() are
 * used to ensure a task will run to act on the changed data.
 *
 * To minimize trips through the kernel, schedule() is a no-op if the task is
 * already running or about to run.  Conversely, a task must never stop working
 * without checking state that may have been recently changed by another thread.
 *
 * External wake operations, like pn_connection_wake() or expired timers are
 * built on top of this schedule() mechanism.
 *
 * pn_proactor_interrupt() must be async-signal-safe so it has a dedicated
 * eventfd to allow a lock-free pn_proactor_interrupt() implementation.
 */


// Fake thread for temporarily disabling the scheduling of a task.
static struct tslot_t *RESCHEDULE_PLACEHOLDER = (struct tslot_t*) -1;

void task_init(task_t *tsk, task_type_t t, pn_proactor_t *p) {
  memset(tsk, 0, sizeof(*tsk));
  pmutex_init(&tsk->mutex);
  tsk->proactor = p;
  tsk->type = t;
}

/*
 * schedule() strategy with eventfd:
 *  - tasks can be in the ready list only once
 *  - a scheduling thread will only activate the eventfd if ready_list_active is false
 * There is a single rearm between ready list empty and non-empty
 *
 * There can potentially be many tasks with work pending.
 *
 * The ready list is in two parts.  The front is the chunk the
 * poller will process until the next epoll_wait().  sched_ready
 * indicates which chunk it is on. The task may already be running or
 * scheduled to run.
 *
 * The task must be actually running to absorb task_t->ready.
 *
 * The ready list can keep growing while popping ready tasks.  The list between
 * sched_ready_first and sched_ready_last are protected by the sched
 * lock (for pop operations), sched_ready_last to ready_list_last are
 * protected by the eventfd mutex (for add operations).  Both locks
 * are needed to cross or reconcile the two portions of the list.
 */

// Call with sched lock held.
static void pop_ready_task(task_t *tsk) {
  // every task on the sched_ready_list is either currently running,
  // or to be scheduled.  schedule() will not "see" any of the ready_next
  // pointers until ready and working have transitioned to 0
  // and false, when a task stops working.
  //
  // every task must transition as:
  //
  // !ready .. schedule() .. on ready_list .. on sched_ready_list .. working task .. !sched_ready && !ready
  //
  // Intervening locks at each transition ensures ready_next has memory coherence throughout the ready task scheduling cycle.
  pn_proactor_t *p = tsk->proactor;
  if (tsk == p->sched_ready_current)
    p->sched_ready_current = tsk->ready_next;
  if (tsk == p->sched_ready_first) {
    // normal code path
    if (tsk == p->sched_ready_last) {
      p->sched_ready_first = p->sched_ready_last = NULL;
    } else {
      p->sched_ready_first = tsk->ready_next;
    }
    if (!p->sched_ready_first)
      p->sched_ready_last = NULL;
  } else {
    // tsk is not first in a multi-element list
    task_t *prev = NULL;
    for (task_t *i = p->sched_ready_first; i != tsk; i = i->ready_next)
      prev = i;
    prev->ready_next = tsk->ready_next;
    if (tsk == p->sched_ready_last)
      p->sched_ready_last = prev;
  }
  tsk->on_ready_list = false;
}

// part1: call with tsk->owner lock held, return true if notify_poller required by caller.
// Nothing to do if the task is currently at work or work is already pending.
bool schedule(task_t *tsk) {
  bool notify = false;

  if (!tsk->ready) {
    if (!tsk->working) {
      tsk->ready = true;
      pn_proactor_t *p = tsk->proactor;
      lock(&p->eventfd_mutex);
      tsk->ready_next = NULL;
      tsk->on_ready_list = true;
      if (!p->ready_list_first) {
        p->ready_list_first = p->ready_list_last = tsk;
      } else {
        p->ready_list_last->ready_next = tsk;
        p->ready_list_last = tsk;
      }
      if (!p->ready_list_active) {
        // unblock poller via the eventfd
        p->ready_list_active = true;
        notify = true;
      }
      unlock(&p->eventfd_mutex);
    }
  }

  return notify;
}

// part2: unblock epoll_wait().  Make OS call without lock held.
void notify_poller(pn_proactor_t *p) {
  if (p->eventfd == -1)
    return;
  rearm(p, &p->epoll_schedule);
}

// call with task lock held from xxx_process().
void schedule_done(task_t *tsk) {
//  assert(tsk->ready > 0);
  tsk->ready = false;
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
static void assign_thread(tslot_t *ts, task_t *tsk) {
  assert(!tsk->runner);
  tsk->runner = ts;
  tsk->prev_runner = NULL;
  tsk->runnable = false;
  ts->task = tsk;
  ts->prev_task = NULL;
}

// call with sched lock
static bool reschedule(task_t *tsk) {
  // Special case schedule() where task is done/unassigned but sched_pending work has arrived.
  // Should be an infrequent corner case.
  bool notify = false;
  pn_proactor_t *p = tsk->proactor;
  lock(&p->eventfd_mutex);
  assert(tsk->ready);
  assert(!tsk->on_ready_list);
  tsk->ready_next = NULL;
  tsk->on_ready_list = true;
  if (!p->ready_list_first) {
    p->ready_list_first = p->ready_list_last = tsk;
  } else {
    p->ready_list_last->ready_next = tsk;
    p->ready_list_last = tsk;
  }
  if (!p->ready_list_active) {
    // unblock the poller via the eventfd
    p->ready_list_active = true;
    notify = true;
  }
  unlock(&p->eventfd_mutex);
  return notify;
}

// Call with sched lock
bool unassign_thread(tslot_t *ts, tslot_state new_state) {
  task_t *tsk = ts->task;
  bool notify = false;
  bool deleting = (ts->state == DELETING);
  ts->task = NULL;
  ts->state = new_state;
  if (tsk) {
    tsk->runner = NULL;
    tsk->prev_runner = ts;
  }

  // Check if unseen events or schedule() calls occurred while task was working.

  if (tsk && !deleting) {
    pn_proactor_t *p = tsk->proactor;
    ts->prev_task = tsk;
    if (tsk->sched_pending) {
      // Make sure the task is already scheduled or put it on the ready list
      if (tsk->sched_ready) {
        if (!tsk->on_ready_list) {
          // Remember it for next poller
          tsk->sched_ready = false;
          notify = reschedule(tsk);     // back on ready list for poller to see
        }
        // else already scheduled
      } else {
        // bad corner case.  Block tsk from being scheduled again until a later post_ready()
        tsk->runner = RESCHEDULE_PLACEHOLDER;
        unlock(&p->sched_mutex);
        lock(&tsk->mutex);
        notify = schedule(tsk);
        unlock(&tsk->mutex);
        lock(&p->sched_mutex);
      }
    }
  }
  return notify;
}

// Call with sched lock
static void earmark_thread(tslot_t *ts, task_t *tsk) {
  assign_thread(ts, tsk);
  ts->earmarked = true;
  tsk->proactor->earmark_count++;
}

// Call with sched lock
static void remove_earmark(tslot_t *ts) {
  task_t *tsk = ts->task;
  ts->task = NULL;
  tsk->runner = NULL;
  ts->earmarked = false;
  tsk->proactor->earmark_count--;
}

// Call with sched lock
static void make_runnable(task_t *tsk) {
  pn_proactor_t *p = tsk->proactor;
  assert(p->n_runnables <= p->runnables_capacity);
  assert(!tsk->runnable);
  if (tsk->runner) return;

  tsk->runnable = true;
  // Track it as normal or warm or earmarked
  if (pni_warm_sched) {
    tslot_t *ts = tsk->prev_runner;
    if (ts && ts->prev_task == tsk) {
      if (ts->state == SUSPENDED || ts->state == PROCESSING) {
        if (p->n_warm_runnables < p->thread_capacity) {
          p->warm_runnables[p->n_warm_runnables++] = tsk;
          assign_thread(ts, tsk);
        }
        else
          p->runnables[p->n_runnables++] = tsk;
        return;
      }
      if (ts->state == UNUSED && !p->earmark_drain) {
        earmark_thread(ts, tsk);
        p->last_earmark = ts;
        return;
      }
    }
  }
  p->runnables[p->n_runnables++] = tsk;
}



void psocket_init(psocket_t* ps, epoll_type_t type)
{
  ps->epoll_io.fd = -1;
  ps->epoll_io.type = type;
  ps->epoll_io.wanted = 0;
  ps->epoll_io.polling = false;
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

static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool sched_ready, bool topup);
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

static inline pconnection_t *task_pconnection(task_t *t) {
  return t->type == PCONNECTION ? containerof(t, pconnection_t, task) : NULL;
}

static inline pn_listener_t *task_listener(task_t *t) {
  return t->type == LISTENER ? containerof(t, pn_listener_t, task) : NULL;
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

// Add an overflowing acceptor to the overflow list. Called with listener task lock held.
static void acceptor_set_overflow(acceptor_t *a) {
  a->overflowed = true;
  pn_proactor_t *p = a->listener->task.proactor;
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
    lock(&l->task.mutex);
    bool rearming = !l->task.closing;
    bool notify = false;
    assert(!a->armed);
    assert(a->overflowed);
    a->overflowed = false;
    if (rearming) {
      rearm(p, &a->psocket.epoll_io);
      a->armed = true;
    }
    else notify = schedule(&l->task);
    unlock(&l->task.mutex);
    if (notify) notify_poller(p);
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
  if (!(pc->timer = pni_timer(&p->timer_manager, pc))) {
    free(pc);
    return "connection timer creation failure";
  }


  task_init(&pc->task, PCONNECTION, p);
  psocket_init(&pc->psocket, PCONNECTION_IO);
  pni_parse_addr(addr, pc->addr_buf, addrlen+1, &pc->host, &pc->port);
  pc->new_events = 0;
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

// Call with lock held and closing == true (i.e. pn_connection_driver_finished() == true), no pending timer.
// Return true when all possible outstanding epoll events associated with this pconnection have been processed.
static inline bool pconnection_is_final(pconnection_t *pc) {
  return !pc->current_arm && !pc->task.ready && !pc->tick_pending;
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
  task_finalize(&pc->task);
  pni_timer_free(pc->timer);
  free(pc);
}


// call without lock
static void pconnection_cleanup(pconnection_t *pc) {
  assert(pconnection_is_final(pc));
  int fd = pc->psocket.epoll_io.fd;
  stop_polling(&pc->psocket.epoll_io, pc->task.proactor->epollfd);
  if (fd != -1)
    pclosefd(pc->task.proactor, fd);

  lock(&pc->task.mutex);
  bool can_free = proactor_remove(&pc->task);
  unlock(&pc->task.mutex);
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
  if (!pc->task.closing) {
    pc->task.closing = true;
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
  pc->task.ready = 0;
  pn_collector_release(pc->driver.collector);
  assert(pconnection_is_final(pc));
  pconnection_cleanup(pc);
}

// Called from timer_manager with no locks.
void pni_pconnection_timeout(pconnection_t  *pc) {
  bool notify = false;
  uint64_t now = pn_proactor_now_64();
  lock(&pc->task.mutex);
  if (!pc->task.closing) {
    // confirm no simultaneous timeout change from another thread.
    if (pc->expected_timeout && now >= pc->expected_timeout) {
      pc->tick_pending = true;
      pc->expected_timeout = 0;
      notify = schedule(&pc->task);
    }
  }
  unlock(&pc->task.mutex);
  if (notify)
    notify_poller(pc->task.proactor);
}

static pn_event_t *pconnection_batch_next(pn_event_batch_t *batch) {
  pconnection_t *pc = batch_pconnection(batch);
  if (!pc->driver.connection) return NULL;
  pn_event_t *e = pn_connection_driver_next_event(&pc->driver);
  if (!e) {
    pn_proactor_t *p = pc->task.proactor;
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

/* Call only from working task (no competitor for pc->current_arm or
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
  rearm(pc->task.proactor, &pc->psocket.epoll_io);
  unlock(&pc->rearm_mutex);
  // Return immediately.  pc may have just been freed by another thread.
}

/* Only call when context switch is imminent.  Sched lock is highly contested. */
// Call with both task and sched locks.
static bool pconnection_sched_sync(pconnection_t *pc) {
  uint32_t sync_events = 0;
  uint32_t sync_args = pc->tick_pending << 1;
  if (pc->psocket.sched_io_events) {
    pc->new_events = pc->psocket.sched_io_events;
    pc->psocket.sched_io_events = 0;
    pc->current_arm = 0;  // or outside lock?
    sync_events = pc->new_events;
  }
  if (pc->task.sched_ready) {
    pc->task.sched_ready = false;
    schedule_done(&pc->task);
    sync_args |= 1;
  }
  pc->task.sched_pending = false;

  if (sync_args || sync_events) {
    // Only replace if poller has found new work for us.
    pc->process_args = (1 << 2) | sync_args;
    pc->process_events = sync_events;
  }

  // Indicate if there are free proactor threads
  pn_proactor_t *p = pc->task.proactor;
  return p->poller_suspended || p->suspend_list_head;
}

/* Call with task lock and having done a write_flush() to "know" the value of wbuf_remaining */
static inline bool pconnection_work_pending(pconnection_t *pc) {
  if (pc->new_events || pni_task_wake_pending(&pc->task) || pc->tick_pending || pc->queued_disconnect)
    return true;
  if (!pc->read_blocked && !pconnection_rclosed(pc))
    return true;
  return (pc->wbuf_remaining > 0 && !pc->write_blocked);
}

/* Call with no locks. */
static void pconnection_done(pconnection_t *pc) {
  pn_proactor_t *p = pc->task.proactor;
  tslot_t *ts = pc->task.runner;
  write_flush(pc);
  bool notify = false;
  bool self_sched = false;
  lock(&pc->task.mutex);
  pc->task.working = false;  // So we can schedule() ourself if necessary.  We remain the de facto
                             // working task instance while the lock is held.  Need sched_sync too to drain
                             // a possible stale sched_ready.
  pc->hog_count = 0;
  bool has_event = pconnection_has_event(pc);
  // Do as little as possible while holding the sched lock
  lock(&p->sched_mutex);
  pconnection_sched_sync(pc);
  unlock(&p->sched_mutex);

  if (has_event || pconnection_work_pending(pc)) {
    self_sched = true;
  } else if (pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->task.mutex);
      pconnection_cleanup(pc);
      // pc may be undefined now
      lock(&p->sched_mutex);
      notify = unassign_thread(ts, UNUSED);
      unlock(&p->sched_mutex);
      if (notify)
        notify_poller(p);
      return;
    }
  }
  if (self_sched)
    notify = schedule(&pc->task);

  int wanted = pconnection_rearm_check(pc);
  unlock(&pc->task.mutex);

  if (wanted) pconnection_rearm(pc, wanted);  // May free pc on another thread.  Return without touching pc again.
  lock(&p->sched_mutex);
  if (unassign_thread(ts, UNUSED))
    notify = true;
  unlock(&p->sched_mutex);
  if (notify) notify_poller(p);
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

static pn_event_batch_t *pconnection_process(pconnection_t *pc, uint32_t events, bool sched_ready, bool topup) {
  bool waking = false;
  bool tick_required = false;
  bool immediate_write = false;
  lock(&pc->task.mutex);
  if (!topup) { // Save some state in case of crash investigation.
    pc->process_events = events;
    pc->process_args = (pc->tick_pending << 1) | sched_ready;
  }
  if (events) {
    pc->new_events = events;
    pc->current_arm = 0;
    events = 0;
  }
  if (sched_ready) schedule_done(&pc->task);

  if (topup) {
    // Only called by the batch owner.  Does not loop, just "tops up"
    // once.  May be back depending on hog_count.
    assert(pc->task.working);
  }
  else {
    if (pc->task.working) {
      // Another thread is the working task.  Should be impossible with new scheduler.
      EPOLL_FATAL("internal epoll proactor error: two worker threads", 0);
    }
    pc->task.working = true;
  }

  // Confirmed as working thread.  Review state and unlock ASAP.

 retry:

  if (pc->queued_disconnect) {  // From pn_proactor_disconnect()
    pc->queued_disconnect = false;
    if (!pc->task.closing) {
      if (pc->disconnect_condition) {
        pn_condition_copy(pn_transport_condition(pc->driver.transport), pc->disconnect_condition);
      }
      pn_connection_driver_close(&pc->driver);
    }
  }

  if (pconnection_has_event(pc)) {
    unlock(&pc->task.mutex);
    return &pc->batch;
  }
  bool closed = pconnection_rclosed(pc) && pconnection_wclosed(pc);
  if (pni_task_wake_pending(&pc->task)) {
    waking = !closed;
    pni_task_wake_done(&pc->task);
  }
  if (pc->tick_pending) {
    pc->tick_pending = false;
    tick_required = !closed;
  }

  if (pc->new_events) {
    uint32_t update_events = pc->new_events;
    pc->current_arm = 0;
    pc->new_events = 0;
    if (!pc->task.closing) {
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

  if (pc->task.closing && pconnection_is_final(pc)) {
    unlock(&pc->task.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

  unlock(&pc->task.mutex);
  pc->hog_count++; // working task doing work

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

  lock(&pc->task.mutex);
  if (pc->task.closing && pconnection_is_final(pc)) {
    unlock(&pc->task.mutex);
    pconnection_cleanup(pc);
    return NULL;
  }

  // Never stop working while work remains.  hog_count exception to this rule is elsewhere.
  lock(&pc->task.proactor->sched_mutex);
  bool workers_free = pconnection_sched_sync(pc);
  unlock(&pc->task.proactor->sched_mutex);

  if (pconnection_work_pending(pc)) {
    goto retry;  // TODO: get rid of goto without adding more locking
  }

  pc->task.working = false;
  pc->hog_count = 0;
  if (pn_connection_driver_finished(&pc->driver)) {
    pconnection_begin_close(pc);
    if (pconnection_is_final(pc)) {
      unlock(&pc->task.mutex);
      pconnection_cleanup(pc);
      return NULL;
    }
  }

  if (workers_free && !pc->task.closing && !pc->io_doublecheck) {
    // check one last time for new io before context switch
    pc->io_doublecheck = true;
    pc->read_blocked = false;
    pc->write_blocked = false;
    pc->task.working = true;
    goto retry;
  }

  int wanted = pconnection_rearm_check(pc);  // holds rearm_mutex until pconnection_rearm() below

  unlock(&pc->task.mutex);
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

/* Called with task.lock held */
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
  int efd = pc->task.proactor->epollfd;
  /* Get the local socket name now, get the peer name in pconnection_connected */
  socklen_t len = sizeof(pc->local.ss);
  (void)getsockname(fd, (struct sockaddr*)&pc->local.ss, &len);

  epoll_extended_t *ee = &pc->psocket.epoll_io;
  if (ee->polling) {     /* This is not the first attempt, stop polling and close the old FD */
    int fd = ee->fd;     /* Save fd, it will be set to -1 by stop_polling */
    stop_polling(ee, efd);
    pclosefd(pc->task.proactor, fd);
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
  return (!p->tasks && !p->disconnects_pending && !p->timeout_set && !p->shutting_down);
}

/* If inactive set need_inactive and return true if poller needs to be unblocked */
bool schedule_if_inactive(pn_proactor_t *p) {
  if (is_inactive(p)) {
    p->need_inactive = true;
    return schedule(&p->task);
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

  lock(&pc->task.mutex);
  proactor_add(&pc->task);
  pn_connection_open(pc->driver.connection); /* Auto-open */

  bool notify = false;

  if (pc->disconnected) {
    notify = schedule(&pc->task);    /* Error during initialization */
  } else {
    int gai_error = pgetaddrinfo(pc->host, pc->port, 0, &pc->addrinfo);
    if (!gai_error) {
      pn_connection_open(pc->driver.connection); /* Auto-open */
      pc->ai = pc->addrinfo;
      pconnection_maybe_connect_lh(pc); /* Start connection attempts */
      if (pc->disconnected) notify = schedule(&pc->task);
    } else {
      psocket_gai_error(&pc->psocket, gai_error, "connect to ");
      notify = schedule(&pc->task);
      lock(&p->task.mutex);
      notify |= schedule_if_inactive(p);
      unlock(&p->task.mutex);
    }
  }
  /* We need to issue INACTIVE on immediate failure */
  unlock(&pc->task.mutex);
  if (notify) notify_poller(pc->task.proactor);
}

static void pconnection_tick(pconnection_t *pc) {
  pn_transport_t *t = pc->driver.transport;
  if (pn_transport_get_idle_timeout(t) || pn_transport_get_remote_idle_timeout(t)) {
    uint64_t now = pn_proactor_now_64();
    uint64_t next = pn_transport_tick(t, now);
    if (next) {
      lock(&pc->task.mutex);
      pc->expected_timeout = next;
      unlock(&pc->task.mutex);
      pni_timer_set(pc->timer, next);
    }
  }
}

void pn_connection_wake(pn_connection_t* c) {
  bool notify = false;
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    lock(&pc->task.mutex);
    if (!pc->task.closing) {
      notify = pni_task_wake(&pc->task);
    }
    unlock(&pc->task.mutex);
  }
  if (notify) notify_poller(pc->task.proactor);
}

void pn_proactor_release_connection(pn_connection_t *c) {
  bool notify = false;
  pconnection_t *pc = get_pconnection(c);
  if (pc) {
    set_pconnection(c, NULL);
    lock(&pc->task.mutex);
    pn_connection_driver_release_connection(&pc->driver);
    pconnection_begin_close(pc);
    notify = schedule(&pc->task);
    unlock(&pc->task.mutex);
  }
  if (notify) notify_poller(pc->task.proactor);
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
    task_init(&l->task, LISTENER, unknown);
  }
  return l;
}

void pn_proactor_listen(pn_proactor_t *p, pn_listener_t *l, const char *addr, int backlog)
{
  // TODO: check listener not already listening for this or another proactor
  lock(&l->task.mutex);
  l->task.proactor = p;

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
          psocket_init(ps, LISTENER_IO);
          ps->epoll_io.fd = fd;
          ps->epoll_io.wanted = EPOLLIN;
          ps->epoll_io.polling = false;
          start_polling(&ps->epoll_io, l->task.proactor->epollfd);  // TODO: check for error
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
  bool notify = schedule(&l->task);

  if (l->acceptors_size == 0) { /* All failed, create dummy socket with an error */
    l->acceptors = (acceptor_t*)realloc(l->acceptors, sizeof(acceptor_t));
    l->acceptors_size = 1;
    memset(l->acceptors, 0, sizeof(acceptor_t));
    psocket_init(&l->acceptors[0].psocket, LISTENER_IO);
    l->acceptors[0].listener = l;
    if (gai_err) {
      psocket_gai_error(&l->acceptors[0].psocket, gai_err, "listen on");
    } else {
      psocket_error(&l->acceptors[0].psocket, errno, "listen on");
    }
  } else {
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_OPEN);
  }
  proactor_add(&l->task);
  unlock(&l->task.mutex);
  if (notify) notify_poller(p);
  return;
}

// call with lock held and task.working false
static inline bool listener_can_free(pn_listener_t *l) {
  return l->task.closing && l->close_dispatched && !l->task.ready && !l->active_count;
}

static inline void listener_final_free(pn_listener_t *l) {
  task_finalize(&l->task);
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
    lock(&l->task.mutex);
    if (l->task.proactor) {
      can_free = proactor_remove(&l->task);
    }
    unlock(&l->task.mutex);
    if (can_free)
      listener_final_free(l);
  }
}

/* Always call with lock held so it can be unlocked around overflow processing. */
static void listener_begin_close(pn_listener_t* l) {
  if (!l->task.closing) {
    l->task.closing = true;

    /* Close all listening sockets */
    for (size_t i = 0; i < l->acceptors_size; ++i) {
      acceptor_t *a = &l->acceptors[i];
      psocket_t *ps = &a->psocket;
      if (ps->epoll_io.fd >= 0) {
        if (a->armed) {
          shutdown(ps->epoll_io.fd, SHUT_RD);  // Force epoll event and callback
        } else {
          int fd = ps->epoll_io.fd;
          stop_polling(&ps->epoll_io, l->task.proactor->epollfd);
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

    unlock(&l->task.mutex);
    /* Remove all acceptors from the overflow list.  closing flag prevents re-insertion.*/
    proactor_rearm_overflow(pn_listener_proactor(l));
    lock(&l->task.mutex);
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_CLOSE);
  }
}

void pn_listener_close(pn_listener_t* l) {
  bool notify = false;
  lock(&l->task.mutex);
  if (!l->task.closing) {
    listener_begin_close(l);
    notify = schedule(&l->task);
  }
  unlock(&l->task.mutex);
  if (notify) notify_poller(l->task.proactor);
}

static void listener_forced_shutdown(pn_listener_t *l) {
  // Called by proactor_free, no competing threads, no epoll activity.
  lock(&l->task.mutex); // needed because of interaction with proactor_rearm_overflow
  listener_begin_close(l);
  unlock(&l->task.mutex);
  // pconnection_process will never be called again.  Zero everything.
  l->task.ready = 0;
  l->close_dispatched = true;
  l->active_count = 0;
  assert(listener_can_free(l));
  pn_listener_free(l);
}

/* Accept a connection as part of listener_process(). Called with listener task lock held. */
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
static pn_event_batch_t *listener_process(pn_listener_t *l, int n_events, bool tsk_ready) {
  // TODO: some parallelization of the accept mechanism.
//  pn_listener_t *l = psocket_listener(ps);
//  acceptor_t *a = psocket_acceptor(ps);

  lock(&l->task.mutex);
  if (n_events) {
    for (size_t i = 0; i < l->acceptors_size; i++) {
      psocket_t *ps = &l->acceptors[i].psocket;
      if (ps->working_io_events) {
        uint32_t events = ps->working_io_events;
        ps->working_io_events = 0;
        if (l->task.closing) {
          l->acceptors[i].armed = false;
          int fd = ps->epoll_io.fd;
          stop_polling(&ps->epoll_io, l->task.proactor->epollfd);
          close(fd);
          l->active_count--;
        } else {
          l->acceptors[i].armed = false;
          if (events & EPOLLRDHUP) {
            /* Calls listener_begin_close which closes all the listener's sockets */
            psocket_error(ps, errno, "listener epoll");
          } else if (!l->task.closing && events & EPOLLIN) {
            listener_accept_lh(ps);
          }
        }
      }
    }
  }
  if (tsk_ready) {
    schedule_done(&l->task); // callback accounting
  }
  pn_event_batch_t *lb = NULL;
  if (!l->task.working) {
    l->task.working = true;
    if (listener_has_event(l))
      lb = &l->batch;
    else {
      l->task.working = false;
      if (listener_can_free(l)) {
        unlock(&l->task.mutex);
        pn_listener_free(l);
        return NULL;
      }
    }
  }
  unlock(&l->task.mutex);
  return lb;
}

static pn_event_t *listener_batch_next(pn_event_batch_t *batch) {
  pn_listener_t *l = batch_listener(batch);
  lock(&l->task.mutex);
  pn_event_t *e = pn_collector_next(l->collector);
  if (!e && l->pending_count) {
    // empty collector means pn_collector_put() will not coalesce
    pn_collector_put(l->collector, PN_CLASSCLASS(pn_listener), l, PN_LISTENER_ACCEPT);
    e = pn_collector_next(l->collector);
  }
  if (e && pn_event_type(e) == PN_LISTENER_CLOSE)
    l->close_dispatched = true;
  unlock(&l->task.mutex);
  return pni_log_event(l, e);
}

static void listener_done(pn_listener_t *l) {
  pn_proactor_t *p = l->task.proactor;
  tslot_t *ts = l->task.runner;
  lock(&l->task.mutex);

  // Just in case the app didn't accept all the pending accepts
  // Shuffle the list back to start at 0
  memmove(&l->pending_accepteds[0], &l->pending_accepteds[l->pending_first], l->pending_count * sizeof(accepted_t));
  l->pending_first = 0;

  if (!l->task.closing) {
    for (size_t i = 0; i < l->acceptors_size; i++) {
      acceptor_t *a = &l->acceptors[i];
      psocket_t *ps = &a->psocket;

      // Rearm acceptor when appropriate
      if (ps->epoll_io.polling && l->pending_count==0 && !a->overflowed) {
        if (!a->armed) {
          rearm(l->task.proactor, &ps->epoll_io);
          a->armed = true;
        }
      }
    }
  }

  bool notify = false;
  l->task.working = false;

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

  if (l->task.sched_ready) {
    l->task.sched_ready = false;
    schedule_done(&l->task);
  }
  unlock(&p->sched_mutex);

  if (!n_events && listener_can_free(l)) {
    unlock(&l->task.mutex);
    pn_listener_free(l);
    lock(&p->sched_mutex);
    notify = unassign_thread(ts, UNUSED);
    unlock(&p->sched_mutex);
    if (notify)
      notify_poller(p);
    return;
  } else if (n_events || listener_has_event(l))
    notify = schedule(&l->task);
  unlock(&l->task.mutex);

  lock(&p->sched_mutex);
  if (unassign_thread(ts, UNUSED))
    notify = true;
  unlock(&p->sched_mutex);
  if (notify) notify_poller(p);
}

pn_proactor_t *pn_listener_proactor(pn_listener_t* l) {
  return l ? l->task.proactor : NULL;
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
  lock(&l->task.mutex);
  if (l->task.closing)
    err2 = EBADF;
  else {
    accepted_t *a = listener_accepted_next(l);
    if (a) {
      fd = a->accepted_fd;
      a->accepted_fd = -1;
    }
    else err2 = EWOULDBLOCK;
  }

  proactor_add(&pc->task);
  lock(&pc->task.mutex);
  if (fd >= 0) {
    configure_socket(fd);
    pconnection_start(pc, fd);
    pconnection_connected_lh(pc);
  }
  else
    psocket_error(&pc->psocket, err2, "pn_listener_accept");
  if (!l->task.working && listener_has_event(l))
    notify = schedule(&l->task);
  unlock(&pc->task.mutex);
  unlock(&l->task.mutex);
  if (notify) notify_poller(l->task.proactor);
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

  p->warm_runnables = (task_t **) realloc(p->warm_runnables, p->thread_capacity * sizeof(task_t *));
  p->resume_list = (tslot_t **) realloc(p->resume_list, p->thread_capacity * sizeof(tslot_t *));

  int old_cap = p->runnables_capacity;
  if (p->runnables_capacity == 0)
    p->runnables_capacity = 16;
  else if (p->runnables_capacity < p->thread_capacity)
    p->runnables_capacity = p->thread_capacity;
  if (p->runnables_capacity != old_cap) {
    p->runnables = (task_t **) realloc(p->runnables, p->runnables_capacity * sizeof(task_t *));
    p->kevents_capacity = p->runnables_capacity;
    size_t sz = p->kevents_capacity * sizeof(struct epoll_event);
    p->kevents = (struct epoll_event *) realloc(p->kevents, sz);
    memset(p->kevents, 0, sz);
  }
}

/* Set up an epoll_extended_t to be used for ready list schedule() or interrupts */
 static void epoll_eventfd_init(epoll_extended_t *ee, int eventfd, int epollfd, bool always_set) {
  ee->fd = eventfd;
  ee->type = EVENT_FD;
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
  task_init(&p->task, PROACTOR, p);
  pmutex_init(&p->eventfd_mutex);
  pmutex_init(&p->sched_mutex);
  pmutex_init(&p->tslot_mutex);

  if ((p->epollfd = epoll_create(1)) >= 0) {
    if ((p->eventfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
      if ((p->interruptfd = eventfd(0, EFD_NONBLOCK)) >= 0) {
        if (pni_timer_manager_init(&p->timer_manager))
          if ((p->collector = pn_collector()) != NULL) {
            p->batch.next_event = &proactor_batch_next;
            start_polling(&p->timer_manager.epoll_timer, p->epollfd);  // TODO: check for error
            epoll_eventfd_init(&p->epoll_schedule, p->eventfd, p->epollfd, true);
            epoll_eventfd_init(&p->epoll_interrupt, p->interruptfd, p->epollfd, false);
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
  pni_timer_manager_finalize(&p->timer_manager);
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
  while (p->tasks) {
    task_t *tsk = p->tasks;
    p->tasks = tsk->next;
    switch (tsk->type) {
     case PCONNECTION:
      pconnection_forced_shutdown(task_pconnection(tsk));
      break;
     case LISTENER:
      listener_forced_shutdown(task_listener(tsk));
      break;
     default:
      break;
    }
  }

  pni_timer_manager_finalize(&p->timer_manager);
  pn_collector_free(p->collector);
  pmutex_finalize(&p->tslot_mutex);
  pmutex_finalize(&p->sched_mutex);
  pmutex_finalize(&p->eventfd_mutex);
  task_finalize(&p->task);
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
  if (l) return l->task.proactor;
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
  lock(&p->task.mutex);
  proactor_update_batch(p);
  pn_event_t *e = pn_collector_next(p->collector);
  if (e) {
    p->current_event_type = pn_event_type(e);
    if (p->current_event_type == PN_PROACTOR_TIMEOUT)
      p->timeout_processed = true;
  }
  unlock(&p->task.mutex);
  return pni_log_event(p, e);
}

static pn_event_batch_t *proactor_process(pn_proactor_t *p, bool interrupt, bool tsk_ready) {
  if (interrupt) {
    (void)read_uint64(p->interruptfd);
    rearm(p, &p->epoll_interrupt);
  }
  lock(&p->task.mutex);
  if (interrupt) {
    p->need_interrupt = true;
  }
  if (tsk_ready) {
    schedule_done(&p->task);
  }
  if (!p->task.working) {       /* Can generate proactor events */
    if (proactor_update_batch(p)) {
      p->task.working = true;
      unlock(&p->task.mutex);
      return &p->batch;
    }
  }
  unlock(&p->task.mutex);
  return NULL;
}

void proactor_add(task_t *tsk) {
  pn_proactor_t *p = tsk->proactor;
  lock(&p->task.mutex);
  if (p->tasks) {
    p->tasks->prev = tsk;
    tsk->next = p->tasks;
  }
  p->tasks = tsk;
  p->task_count++;
  unlock(&p->task.mutex);
}

// call with psocket's mutex held
// return true if safe for caller to free psocket
bool proactor_remove(task_t *tsk) {
  pn_proactor_t *p = tsk->proactor;
  // Disassociate this task from scheduler
  if (!p->shutting_down) {
    lock(&p->sched_mutex);
    tsk->runner->state = DELETING;
    for (pn_handle_t entry = pn_hash_head(p->tslot_map); entry; entry = pn_hash_next(p->tslot_map, entry)) {
      tslot_t *ts = (tslot_t *) pn_hash_value(p->tslot_map, entry);
      if (ts->task == tsk)
        ts->task = NULL;
      if (ts->prev_task == tsk)
        ts->prev_task = NULL;
    }
    unlock(&p->sched_mutex);
  }

  lock(&p->task.mutex);
  bool can_free = true;
  if (tsk->disconnecting) {
    // No longer on tasks list
    --p->disconnects_pending;
    if (--tsk->disconnect_ops != 0) {
      // procator_disconnect() does the free
      can_free = false;
    }
  }
  else {
    // normal case
    if (tsk->prev)
      tsk->prev->next = tsk->next;
    else {
      p->tasks = tsk->next;
      tsk->next = NULL;
      if (p->tasks)
        p->tasks->prev = NULL;
    }
    if (tsk->next) {
      tsk->next->prev = tsk->prev;
    }
    p->task_count--;
  }
  bool notify = schedule_if_inactive(p);
  unlock(&p->task.mutex);
  if (notify) notify_poller(p);
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
static pn_event_batch_t *process(task_t *tsk) {
  bool tsk_ready = false;
  tsk->sched_pending = false;
  if (tsk->sched_ready) {
    // update the ready status before releasing the sched_mutex
    tsk->sched_ready = false;
    tsk_ready = true;
  }
  pn_proactor_t *p = tsk->proactor;
  pn_event_batch_t* batch = NULL;
  switch (tsk->type) {
  case PROACTOR: {
    bool intr = p->sched_interrupt;
    if (intr) p->sched_interrupt = false;
    unlock(&p->sched_mutex);
    batch = proactor_process(p, intr, tsk_ready);
    break;
  }
  case PCONNECTION: {
    pconnection_t *pc = task_pconnection(tsk);
    uint32_t events = pc->psocket.sched_io_events;
    if (events) pc->psocket.sched_io_events = 0;
    unlock(&p->sched_mutex);
    batch = pconnection_process(pc, events, tsk_ready, false);
    break;
  }
  case LISTENER: {
    pn_listener_t *l = task_listener(tsk);
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
    batch = listener_process(l, n_events, tsk_ready);
    break;
  }
  case RAW_CONNECTION: {
    unlock(&p->sched_mutex);
    batch = pni_raw_connection_process(tsk, tsk_ready);
    break;
  }
  case TIMER_MANAGER: {
    pni_timer_manager_t *tm = &p->timer_manager;
    bool timeout = tm->sched_timeout;
    if (timeout) tm->sched_timeout = false;
    unlock(&p->sched_mutex);
    batch = pni_timer_manager_process(tm, timeout, tsk_ready);
    break;
  }
  default:
    assert(NULL);
  }
  lock(&p->sched_mutex);
  return batch;
}


// Call with both sched_mutex and eventfd_mutex held
static void schedule_ready_list(pn_proactor_t *p) {
  // append ready_list_first..ready_list_last to end of sched_ready_last
  if (p->ready_list_first) {
    if (p->sched_ready_last)
      p->sched_ready_last->ready_next = p->ready_list_first;  // join them
    if (!p->sched_ready_first)
      p->sched_ready_first = p->ready_list_first;
    p->sched_ready_last = p->ready_list_last;
    if (!p->sched_ready_current)
      p->sched_ready_current = p->sched_ready_first;
    p->ready_list_first = p->ready_list_last = NULL;
  }
}

// Call with schedule lock held.  Called only by poller thread.
static task_t *post_event(pn_proactor_t *p, struct epoll_event *evp) {
  epoll_extended_t *ee = (epoll_extended_t *) evp->data.ptr;
  task_t *tsk = NULL;

  switch (ee->type) {
  case EVENT_FD:
    if  (ee->fd == p->interruptfd) {        /* Interrupts have their own dedicated eventfd */
      p->sched_interrupt = true;
      tsk = &p->task;
      tsk->sched_pending = true;
    } else {
      // main ready tasks eventfd
      lock(&p->eventfd_mutex);
      schedule_ready_list(p);
      tsk = p->sched_ready_current;
      unlock(&p->eventfd_mutex);
    }
    break;
  case PCONNECTION_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    pconnection_t *pc = psocket_pconnection(ps);
    assert(pc);
    tsk = &pc->task;
    ps->sched_io_events = evp->events;
    tsk->sched_pending = true;
    break;
  }
  case LISTENER_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    pn_listener_t *l = psocket_listener(ps);
    assert(l);
    tsk = &l->task;
    ps->sched_io_events = evp->events;
    tsk->sched_pending = true;
    break;
  }
  case RAW_CONNECTION_IO: {
    psocket_t *ps = containerof(ee, psocket_t, epoll_io);
    tsk = pni_psocket_raw_task(ps);
    ps->sched_io_events = evp->events;
    tsk->sched_pending = true;
    break;
  }
  case TIMER: {
    pni_timer_manager_t *tm = &p->timer_manager;
    tsk = &tm->task;
    tm->sched_timeout = true;
    tsk->sched_pending = true;
    break;
  }
  }
  if (tsk && !tsk->runnable && !tsk->runner)
    return tsk;
  return NULL;
}


static task_t *post_ready(pn_proactor_t *p, task_t *tsk) {
  tsk->sched_ready = true;
  tsk->sched_pending = true;
  if (!tsk->runnable && !tsk->runner)
    return tsk;
  return NULL;
}

// call with sched_lock held
static task_t *next_drain(pn_proactor_t *p, tslot_t *ts) {
  // This should be called seldomly, best case once per thread removal on shutdown.
  // TODO: how to reduce?  Instrumented near 5 percent of earmarks, 1 in 2000 calls to do_epoll().

  for (pn_handle_t entry = pn_hash_head(p->tslot_map); entry; entry = pn_hash_next(p->tslot_map, entry)) {
    tslot_t *ts2 = (tslot_t *) pn_hash_value(p->tslot_map, entry);
    if (ts2->earmarked) {
      // undo the old assign thread and earmark.  ts2 may never come back
      task_t *switch_tsk = ts2->task;
      remove_earmark(ts2);
      assign_thread(ts, switch_tsk);
      ts->earmark_override = ts2;
      ts->earmark_override_gen = ts2->generation;
      return switch_tsk;
    }
  }
  assert(false);
  return NULL;
}

// call with sched_lock held
static task_t *next_runnable(pn_proactor_t *p, tslot_t *ts) {
  if (ts->task) {
    // Already assigned
    if (ts->earmarked) {
      ts->earmarked = false;
      if (--p->earmark_count == 0)
        p->earmark_drain = false;
    }
    return ts->task;
  }

  // warm pairing ?
  task_t *tsk = ts->prev_task;
  if (tsk && (tsk->runnable)) { // or tsk->sched_ready too?
    assign_thread(ts, tsk);
    return tsk;
  }

  // check for an unassigned runnable task or ready list task
  if (p->n_runnables) {
    // Any unclaimed runnable?
    while (p->n_runnables) {
      tsk = p->runnables[p->next_runnable++];
      if (p->n_runnables == p->next_runnable)
        p->n_runnables = 0;
      if (tsk->runnable) {
        assign_thread(ts, tsk);
        return tsk;
      }
    }
  }

  if (p->sched_ready_current) {
    tsk = p->sched_ready_current;
    pop_ready_task(tsk);  // updates sched_ready_current
    assert(!tsk->runnable && !tsk->runner);
    assign_thread(ts, tsk);
    return tsk;
  }

  if (p->earmark_drain) {
    tsk = next_drain(p, ts);
    if (p->earmark_count == 0)
      p->earmark_drain = false;
    return tsk;
  }

  return NULL;
}

static pn_event_batch_t *next_event_batch(pn_proactor_t* p, bool can_block) {
  lock(&p->tslot_mutex);
  tslot_t * ts = find_tslot(p);
  unlock(&p->tslot_mutex);

  lock(&p->sched_mutex);
  assert(ts->task == NULL || ts->earmarked);
  assert(ts->state == UNUSED || ts->state == NEW);
  ts->state = PROCESSING;
  ts->generation++;  // wrapping OK.  Just looking for any change

  // Process outstanding epoll events until we get a batch or need to block.
  while (true) {
    // First see if there are any tasks waiting to run and perhaps generate new Proton events,
    task_t *tsk = next_runnable(p, ts);
    if (tsk) {
      ts->state = BATCHING;
      pn_event_batch_t *batch = process(tsk);
      if (batch) {
        unlock(&p->sched_mutex);
        return batch;
      }
      bool notify = unassign_thread(ts, PROCESSING);
      if (notify) {
        unlock(&p->sched_mutex);
        notify_poller(p);
        lock(&p->sched_mutex);
      }
      continue;  // Long time may have passed.  Back to beginning.
    }

    // Poll or wait for a runnable task
    if (p->poller == NULL) {
      bool return_immediately;
      p->poller = ts;
      // Get new epoll events (if any) and mark the relevant tasks as runnable
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
  task_t *tsk;

  while (true) {
    assert(p->n_runnables == 0);
    if (p->thread_count > p->thread_capacity)
      grow_poller_bufs(p);
    p->next_runnable = 0;
    p->n_warm_runnables = 0;
    p->last_earmark = NULL;

    bool unfinished_earmarks = p->earmark_count > 0;
    bool new_ready_tasks = false;
    bool epoll_immediate = unfinished_earmarks || !can_block;
    assert(!p->sched_ready_first);
    if (!epoll_immediate) {
      lock(&p->eventfd_mutex);
      if (p->ready_list_first) {
        epoll_immediate = true;
        new_ready_tasks = true;
      } else {
        p->ready_list_active = false;
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
    if (new_ready_tasks) {
      lock(&p->eventfd_mutex);
      schedule_ready_list(p);
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
    tsk = post_event(p, &p->kevents[i]);
    if (tsk)
      make_runnable(tsk);
  }
  if (n_events > 0)
    memset(p->kevents, 0, sizeof(struct epoll_event) * n_events);

  // The list of ready tasks can be very long.  Traverse part of it looking for warm pairings.
  task_t *ctsk = p->sched_ready_current;
  int max_runnables = p->runnables_capacity;
  while (ctsk && p->n_runnables < max_runnables) {
    if (ctsk->runner == RESCHEDULE_PLACEHOLDER)
      ctsk->runner = NULL;  // Allow task to run again.
    tsk = post_ready(p, ctsk);
    if (tsk)
      make_runnable(tsk);
    pop_ready_task(ctsk);
    ctsk = ctsk->ready_next;
  }
  p->sched_ready_current = ctsk;
  // More ready tasks than places on the runnables list
  while (ctsk) {
    if (ctsk->runner == RESCHEDULE_PLACEHOLDER)
      ctsk->runner = NULL;  // Allow task to run again.
    ctsk->sched_ready = true;
    ctsk->sched_pending = true;
    if (ctsk->runnable || ctsk->runner)
      pop_ready_task(ctsk);
    ctsk = ctsk->ready_next;
  }

  if (pni_immediate && !ts->task) {
    // Poller gets to run if possible
    task_t *ptsk;
    if (p->n_runnables) {
      assert(p->next_runnable == 0);
      ptsk = p->runnables[0];
      if (++p->next_runnable == p->n_runnables)
        p->n_runnables = 0;
    } else if (p->n_warm_runnables) {
      ptsk = p->warm_runnables[--p->n_warm_runnables];
      tslot_t *ts2 = ptsk->runner;
      ts2->prev_task = ts2->task = NULL;
      ptsk->runner = NULL;
    } else if (p->last_earmark) {
      ptsk = p->last_earmark->task;
      remove_earmark(p->last_earmark);
      if (p->earmark_count == 0)
        p->earmark_drain = false;
    } else {
      ptsk = NULL;
    }
    if (ptsk) {
      assign_thread(ts, ptsk);
    }
  }
  return false;
}

// Call with sched lock, but only as poller.
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
        task_t *tsk = p->warm_runnables[i];
        tslot_t *tsp = tsk->runner;
        if (tsp->state == SUSPENDED) {
          resume_list2[resume_list_count++] = tsp;
          LL_REMOVE(p, suspend_list, tsp);
          p->suspend_list_count--;
          tsp->state = PROCESSING;
        }
      }

      int can_use = p->suspend_list_count;
      if (!ts->task)
        can_use++;
      // Run as many unpaired runnable tasks as possible and allow for a new poller.
      int new_runners = pn_min(p->n_runnables + 1, can_use);
      if (!ts->task)
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
    tslot_t *ts = pc->task.runner;
    pconnection_done(pc);
    // pc possibly freed/invalid
    check_earmark_override(p, ts);
    return;
  }
  pn_listener_t *l = batch_listener(batch);
  if (l) {
    tslot_t *ts = l->task.runner;
    listener_done(l);
    // l possibly freed/invalid
    check_earmark_override(p, ts);
    return;
  }
  praw_connection_t *rc = pni_batch_raw_connection(batch);
  if (rc) {
    tslot_t *ts = pni_raw_connection_task(rc)->runner;
    pni_raw_connection_done(rc);
    // rc possibly freed/invalid
    check_earmark_override(p, ts);
    return;
  }
  pn_proactor_t *bp = batch_proactor(batch);
  if (bp == p) {
    bool notify = false;
    lock(&p->task.mutex);
    p->task.working = false;
    if (p->timeout_processed) {
      p->timeout_processed = false;
      if (schedule_if_inactive(p))
        notify = true;
    }
    proactor_update_batch(p);
    if (proactor_has_event(p))
      if (schedule(&p->task))
        notify = true;

    unlock(&p->task.mutex);
    lock(&p->sched_mutex);
    tslot_t *ts = p->task.runner;
    if (unassign_thread(ts, UNUSED))
      notify = true;
    unlock(&p->sched_mutex);

    if (notify)
      notify_poller(p);
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
  lock(&p->task.mutex);
  p->timeout_set = true;
  if (t == 0) {
    pni_timer_set(p->timer, 0);
    p->need_timeout = true;
    notify = schedule(&p->task);
  } else {
    pni_timer_set(p->timer, t + pn_proactor_now_64());
  }
  unlock(&p->task.mutex);
  if (notify) notify_poller(p);
}

void pn_proactor_cancel_timeout(pn_proactor_t *p) {
  lock(&p->task.mutex);
  p->timeout_set = false;
  p->need_timeout = false;
  pni_timer_set(p->timer, 0);
  bool notify = schedule_if_inactive(p);
  unlock(&p->task.mutex);
  if (notify) notify_poller(p);
}

void pni_proactor_timeout(pn_proactor_t *p) {
  bool notify = false;
  lock(&p->task.mutex);
  if (!p->task.closing) {
    p->need_timeout = true;
    notify = schedule(&p->task);
  }
  unlock(&p->task.mutex);
  if (notify) notify_poller(p);
}

pn_proactor_t *pn_connection_proactor(pn_connection_t* c) {
  pconnection_t *pc = get_pconnection(c);
  return pc ? pc->task.proactor : NULL;
}

void pn_proactor_disconnect(pn_proactor_t *p, pn_condition_t *cond) {
  bool notify = false;

  lock(&p->task.mutex);
  // Move the whole tasks list into a disconnecting state
  task_t *disconnecting_tasks = p->tasks;
  p->tasks = NULL;
  // First pass: mark each task as disconnecting and update global pending count.
  task_t *tsk = disconnecting_tasks;
  while (tsk) {
    tsk->disconnecting = true;
    tsk->disconnect_ops = 2;   // Second pass below and proactor_remove(), in any order.
    p->disconnects_pending++;
    tsk = tsk->next;
    p->task_count--;
  }
  notify = schedule_if_inactive(p);
  unlock(&p->task.mutex);
  if (!disconnecting_tasks) {
    if (notify) notify_poller(p);
    return;
  }

  // Second pass: different locking, close the tasks, free them if !disconnect_ops
  task_t *next = disconnecting_tasks;
  while (next) {
    tsk = next;
    next = tsk->next;           /* Save next pointer in case we free tsk */
    bool do_free = false;
    bool tsk_notify = false;
    pmutex *tsk_mutex = NULL;
    // TODO: Need to extend this for raw connections too
    pconnection_t *pc = task_pconnection(tsk);
    if (pc) {
      tsk_mutex = &pc->task.mutex;
      lock(tsk_mutex);
      if (!tsk->closing) {
        tsk_notify = true;
        if (tsk->working) {
          // Must defer
          pc->queued_disconnect = true;
          if (cond) {
            if (!pc->disconnect_condition)
              pc->disconnect_condition = pn_condition();
            pn_condition_copy(pc->disconnect_condition, cond);
          }
        }
        else {
          // No conflicting working task.
          if (cond) {
            pn_condition_copy(pn_transport_condition(pc->driver.transport), cond);
          }
          pn_connection_driver_close(&pc->driver);
        }
      }
    } else {
      pn_listener_t *l = task_listener(tsk);
      assert(l);
      tsk_mutex = &l->task.mutex;
      lock(tsk_mutex);
      if (!tsk->closing) {
        tsk_notify = true;
        if (cond) {
          pn_condition_copy(pn_listener_condition(l), cond);
        }
        listener_begin_close(l);
      }
    }

    lock(&p->task.mutex);
    if (--tsk->disconnect_ops == 0) {
      do_free = true;
      tsk_notify = false;
      notify = schedule_if_inactive(p);
    } else {
      // If initiating the close, schedule the task to do the free.
      if (tsk_notify)
        tsk_notify = schedule(tsk);
      if (tsk_notify)
        notify_poller(p);
    }
    unlock(&p->task.mutex);
    unlock(tsk_mutex);

    // Unsafe to touch tsk after lock release, except if we are the designated final_free
    if (do_free) {
      if (pc) pconnection_final_free(pc);
      else listener_final_free(task_listener(tsk));
    }
  }
  if (notify)
    notify_poller(p);
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
