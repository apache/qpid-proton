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

#include "epoll-internal.h"
#include "core/util.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

/*
 * Epoll proactor subsystem for timers.
 *
 * Two types of timers: (1) connection timers, one per connection, active if at least one of the peers has set a heartbeat,
 * timers move forward in time (but see replace_timer_deadline() note below), latency not critical; (2) a single proactor
 * timer, can move forwards or backwards, can be canceled.
 *
 * A single timerfd is shared by all the timers.  Connection timers are tracked on a heap ordered list.  The proactor timer is
 * tracked separately.  The next timerfd_deadline is the earliest of all the timers, in this case the earliest of the first
 * connection timer and the poactor timer.
 *
 * If a connection timer is changed to a later time, it is not moved.  It is kept in place but marked with the new deadline.  On
 * expiry, depending on whether the deadline was extended, the decision is made to either generate a timeout or replace the
 * timer on the ordered list.
 *
 * When a timerfd read event is generated, the proactor invokes pni_timer_manager_process() to generate timeouts for each
 * expired timer and to do housekeeping on the rest.
 *
 * replace_timer_deadline(): a connection timer can go backwards in time at most once if: both peers have heartbeats and the
 * second AMQP open frame results in a shorter periodic transport timer than the first open frame.  In this case, the
 * existing timer_deadline is immediately orphaned and a new one created for the rest of the connection's life.
 *
 * Lock ordering: tm->task_mutex --> tm->deletion_mutex.
 */

static void timerfd_set(int fd, uint64_t t_millis) {
  // t_millis == 0 -> cancel
  struct itimerspec newt;
  memset(&newt, 0, sizeof(newt));
  newt.it_value.tv_sec = t_millis / 1000;
  newt.it_value.tv_nsec = (t_millis % 1000) * 1000000;
  timerfd_settime(fd, 0, &newt, NULL);
}

static void timerfd_drain(int fd) {
  // Forget any old expired timers and only trigger an epoll read event for a subsequent expiry.
  uint64_t result = 0;
  ssize_t n = read(fd, &result, sizeof(result));
  if (n != sizeof(result) && !(n < 0 && errno == EAGAIN)) {
    EPOLL_FATAL("timerfd read error", errno);
  }
}

// Struct to manage the ordering of timers on the heap ordered list and manage the lifecycle if
// the parent timer is self-deleting.
typedef struct timer_deadline_t {
  uint64_t list_deadline;      // Heap ordering deadline.  Must not change while on list.
  pni_timer_t *timer;          // Parent timer.  NULL means orphaned and to be deleted.
  bool resequenced;            // An out-of-order connection timeout caught and handled.
} timer_deadline_t;

static void timer_deadline_initialize(void *object) {
  timer_deadline_t *td = (timer_deadline_t *) object;
  memset(td, 0 , sizeof(*td));
}

static void timer_deadline_finalize(void *object) {
  assert(((timer_deadline_t *) object)->list_deadline == 0);
}

static intptr_t timer_deadline_compare(void *oa, void *ob) {
  timer_deadline_t *a = (timer_deadline_t *) oa;
  timer_deadline_t *b = (timer_deadline_t *) ob;
  return a->list_deadline - b->list_deadline;
}

#define timer_deadline_inspect NULL
#define timer_deadline_hashcode NULL
#define CID_timer_deadline CID_pn_void

static timer_deadline_t* pni_timer_deadline(void) {
  static const pn_class_t timer_deadline_clazz = PN_CLASS(timer_deadline);
  return (timer_deadline_t *) pn_class_new(&timer_deadline_clazz, sizeof(timer_deadline_t));
}


struct pni_timer_t {
  uint64_t deadline;
  timer_deadline_t *timer_deadline;
  pni_timer_manager_t *manager;
  pconnection_t *connection;
};

pni_timer_t *pni_timer(pni_timer_manager_t *tm, pconnection_t *c) {
  timer_deadline_t *td = NULL;
  pni_timer_t *timer = NULL;
  assert(c || !tm->task.proactor->timer);  // Proactor timer.  Can only be one.
  timer = (pni_timer_t *) malloc(sizeof(pni_timer_t));
  if (!timer) return NULL;
  if (c) {
    // Connections are tracked on the timer_heap.  Allocate the tracking struct.
    td = pni_timer_deadline();
    if (!td) {
      free(timer);
      return NULL;
    }
  }

  lock(&tm->task.mutex);
  timer->connection = c;
  timer->manager = tm;
  timer->timer_deadline = td;
  timer->deadline = 0;
  if (c)
    td->timer = timer;
  unlock(&tm->task.mutex);
  return timer;
}

