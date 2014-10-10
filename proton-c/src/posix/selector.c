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

#include <proton/selector.h>
#include <proton/error.h>
#include <poll.h>
#include <stdlib.h>
#include <assert.h>
#include "../platform.h"
#include "../selectable.h"
#include "../util.h"

struct pn_selector_t {
  struct pollfd *fds;
  pn_timestamp_t *deadlines;
  size_t capacity;
  pn_list_t *selectables;
  pn_timestamp_t deadline;
  size_t current;
  pn_timestamp_t awoken;
  pn_error_t *error;
};

void pn_selector_initialize(void *obj)
{
  pn_selector_t *selector = (pn_selector_t *) obj;
  selector->fds = NULL;
  selector->deadlines = NULL;
  selector->capacity = 0;
  selector->selectables = pn_list(0, 0);
  selector->deadline = 0;
  selector->current = 0;
  selector->awoken = 0;
  selector->error = pn_error();
}

void pn_selector_finalize(void *obj)
{
  pn_selector_t *selector = (pn_selector_t *) obj;
  free(selector->fds);
  free(selector->deadlines);
  pn_free(selector->selectables);
  pn_error_free(selector->error);
}

#define pn_selector_hashcode NULL
#define pn_selector_compare NULL
#define pn_selector_inspect NULL

pn_selector_t *pni_selector(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_selector);
  pn_selector_t *selector = (pn_selector_t *) pn_new(sizeof(pn_selector_t), &clazz);
  return selector;
}

void pn_selector_add(pn_selector_t *selector, pn_selectable_t *selectable)
{
  assert(selector);
  assert(selectable);
  assert(pni_selectable_get_index(selectable) < 0);

  if (pni_selectable_get_index(selectable) < 0) {
    pn_list_add(selector->selectables, selectable);
    size_t size = pn_list_size(selector->selectables);

    if (selector->capacity < size) {
      selector->fds = (struct pollfd *) realloc(selector->fds, size*sizeof(struct pollfd));
      selector->deadlines = (pn_timestamp_t *) realloc(selector->deadlines, size*sizeof(pn_timestamp_t));
      selector->capacity = size;
    }

    pni_selectable_set_index(selectable, size - 1);
  }

  pn_selector_update(selector, selectable);
}

void pn_selector_update(pn_selector_t *selector, pn_selectable_t *selectable)
{
  int idx = pni_selectable_get_index(selectable);
  assert(idx >= 0);
  selector->fds[idx].fd = pn_selectable_fd(selectable);
  selector->fds[idx].events = 0;
  selector->fds[idx].revents = 0;
  if (pn_selectable_capacity(selectable) > 0) {
    selector->fds[idx].events |= POLLIN;
  }
  if (pn_selectable_pending(selectable) > 0) {
    selector->fds[idx].events |= POLLOUT;
  }
  selector->deadlines[idx] = pn_selectable_deadline(selectable);
}

void pn_selector_remove(pn_selector_t *selector, pn_selectable_t *selectable)
{
  assert(selector);
  assert(selectable);

  int idx = pni_selectable_get_index(selectable);
  assert(idx >= 0);
  pn_list_del(selector->selectables, idx, 1);
  size_t size = pn_list_size(selector->selectables);
  for (size_t i = idx; i < size; i++) {
    pn_selectable_t *sel = (pn_selectable_t *) pn_list_get(selector->selectables, i);
    pni_selectable_set_index(sel, i);
    selector->fds[i] = selector->fds[i + 1];
  }

  pni_selectable_set_index(selectable, -1);
}

int pn_selector_select(pn_selector_t *selector, int timeout)
{
  assert(selector);

  size_t size = pn_list_size(selector->selectables);

  if (timeout) {
    pn_timestamp_t deadline = 0;
    for (size_t i = 0; i < size; i++) {
      pn_timestamp_t d = selector->deadlines[i];
      if (d)
        deadline = (deadline == 0) ? d : pn_min(deadline, d);
    }

    if (deadline) {
      pn_timestamp_t now = pn_i_now();
      int delta = selector->deadline - now;
      if (delta < 0) {
        timeout = 0;
      } else if (delta < timeout) {
        timeout = delta;
      }
    }
  }

  int result = poll(selector->fds, size, timeout);
  if (result == -1) {
    pn_i_error_from_errno(selector->error, "poll");
  } else {
    selector->current = 0;
    selector->awoken = pn_i_now();
  }

  return pn_error_code(selector->error);
}

pn_selectable_t *pn_selector_next(pn_selector_t *selector, int *events)
{
  pn_list_t *l = selector->selectables;
  size_t size = pn_list_size(l);
  while (selector->current < size) {
    pn_selectable_t *sel = (pn_selectable_t *) pn_list_get(l, selector->current);
    struct pollfd *pfd = &selector->fds[selector->current];
    pn_timestamp_t deadline = selector->deadlines[selector->current];
    int ev = 0;
    if (pfd->revents & POLLIN) {
      ev |= PN_READABLE;
    }
    if (pfd->revents & POLLOUT) {
      ev |= PN_WRITABLE;
    }
    if (deadline && selector->awoken >= deadline) {
      ev |= PN_EXPIRED;
    }
    selector->current++;
    if (ev) {
      *events = ev;
      return sel;
    }
  }
  return NULL;
}

void pn_selector_free(pn_selector_t *selector)
{
  assert(selector);
  pn_free(selector);
}
