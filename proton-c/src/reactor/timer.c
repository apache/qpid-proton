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

#include <proton/object.h>
#include <proton/reactor.h>
#include <assert.h>

struct pn_task_t {
  pn_list_t *pool;
  pn_record_t *attachments;
  pn_timestamp_t deadline;
  bool cancelled;
};

void pn_task_initialize(pn_task_t *task) {
  task->pool = NULL;
  task->attachments = pn_record();
  task->deadline = 0;
  task->cancelled = false;
}

void pn_task_finalize(pn_task_t *task) {
  // if we are the last reference to the pool then don't put ourselves
  // into it
  if (task->pool && pn_refcount(task->pool) > 1) {
    pn_record_clear(task->attachments);
    pn_list_add(task->pool, task);
    pn_decref(task->pool);
    task->pool = NULL;
  } else {
    pn_decref(task->pool);
    pn_decref(task->attachments);
  }
}

intptr_t pn_task_compare(pn_task_t *a, pn_task_t *b) {
  return a->deadline - b->deadline;
}

#define pn_task_inspect NULL
#define pn_task_hashcode NULL

PN_CLASSDEF(pn_task)

pn_task_t *pn_task(void) {
  pn_task_t *task = pn_task_new();
  return task;
}

pn_record_t *pn_task_attachments(pn_task_t *task) {
  assert(task);
  return task->attachments;
}

void pn_task_cancel(pn_task_t *task) {
    assert(task);
    task->cancelled = true;
}

//
// timer
//

struct pn_timer_t {
  pn_list_t *pool;
  pn_list_t *tasks;
  pn_collector_t *collector;
};

static void pn_timer_initialize(pn_timer_t *timer) {
  timer->pool = pn_list(PN_OBJECT, 0);
  timer->tasks = pn_list(PN_OBJECT, 0);
}

static void pn_timer_finalize(pn_timer_t *timer) {
  pn_decref(timer->pool);
  pn_free(timer->tasks);
}

#define pn_timer_inspect NULL
#define pn_timer_compare NULL
#define pn_timer_hashcode NULL

PN_CLASSDEF(pn_timer)

pn_timer_t *pn_timer(pn_collector_t *collector) {
  pn_timer_t *timer = pn_timer_new();
  timer->collector = collector;
  return timer;
}

pn_task_t *pn_timer_schedule(pn_timer_t *timer,  pn_timestamp_t deadline) {
  pn_task_t *task = (pn_task_t *) pn_list_pop(timer->pool);
  if (!task) {
    task = pn_task();
  }
  task->pool = timer->pool;
  pn_incref(task->pool);
  task->deadline = deadline;
  task->cancelled = false;
  pn_list_minpush(timer->tasks, task);
  pn_decref(task);
  return task;
}

void pni_timer_flush_cancelled(pn_timer_t *timer) {
    while (pn_list_size(timer->tasks)) {
        pn_task_t *task = (pn_task_t *) pn_list_get(timer->tasks, 0);
        if (task->cancelled) {
            pn_task_t *min = (pn_task_t *) pn_list_minpop(timer->tasks);
            assert(min == task);
            pn_decref(min);
        } else {
            break;
        }
    }
}

pn_timestamp_t pn_timer_deadline(pn_timer_t *timer) {
  assert(timer);
  pni_timer_flush_cancelled(timer);
  if (pn_list_size(timer->tasks)) {
    pn_task_t *task = (pn_task_t *) pn_list_get(timer->tasks, 0);
    return task->deadline;
  } else {
    return 0;
  }
}

void pn_timer_tick(pn_timer_t *timer, pn_timestamp_t now) {
  assert(timer);
  while (pn_list_size(timer->tasks)) {
    pn_task_t *task = (pn_task_t *) pn_list_get(timer->tasks, 0);
    if (now >= task->deadline) {
      pn_task_t *min = (pn_task_t *) pn_list_minpop(timer->tasks);
      assert(min == task);
      if (!min->cancelled)
          pn_collector_put(timer->collector, PN_OBJECT, min, PN_TIMER_TASK);
      pn_decref(min);
    } else {
      break;
    }
  }
}

int pn_timer_tasks(pn_timer_t *timer) {
  assert(timer);
  pni_timer_flush_cancelled(timer);
  return pn_list_size(timer->tasks);
}