// Call with no locks.
void pni_timer_free(pni_timer_t *timer) {
  timer_deadline_t *td = timer->timer_deadline;
  bool can_free_td = false;
  if (td) pni_timer_set(timer, 0);
  pni_timer_manager_t *tm = timer->manager;
  lock(&tm->task.mutex);
  lock(&tm->deletion_mutex);
  if (td) {
    if (td->list_deadline)
      td->timer = NULL;  // Orphan.  timer_manager does eventual pn_free() in process().
    else
      can_free_td = true;
  }
  unlock(&tm->deletion_mutex);
  unlock(&tm->task.mutex);
  if (can_free_td) {
    pn_free(td);
  }
  free(timer);
}

static timer_deadline_t *replace_timer_deadline(pni_timer_manager_t *tm, pni_timer_t *timer);

// Return true if initialization succeeds.  Called once at proactor creation.
bool pni_timer_manager_init(pni_timer_manager_t *tm) {
  tm->epoll_timer.fd = -1;
  tm->timerfd_deadline = 0;
  tm->timers_heap = NULL;
  tm->proactor_timer = NULL;
  pn_proactor_t *p = containerof(tm, pn_proactor_t, timer_manager);
  task_init(&tm->task, TIMER_MANAGER, p);
  pmutex_init(&tm->deletion_mutex);

  // PN_VOID turns off ref counting for the elements in the list.
  tm->timers_heap = pn_list(PN_VOID, 0);
  if (!tm->timers_heap)
    return false;
  tm->proactor_timer = pni_timer(tm, NULL);
  if (!tm->proactor_timer)
    return false;

  p->timer = tm->proactor_timer;
  epoll_extended_t *ee = &tm->epoll_timer;
  ee->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  ee->type = TIMER;
  ee->wanted = EPOLLIN;
  ee->polling = false;
  return (ee->fd >= 0);
}

// Only call from proactor's destructor, when it is single threaded and scheduling has stopped.
void pni_timer_manager_finalize(pni_timer_manager_t *tm) {
  lock(&tm->task.mutex);
  unlock(&tm->task.mutex);  // Memory barrier
  if (tm->epoll_timer.fd >= 0) close(tm->epoll_timer.fd);
  pni_timer_free(tm->proactor_timer);
  if (tm->timers_heap) {
    size_t sz = pn_list_size(tm->timers_heap);
    // On teardown there is no need to preserve the heap.  Traverse the list ignoring minpop().
    for (size_t idx = 0; idx < sz; idx++) {
      timer_deadline_t *td = (timer_deadline_t *) pn_list_get(tm->timers_heap, idx);
      td->list_deadline = 0;
      pn_free(td);
    }
    pn_free(tm->timers_heap);
  }
  pmutex_finalize(&tm->deletion_mutex);
  task_finalize(&tm->task);
}

// Call with timer_manager lock held.  Return true if notify_poller required.
static bool adjust_deadline(pni_timer_manager_t *tm) {
  // Make sure the timer_manager task will get a timeout in time for the earliest connection timeout.
  if (tm->task.working)
    return false;  // timer_manager task will adjust the timer when it stops working
  bool notify = false;
  uint64_t new_deadline = tm->proactor_timer->deadline;
  if (pn_list_size(tm->timers_heap)) {
    // First element of timers_heap has earliest deadline on the heap.
    timer_deadline_t *heap0 = (timer_deadline_t *) pn_list_get(tm->timers_heap, 0);
    assert(heap0->list_deadline != 0);
    new_deadline = new_deadline ? pn_min(new_deadline, heap0->list_deadline) : heap0->list_deadline;
  }
  // Only change target deadline if new_deadline is in future but earlier than old timerfd_deadline.
  if (new_deadline) {
    if (tm->timerfd_deadline == 0 || new_deadline < tm->timerfd_deadline) {
      uint64_t now = pn_proactor_now_64();
      if (new_deadline <= now) {
        // no need for a timer update.  Wake the timer_manager.
        notify = schedule(&tm->task);
      }
      else {
        timerfd_set(tm->epoll_timer.fd, new_deadline - now);
        tm->timerfd_deadline = new_deadline;
      }
    }
  }
  return notify;
}

