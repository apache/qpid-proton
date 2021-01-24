#ifndef PROACTOR_EPOLL_INTERNAL_H
#define PROACTOR_EPOLL_INTERNAL_H

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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>


#include <proton/connection_driver.h>
#include <proton/proactor.h>

#include "netaddr-internal.h"
#include "proactor-internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct acceptor_t acceptor_t;
typedef struct tslot_t tslot_t;
typedef pthread_mutex_t pmutex;
typedef struct pni_timer_t pni_timer_t;

typedef enum {
  EVENT_FD,   /* schedule() or pn_proactor_interrupt() */
  LISTENER_IO,
  PCONNECTION_IO,
  RAW_CONNECTION_IO,
  TIMER
} epoll_type_t;

// Data to use with epoll.
typedef struct epoll_extended_t {
  int fd;
  epoll_type_t type;   // io/timer/eventfd
  uint32_t wanted;     // events to poll for
  bool polling;
  pmutex barrier_mutex;
} epoll_extended_t;

typedef enum {
  PROACTOR,
  PCONNECTION,
  LISTENER,
  RAW_CONNECTION,
  TIMER_MANAGER
} task_type_t;

typedef struct task_t {
  pmutex mutex;
  pn_proactor_t *proactor;  /* Immutable */
  task_type_t type;
  bool working;
  bool ready;                // ready to run and on ready list.  Poller notified by eventfd.
  bool waking;
  bool on_ready_list;        // todo: protected by eventfd_mutex or sched mutex?  needed?
  struct task_t *ready_next; // ready list, guarded by proactor eventfd_mutex
  bool closing;
  // Next 4 are protected by the proactor mutex
  struct task_t* next;  /* Protected by proactor.mutex */
  struct task_t* prev;  /* Protected by proactor.mutex */
  int disconnect_ops;           /* ops remaining before disconnect complete */
  bool disconnecting;           /* pn_proactor_disconnect */
  // Protected by schedule mutex
  tslot_t *runner __attribute__((aligned(64)));  /* designated or running thread */
  tslot_t *prev_runner;
  bool sched_ready;
  bool sched_pending;           /* If true, one or more unseen epoll or other events to process() */
  bool runnable ;               /* on one of the runnable lists */
} task_t;

typedef enum {
  NEW,
  UNUSED,                       /* pn_proactor_done() called, may never come back */
  SUSPENDED,
  PROCESSING,                   /* Hunting for a task  */
  BATCHING,                     /* Doing work on behalf of a task */
  DELETING,
  POLLING
} tslot_state;

// Epoll proactor's concept of a worker thread provided by the application.
struct tslot_t {
  pmutex mutex;  // suspend and resume
  pthread_cond_t cond;
  unsigned int generation;
  bool suspended;
  volatile bool scheduled;
  tslot_state state;
  task_t *task;
  task_t *prev_task;
  bool earmarked;
  tslot_t *suspend_list_prev;
  tslot_t *suspend_list_next;
  tslot_t *earmark_override;   // on earmark_drain, which thread was unassigned
  unsigned int earmark_override_gen;
};

typedef struct pni_timer_manager_t {
  task_t task;
  epoll_extended_t epoll_timer;
  pmutex deletion_mutex;
  pni_timer_t *proactor_timer;
  pn_list_t *timers_heap;
  uint64_t timerfd_deadline;
  bool sched_timeout;
} pni_timer_manager_t;

struct pn_proactor_t {
  task_t task;
  pni_timer_manager_t timer_manager;
  epoll_extended_t epoll_schedule;     /* ready list */
  epoll_extended_t epoll_interrupt;
  pn_event_batch_t batch;
  task_t *tasks;         /* track in-use tasks for PN_PROACTOR_INACTIVE and disconnect */
  pni_timer_t *timer;
  size_t disconnects_pending;   /* unfinished proactor disconnects*/
  // need_xxx flags indicate we should generate PN_PROACTOR_XXX on the next update_batch()
  bool need_interrupt;
  bool need_inactive;
  bool need_timeout;
  bool timeout_set; /* timeout has been set by user and not yet cancelled or generated event */
  bool timeout_processed;  /* timeout event dispatched in the most recent event batch */
  int task_count;

