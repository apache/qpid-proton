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
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*
  A single set of timers in the epoll proactor for connection heartbeats.
  A single internal ptimer (and single timerfd) services all of these timers per proactor
  instance.

  The ctimerq is a separate pcontext_t that wakes all connnections with expired heartbeat
  timers.  TODO: avoid one context switch - convert to the context of the first expired
  connection (if not already scheduled).

  pn_tick_timer_t is a Proton class for use in an ordered pn_list.
  pn_tick_timer_compare() is used by the pn_list to create a heap ordering in ascending
  tick deadlines.

  In general, the tick_timer for a connection always moves forward.  It can move backwards
  at most once if the second open frame results in a shorter interval than set for the
  first open frame.  See pn_tick_timer_t.resequenced.

  The existing pn_list_t heap implementation requires that only minpush() and minpop() be
  used to add/remove from the list.  Consequently a tick_timer can not be moved on the
  list.  The movement must be noted and the move deferred until the tick timer makes its
  way to the front of the list.
*/

struct pn_tick_timer_t {
  pconnection_t *connection;
  uint64_t tick_deadline;
  uint64_t list_deadline;
  bool timeout_pending;
  bool resequenced;            // an out-of-order timeout caught and handled
};

void pn_tick_timer_initialize(pn_tick_timer_t *tt) {
  memset(tt, 0 , sizeof(*tt));
}

void pn_tick_timer_finalize(pn_tick_timer_t *tt) {
  assert(tt->list_deadline == 0);
}

intptr_t pn_tick_timer_compare(pn_tick_timer_t *a, pn_tick_timer_t *b) {
  return a->tick_deadline - b->tick_deadline;
}

#define pn_tick_timer_inspect NULL
#define pn_tick_timer_hashcode NULL
PN_CLASSDEF(pn_tick_timer)


// Return true if initialization succeeds.  Called once at proactor creation.
bool ctimerq_init(pn_proactor_t *p) {
  connection_timerq_t *ctq = &p->ctimerq;
  // PN_VOID turns off ref counting for the elements in the list.
  ctq->pending_ticks = pn_list(PN_VOID, 0);
  if (!ctq)
    return false;
  
  pcontext_init(&ctq->context, CONN_TIMERQ, p);
  return ptimer_init(&ctq->timer, CTIMERQ_TIMER);
}

// Only call from proactor's destructor, when it is single threaded and scheduling has stopped.  
void ctimerq_finalize(pn_proactor_t *p) {
  connection_timerq_t *ctq = &p->ctimerq;
  lock(&ctq->context.mutex);
  unlock(&ctq->context.mutex);  // Memory barrier
  if (!ctq->pending_ticks)
    return;
  int fd = ctq->timer.epoll_io.fd;
  stop_polling(&ctq->timer.epoll_io, ctq->context.proactor->epollfd);
  if (fd != -1)
    close(fd);
  ptimer_finalize(&ctq->timer);
  size_t sz = pn_list_size(ctq->pending_ticks);
  // On teardown there is no need to preserve the heap.  Traverse the list ignoring minpop().
  for (size_t idx = 0; idx < sz; idx++) {
    pn_tick_timer_t *tt = (pn_tick_timer_t *) pn_list_get(ctq->pending_ticks, idx);
    tt->list_deadline = 0;
    pn_free(tt);
  }
  pn_free(ctq->pending_ticks);
}

// Return true if successful.  Called once at pconnection creation.
// No locks.  pconnection_t lock not even initialized yet.
bool ctimerq_register(connection_timerq_t *ctq, pconnection_t *pc) {
  pn_tick_timer_t *tt = pn_tick_timer_new();
  if (tt) {
    lock(&ctq->context.mutex);
    tt->connection = pc;
    pc->timer = tt;
    unlock(&ctq->context.mutex);
    return true;
  }
  return false;
}

// Call with no locks.  Called once at pconnection destruction.
void ctimerq_deregister(connection_timerq_t *ctq, pconnection_t *pc) {
  bool must_free = true;
  lock(&ctq->context.mutex);
  if (!pc->timer) {
    unlock(&ctq->context.mutex);
    return;
  }
  pn_tick_timer_t *tt = pc->timer;
  pc->timer = NULL;
  tt->connection = NULL;

  tt->tick_deadline = 0;
  if (tt->list_deadline) {
    must_free = false;  // ctimerq context does the free when the list_deadline expires
  }

  unlock(&ctq->context.mutex);
  if (must_free)
    pn_free(tt);
}

// Call with ctimerq lock held.  Return true if wake_notify required.
static bool ctimerq_adjust_deadline(connection_timerq_t *ctq, uint64_t now) {
  // Make sure the ctimerq context will get a timeout in time for the earliest connection timeout.
  if (ctq->context.working)
    return false;  // ctimerq context will adjust the timer when it stops working
  bool notify = false;
  if (pn_list_size(ctq->pending_ticks)) {
    pn_tick_timer_t *next_tick_timer = (pn_tick_timer_t *) pn_list_get(ctq->pending_ticks, 0);
    uint64_t next_deadline = next_tick_timer->list_deadline;
    if (ctq->ctq_deadline == 0 || ctq->ctq_deadline > next_deadline) {
      if (next_deadline <= now) {
        notify = wake(&ctq->context);
      }
      else {
        ptimer_set(&ctq->timer, next_deadline - now);
        ctq->ctq_deadline = next_deadline;
      }
    }
  }
  return notify;
}