// Call without task lock or timer_manager lock.
// Calls for connection timers are generated in the proactor and serialized per connection.
// Calls for the proactor timer can come from arbitrary user threads.
void pni_timer_set(pni_timer_t *timer, uint64_t deadline) {
  pni_timer_manager_t *tm = timer->manager;
  bool notify = false;

  lock(&tm->task.mutex);
  if (deadline == timer->deadline) {
    unlock(&tm->task.mutex);
    return;  // No change.
  }

  if (timer == tm->proactor_timer) {
    assert(!timer->connection);
    timer->deadline = deadline;
  } else {
    // Connection
    timer_deadline_t *td = timer->timer_deadline;
    // A connection timer can go backwards at most once.  Check here.
    if (deadline && td->list_deadline && deadline < td->list_deadline) {
      if (td->resequenced)
        EPOLL_FATAL("idle timeout sequencing error", 0);  //
      else {
        // replace drops the lock for malloc.  Safe because there can be no competing call to
        // the timer set function by the same pconnection from another thread.
        td = replace_timer_deadline(tm, timer);
      }
    }

    timer->deadline = deadline;
    // Put on list if not already there.
    if (deadline && !td->list_deadline) {
      td->list_deadline = deadline;
      pn_list_minpush(tm->timers_heap, td);
    }
  }

  // Skip a cancelled timer (deadline == 0) since it doesn't change the timerfd deadline.
  if (deadline)
    notify = adjust_deadline(tm);
  unlock(&tm->task.mutex);

  if (notify)
    notify_poller(tm->task.proactor);
}

pn_event_batch_t *pni_timer_manager_process(pni_timer_manager_t *tm, bool timeout, bool sched_ready) {
  uint64_t now = pn_proactor_now_64();
  lock(&tm->task.mutex);
  tm->task.working = true;
  if (timeout)
    tm->timerfd_deadline = 0;
  if (sched_ready)
    schedule_done(&tm->task);

  // First check for proactor timer expiry.
  uint64_t deadline = tm->proactor_timer->deadline;
  if (deadline && deadline <= now) {
    tm->proactor_timer->deadline = 0;
    unlock(&tm->task.mutex);
    pni_proactor_timeout(tm->task.proactor);
    lock(&tm->task.mutex);
    // If lower latency desired for the proactor timer, we could convert to the proactor task (if not working) and return
    // here with the event batch, and schedule the timer manager task to process the connection timers.
  }

  // Next, find all expired connection timers at front of the ordered heap.
  while (pn_list_size(tm->timers_heap)) {
    timer_deadline_t *td = (timer_deadline_t *) pn_list_get(tm->timers_heap, 0);
    if (td->list_deadline > now)
      break;

    // Expired. Remove from list.
    timer_deadline_t *min = (timer_deadline_t *) pn_list_minpop(tm->timers_heap);
    assert (min == td);
    min->list_deadline = 0;

    // Three possibilities to act on:
    //   timer expired -> pni_connection_timeout()
    //   timer deadline extended -> minpush back on list to new spot
    //   timer freed -> free the associated timer_deadline popped off the list
    if (!td->timer) {
      unlock(&tm->task.mutex);
      pn_free(td);
      lock(&tm->task.mutex);
    } else {
      uint64_t deadline = td->timer->deadline;
      if (deadline) {
        if (deadline <= now) {
          td->timer->deadline = 0;
          pconnection_t *pc = td->timer->connection;
          lock(&tm->deletion_mutex);     // Prevent connection from deleting itself when tm->task.mutex dropped.
          unlock(&tm->task.mutex);
          pni_pconnection_timeout(pc);
          unlock(&tm->deletion_mutex);
          lock(&tm->task.mutex);
        } else {
          td->list_deadline = deadline;
          pn_list_minpush(tm->timers_heap, td);
        }
      }
    }
  }

  if (timeout) {
    // TODO: query whether perf gain by doing these system calls outside the lock, perhaps with additional set_reset_mutex.
    timerfd_drain(tm->epoll_timer.fd);
    rearm_polling(&tm->epoll_timer, tm->task.proactor->epollfd);
  }
  tm->task.working = false;  // must be false for adjust_deadline to do adjustment
  bool notify = adjust_deadline(tm);
  unlock(&tm->task.mutex);

  if (notify)
    notify_poller(tm->task.proactor);
  // The timer_manager never has events to batch.
  return NULL;
  // TODO: perhaps become task of one of the timed out timers (if otherwise idle) and process() that task.
}

// Call with timer_manager lock held.
// There can be no competing call to this and timer_set() from the same connection.
static timer_deadline_t *replace_timer_deadline(pni_timer_manager_t *tm, pni_timer_t *timer) {
  assert(timer->connection);
  timer_deadline_t *old_td = timer->timer_deadline;
  assert(old_td);
  // Mark old struct for deletion.  No parent timer.
  old_td->timer = NULL;

  unlock(&tm->task.mutex);
  // Create replacement timer for life of connection.
  timer_deadline_t *new_td = pni_timer_deadline();
  if (!new_td)
    EPOLL_FATAL("replacement timer deadline allocation", errno);
  lock(&tm->task.mutex);

  new_td->list_deadline = 0;
  new_td->timer = timer;
  new_td->resequenced = true;
  timer->timer_deadline = new_td;
  return new_td;
}