  // ready list subsystem
  int eventfd;
  pmutex eventfd_mutex;
  bool ready_list_active;
  task_t *ready_list_first;
  task_t *ready_list_last;
  // Interrupts have a dedicated eventfd because they must be async-signal safe.
  int interruptfd;
  // If the process runs out of file descriptors, disarm listening sockets temporarily and save them here.
  acceptor_t *overflow;
  pmutex overflow_mutex;

  // Sched vars specific to proactor task.
  bool sched_interrupt;

  // Global scheduling/poller vars.
  // Warm runnables have assigned or earmarked tslots and can run right away.
  // Other runnables are run as tslots come available.
  pmutex sched_mutex;
  int n_runnables;
  int next_runnable;
  int n_warm_runnables;
  tslot_t *suspend_list_head;
  tslot_t *suspend_list_tail;
  int suspend_list_count;
  tslot_t *poller;
  bool poller_suspended;
  tslot_t *last_earmark;
  task_t *sched_ready_first;
  task_t *sched_ready_last;
  task_t *sched_ready_current;
  pmutex tslot_mutex;
  int earmark_count;
  bool earmark_drain;
  // For debugging help for core dumps with optimized code.
  pn_event_type_t current_event_type;

  // Mostly read only: after init or once thread_count stabilizes
  pn_collector_t *collector  __attribute__((aligned(64)));
  task_t **warm_runnables;
  task_t **runnables;
  tslot_t **resume_list;
  pn_hash_t *tslot_map;
  struct epoll_event *kevents;
  int epollfd;
  int thread_count;
  int thread_capacity;
  int runnables_capacity;
  int kevents_capacity;
  bool shutting_down;
};

/* common to connection and listener */
typedef struct psocket_t {
  // Protected by the pconnection/listener mutex
  epoll_extended_t epoll_io;
  uint32_t sched_io_events;
  uint32_t working_io_events;
} psocket_t;

typedef struct pconnection_t {
  task_t task;
  psocket_t psocket;
  pni_timer_t *timer;
  const char *host, *port;
  uint32_t new_events;
  bool server;                /* accept, not connect */
  bool tick_pending;
  bool queued_disconnect;     /* deferred from pn_proactor_disconnect() */
  pn_condition_t *disconnect_condition;
  // Following values only changed by (sole) working task:
  uint32_t current_arm;  // active epoll io events
  bool connected;
  bool read_blocked;
  bool write_blocked;
  bool disconnected;
  int hog_count; // thread hogging limiter
  pn_event_batch_t batch;
  pn_connection_driver_t driver;
  bool output_drained;
  const char *wbuf_current;
  size_t wbuf_remaining;
  size_t wbuf_completed;
  pn_event_type_t current_event_type;/* Sole use for debugging, i.e. crash analysis of optimized code. */
  uint32_t process_args;             /* Sole use for debugging */
  uint32_t process_events;           /* Sole use for debugging */
  struct pn_netaddr_t local, remote; /* Actual addresses */
  struct addrinfo *addrinfo;         /* Resolved address list */
  struct addrinfo *ai;               /* Current connect address */
  pmutex rearm_mutex;                /* protects pconnection_rearm from out of order arming*/
  bool io_doublecheck;               /* callbacks made and new IO may have arrived */
  uint64_t expected_timeout;
  char addr_buf[1];
} pconnection_t;

/*
 * A listener can have multiple sockets (as specified in the addrinfo).  They
 * are armed separately.  The individual psockets can be part of at most one
 * list: the global proactor overflow retry list or the per-listener list of
 * pending accepts (valid inbound socket obtained, but pn_listener_accept not
 * yet called by the application).  These lists will be small and quick to
 * traverse.
 */

struct acceptor_t{
  psocket_t psocket;
  struct pn_netaddr_t addr;      /* listening address */
  pn_listener_t *listener;
  acceptor_t *next;              /* next listener list member */
  bool armed;
  bool overflowed;
};

typedef struct accepted_t{
  int accepted_fd;
} accepted_t;

struct pn_listener_t {
  task_t task;
  acceptor_t *acceptors;          /* Array of listening sockets */
  size_t acceptors_size;
  char addr_buf[PN_MAX_ADDR];
  const char *host, *port;
  int active_count;               /* Number of listener sockets registered with epoll */
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_event_batch_t batch;
  pn_record_t *attachments;
  void *listener_context;
  accepted_t *pending_accepteds;  /* array of accepted connections */
  size_t pending_first;              /* index of first pending connection */
  size_t pending_count;              /* number of pending accepted connections */
  size_t backlog;                 /* size of pending accepted array */
  bool close_dispatched;
  uint32_t sched_io_events;
};