// call without pconnection lock or ctimerq lock
void ctimerq_schedule_tick(connection_timerq_t *ctq, pconnection_t *pc, uint64_t deadline, uint64_t now) {
  assert(!pc->context.closing);
  bool notify = false;

  lock(&ctq->context.mutex);
  pn_tick_timer_t *tt = pc->timer;
  assert(tt != NULL);

  if (deadline == tt->tick_deadline) {
    unlock(&ctq->context.mutex);
    return;  // deadline already set
  }

  if (tt->timeout_pending) {
    // A tie. Undo the timeout and allow the new deadline to dictate.
    lock(&pc->context.mutex);
    tt->timeout_pending = false;
    pc->tick_pending = false;
    unlock(&pc->context.mutex);
  }

  if (deadline && deadline < tt->tick_deadline && tt->list_deadline) {
    if (tt->resequenced)
      EPOLL_FATAL("idle timeout sequencing error", 0); // Can happen at most once.
    // Ensure tt is consistent for finalizing when list_deadline expires.
    tt->tick_deadline = 0;
    tt->connection = NULL;
    pc->timer = NULL;
    unlock(&ctq->context.mutex);
    // Create replacement timer for life of connection.
    pn_tick_timer_t *new_tt = pn_tick_timer_new();
    if (!new_tt)
      EPOLL_FATAL("idle timeout sequencing error", errno);
    lock(&ctq->context.mutex);
    tt = new_tt;
    tt->resequenced = true;
    tt->connection = pc;
    pc->timer = tt;
  }

  tt->tick_deadline = deadline;
  if (tt->tick_deadline) {
    assert(tt->tick_deadline > tt->list_deadline);
    // If not on list, add it.
    if (!tt->list_deadline) {
      tt->list_deadline = tt->tick_deadline;
      pn_list_minpush(ctq->pending_ticks, tt);
    }
  }
  notify = ctimerq_adjust_deadline(ctq, now);
  unlock(&ctq->context.mutex);
  
  if (notify)
    wake_notify(&ctq->context);
}

pn_event_batch_t *ctimerq_process(connection_timerq_t *ctq, bool timeout) {
  bool notify;
  if (timeout)
    ptimer_callback(&ctq->timer);  // ptimer accounting after timerfd event
  uint64_t now = pn_proactor_now_64();
  lock(&ctq->context.mutex);
  ctq->context.working = true;
  if (timeout)
    ctq->ctq_deadline = 0;
  while (pn_list_size(ctq->pending_ticks)) {
    // Find all expired tick timers at front of ordered list.
    pn_tick_timer_t *tt = (pn_tick_timer_t *) pn_list_get(ctq->pending_ticks, 0);
    if (tt->list_deadline > now)
      break;

    // Expired. Remove from list.
    pn_tick_timer_t *min = (pn_tick_timer_t *) pn_list_minpop(ctq->pending_ticks);
    assert (min == tt);
    tt->list_deadline = 0;

    // Three possibilities to act on:
    //   timer expired -> set tick_pending and wake connection
    //   timer deadline extended -> minpush back on list to new spot
    //   connection has deregistered -> free the tick timer memory
    bool do_free = false;
    bool relist = false;
    bool expired = false;
    pconnection_t *pc = min->connection;
    bool notify = false;
    if (!pc)
      do_free = true;
    else {
      if (tt->tick_deadline && tt->tick_deadline <= now) {
        expired = true;
      } else {
        relist = true;
        tt->list_deadline = tt->tick_deadline;
      }
    }

    // At end of this if-else block : ctq->context.mutex is held, pc->context.mutex not held.
    if (do_free) {
      unlock(&ctq->context.mutex);
      pn_free(tt);
      lock(&ctq->context.mutex);
    } else if (relist) {
      pn_list_minpush(ctq->pending_ticks, tt);
    } else if (expired) {
      notify = false;
      lock(&pc->context.mutex);
      if (!pc->context.closing) {
        tt->timeout_pending = true;
        pc->tick_pending = true;
        notify = wake(&pc->context);
      }
      unlock(&pc->context.mutex);
      if (notify) {
        unlock(&ctq->context.mutex);
        wake_notify(&pc->context);
        lock(&ctq->context.mutex);
      }
    }
  }

  ctq->context.working = false;  // must be false for adjust_deadline to do adjustment
  notify = ctimerq_adjust_deadline(ctq, now);
  unlock(&ctq->context.mutex);
  if (notify)
    wake_notify(&ctq->context);
  if (timeout)
    rearm_polling(&ctq->timer.epoll_io, ctq->context.proactor->epollfd);
  // The ctimerq never has events to batch.
  return NULL;
  // TODO: perhaps become context of first timed out connection (if no other conflict) and return pconnection_process() on that.
}