typedef char strerrorbuf[1024];      /* used for pstrerror message buffer */
void pstrerror(int err, strerrorbuf msg);

/* Internal error, no recovery */
#define EPOLL_FATAL(EXPR, SYSERRNO)                                     \
  do {                                                                  \
    strerrorbuf msg;                                                    \
    pstrerror((SYSERRNO), msg);                                         \
    fprintf(stderr, "epoll proactor failure in %s:%d: %s: %s\n",        \
            __FILE__, __LINE__ , #EXPR, msg);                           \
    abort();                                                            \
  } while (0)

// In general all locks to be held singly and shortly (possibly as spin locks).
// See above about lock ordering.

static inline void pmutex_init(pthread_mutex_t *pm){
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
  if (pthread_mutex_init(pm, &attr)) {
    perror("pthread failure");
    abort();
  }
}

static inline void pmutex_finalize(pthread_mutex_t *m) { pthread_mutex_destroy(m); }
static inline void lock(pmutex *m) { pthread_mutex_lock(m); }
static inline void unlock(pmutex *m) { pthread_mutex_unlock(m); }

static inline bool pconnection_has_event(pconnection_t *pc) {
  return pn_connection_driver_has_event(&pc->driver);
}

static inline bool listener_has_event(pn_listener_t *l) {
  return pn_collector_peek(l->collector) || (l->pending_count);
}

static inline bool proactor_has_event(pn_proactor_t *p) {
  return pn_collector_peek(p->collector);
}

bool schedule_if_inactive(pn_proactor_t *p);
int pclosefd(pn_proactor_t *p, int fd);

void proactor_add(task_t *tsk);
bool proactor_remove(task_t *tsk);

bool unassign_thread(tslot_t *ts, tslot_state new_state);

void task_init(task_t *tsk, task_type_t t, pn_proactor_t *p);
static void task_finalize(task_t* tsk) {
  pmutex_finalize(&tsk->mutex);
}

bool schedule(task_t *tsk);
void notify_poller(pn_proactor_t *p);
void schedule_done(task_t *tsk);

void psocket_init(psocket_t* ps, epoll_type_t type);
bool start_polling(epoll_extended_t *ee, int epollfd);
void stop_polling(epoll_extended_t *ee, int epollfd);
void rearm_polling(epoll_extended_t *ee, int epollfd);

int pgetaddrinfo(const char *host, const char *port, int flags, struct addrinfo **res);
void configure_socket(int sock);

accepted_t *listener_accepted_next(pn_listener_t *listener);

task_t *pni_psocket_raw_task(psocket_t *ps);
pn_event_batch_t *pni_raw_connection_process(task_t *t, bool sched_ready);

typedef struct praw_connection_t praw_connection_t;
task_t *pni_raw_connection_task(praw_connection_t *rc);
praw_connection_t *pni_batch_raw_connection(pn_event_batch_t* batch);
void pni_raw_connection_done(praw_connection_t *rc);

pni_timer_t *pni_timer(pni_timer_manager_t *tm, pconnection_t *c);
void pni_timer_free(pni_timer_t *timer);
void pni_timer_set(pni_timer_t *timer, uint64_t deadline);
bool pni_timer_manager_init(pni_timer_manager_t *tm);
void pni_timer_manager_finalize(pni_timer_manager_t *tm);
pn_event_batch_t *pni_timer_manager_process(pni_timer_manager_t *tm, bool timeout, bool sched_ready);
void pni_pconnection_timeout(pconnection_t *pc);
void pni_proactor_timeout(pn_proactor_t *p);

// Generic wake primitives for a task.

// Call with task lock held.  Must call notify_poller() if returns true.
static inline bool pni_task_wake(task_t *tsk) {
  if (!tsk->waking) {
    tsk->waking = true;
    return schedule(tsk);
  }
  return false;
}

// Call with task lock held.
static inline bool pni_task_wake_pending(task_t *tsk) {
  return tsk->waking;
}

// Call with task lock held and only from the running task.
static inline void pni_task_wake_done(task_t *tsk) {
  tsk->waking = false;
}


#ifdef __cplusplus
}
#endif
#endif // PROACTOR_EPOLL_INTERNAL_H
