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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#define PN_WINAPI

#include "platform.h"
#include <proton/object.h>
#include <proton/io.h>
#include <proton/selector.h>
#include <proton/error.h>
#include <assert.h>
#include "selectable.h"
#include "util.h"
#include "iocp.h"

static void interests_update(iocpdesc_t *iocpd, int interests);
static void deadlines_update(iocpdesc_t *iocpd, pn_timestamp_t t);

struct pn_selector_t {
  iocp_t *iocp;
  pn_timestamp_t *deadlines;
  size_t capacity;
  pn_list_t *selectables;
  pn_list_t *iocp_descriptors;
  pn_timestamp_t deadline;
  size_t current;
  iocpdesc_t *current_triggered;
  pn_timestamp_t awoken;
  pn_error_t *error;
  iocpdesc_t *triggered_list_head;
  iocpdesc_t *triggered_list_tail;
  iocpdesc_t *deadlines_head;
  iocpdesc_t *deadlines_tail;
};

void pn_selector_initialize(void *obj)
{
  pn_selector_t *selector = (pn_selector_t *) obj;
  selector->iocp = NULL;
  selector->deadlines = NULL;
  selector->capacity = 0;
  selector->selectables = pn_list(PN_WEAKREF, 0);
  selector->iocp_descriptors = pn_list(PN_OBJECT, 0);
  selector->deadline = 0;
  selector->current = 0;
  selector->current_triggered = NULL;
  selector->awoken = 0;
  selector->error = pn_error();
  selector->triggered_list_head = NULL;
  selector->triggered_list_tail = NULL;
  selector->deadlines_head = NULL;
  selector->deadlines_tail = NULL;
}

void pn_selector_finalize(void *obj)
{
  pn_selector_t *selector = (pn_selector_t *) obj;
  free(selector->deadlines);
  pn_free(selector->selectables);
  pn_free(selector->iocp_descriptors);
  pn_error_free(selector->error);
  selector->iocp->selector = NULL;
}

#define pn_selector_hashcode NULL
#define pn_selector_compare NULL
#define pn_selector_inspect NULL

pn_selector_t *pni_selector()
{
  static const pn_class_t clazz = PN_CLASS(pn_selector);
  pn_selector_t *selector = (pn_selector_t *) pn_class_new(&clazz, sizeof(pn_selector_t));
  return selector;
}

pn_selector_t *pni_selector_create(iocp_t *iocp)
{
  pn_selector_t *selector = pni_selector();
  selector->iocp = iocp;
  return selector;
}

void pn_selector_add(pn_selector_t *selector, pn_selectable_t *selectable)
{
  assert(selector);
  assert(selectable);
  assert(pni_selectable_get_index(selectable) < 0);
  pn_socket_t sock = pn_selectable_fd(selectable);

  iocpdesc_t *iocpd = NULL;
  if (sock != INVALID_SOCKET) {
    iocpd = pni_iocpdesc_map_get(selector->iocp, sock);
    if (!iocpd) {
      // Socket created outside proton.  Hook it up to iocp.
      iocpd = pni_iocpdesc_create(selector->iocp, sock, true);
      pni_iocpdesc_start(iocpd);
    } else {
      assert(iocpd->iocp == selector->iocp);
    }
  }

  if (pni_selectable_get_index(selectable) < 0) {
    pn_list_add(selector->selectables, selectable);
    pn_list_add(selector->iocp_descriptors, iocpd);
    size_t size = pn_list_size(selector->selectables);

    if (selector->capacity < size) {
      selector->deadlines = (pn_timestamp_t *) realloc(selector->deadlines, size*sizeof(pn_timestamp_t));
      selector->capacity = size;
    }

    pni_selectable_set_index(selectable, size - 1);
    if (iocpd) {
      iocpd->selector = selector;
      iocpd->selectable = selectable;
    }
  }

  pn_selector_update(selector, selectable);
}

void pn_selector_update(pn_selector_t *selector, pn_selectable_t *selectable)
{
  int idx = pni_selectable_get_index(selectable);
  assert(idx >= 0);
  selector->deadlines[idx] = pn_selectable_deadline(selectable);

  pn_socket_t sock = pn_selectable_fd(selectable);
  iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(selector->iocp_descriptors, idx);
  if (iocpd) {
    assert(sock == iocpd->socket || iocpd->closing);
    int interests = 0;
    if (pn_selectable_capacity(selectable) > 0) {
      interests |= PN_READABLE;
    }
    if (pn_selectable_pending(selectable) > 0) {
      interests |= PN_WRITABLE;
    }
    interests_update(iocpd, interests);
    deadlines_update(iocpd, selector->deadlines[idx]);
  }
}

void pn_selector_remove(pn_selector_t *selector, pn_selectable_t *selectable)
{
  assert(selector);
  assert(selectable);

  int idx = pni_selectable_get_index(selectable);
  assert(idx >= 0);
  iocpdesc_t *iocpd = (iocpdesc_t *) pn_list_get(selector->iocp_descriptors, idx);
  if (iocpd) {
    if (selector->current_triggered == iocpd)
      selector->current_triggered = iocpd->triggered_list_next;
    interests_update(iocpd, 0);
    deadlines_update(iocpd, 0);
    assert(selector->triggered_list_head != iocpd && !iocpd->triggered_list_prev);
    assert(selector->deadlines_head != iocpd && !iocpd->deadlines_prev);
    iocpd->selector = NULL;
    iocpd->selectable = NULL;
  }
  pn_list_del(selector->selectables, idx, 1);
  pn_list_del(selector->iocp_descriptors, idx, 1);
  size_t size = pn_list_size(selector->selectables);
  for (size_t i = idx; i < size; i++) {
    pn_selectable_t *sel = (pn_selectable_t *) pn_list_get(selector->selectables, i);
    pni_selectable_set_index(sel, i);
  }
  pni_selectable_set_index(selectable, -1);
}

int pn_selector_select(pn_selector_t *selector, int timeout)
{
  assert(selector);
  pn_error_clear(selector->error);
  pn_timestamp_t deadline = 0;
  pn_timestamp_t now = pn_i_now();

  if (timeout) {
    if (selector->deadlines_head)
      deadline = selector->deadlines_head->deadline;
  }
  if (deadline) {
    int delta = deadline - now;
    if (delta < 0) {
      delta = 0;
    } 
    if (timeout < 0)
      timeout = delta;
    else if (timeout > delta)
      timeout = delta;
  }	
  deadline = (timeout >= 0) ? now + timeout : 0;

  // Process all currently available completions, even if matched events available
  pni_iocp_drain_completions(selector->iocp);
  pni_zombie_check(selector->iocp, now);
  // Loop until an interested event is matched, or until deadline
  while (true) {
    if (selector->triggered_list_head)
      break;
    if (deadline && deadline <= now)
      break;
    pn_timestamp_t completion_deadline = deadline;
    pn_timestamp_t zd = pni_zombie_deadline(selector->iocp);
    if (zd)
      completion_deadline = completion_deadline ? pn_min(zd, completion_deadline) : zd;

    int completion_timeout = (!completion_deadline) ? -1 : completion_deadline - now;
    int rv = pni_iocp_wait_one(selector->iocp, completion_timeout, selector->error);
    if (rv < 0)
      return pn_error_code(selector->error);

    now = pn_i_now();
    if (zd && zd <= now) {
      pni_zombie_check(selector->iocp, now);
    }
  }

  selector->current = 0;
  selector->awoken = now;
  selector->current_triggered = selector->triggered_list_head;
  for (iocpdesc_t *iocpd = selector->deadlines_head; iocpd; iocpd = iocpd->deadlines_next) {
    if (iocpd->deadline <= now)
      pni_events_update(iocpd, iocpd->events | PN_EXPIRED);
    else
      break;
  }
  return pn_error_code(selector->error);
}

pn_selectable_t *pn_selector_next(pn_selector_t *selector, int *events)
{
  if (selector->current_triggered) {
    iocpdesc_t *iocpd = selector->current_triggered;
    *events = iocpd->interests & iocpd->events;
    selector->current_triggered = iocpd->triggered_list_next;
    return iocpd->selectable;
  }
  return NULL;
}

void pn_selector_free(pn_selector_t *selector)
{
  assert(selector);
  pn_free(selector);
}


static void triggered_list_add(pn_selector_t *selector, iocpdesc_t *iocpd)
{
  if (iocpd->triggered_list_prev || selector->triggered_list_head == iocpd)
    return; // already in list
  LL_ADD(selector, triggered_list, iocpd);
}

static void triggered_list_remove(pn_selector_t *selector, iocpdesc_t *iocpd)
{
  if (!iocpd->triggered_list_prev && selector->triggered_list_head != iocpd)
    return; // not in list
  LL_REMOVE(selector, triggered_list, iocpd);
  iocpd->triggered_list_prev = NULL;
  iocpd->triggered_list_next = NULL;
}


void pni_events_update(iocpdesc_t *iocpd, int events)
{
  int old_events = iocpd->events;
  if (old_events == events)
    return;
  iocpd->events = events;
  if (iocpd->selector) {
    if (iocpd->events & iocpd->interests)
      triggered_list_add(iocpd->selector, iocpd);
    else
      triggered_list_remove(iocpd->selector, iocpd);
  }
}

static void interests_update(iocpdesc_t *iocpd, int interests)
{
  int old_interests = iocpd->interests;
  if (old_interests == interests)
    return;
  iocpd->interests = interests;
  if (iocpd->selector) {
    if (iocpd->events & iocpd->interests)
      triggered_list_add(iocpd->selector, iocpd);
    else
      triggered_list_remove(iocpd->selector, iocpd);
  }
}

static void deadlines_remove(pn_selector_t *selector, iocpdesc_t *iocpd)
{
  if (!iocpd->deadlines_prev && selector->deadlines_head != iocpd)
    return; // not in list
  LL_REMOVE(selector, deadlines, iocpd);
  iocpd->deadlines_prev = NULL;
  iocpd->deadlines_next = NULL;
}


static void deadlines_update(iocpdesc_t *iocpd, pn_timestamp_t deadline)
{
  if (deadline == iocpd->deadline)
    return;
  iocpd->deadline = deadline;
  pn_selector_t *selector = iocpd->selector;
  if (!deadline) {
    deadlines_remove(selector, iocpd);
    pni_events_update(iocpd, iocpd->events & ~PN_EXPIRED);
    interests_update(iocpd, iocpd->interests & ~PN_EXPIRED);
  } else {
    if (iocpd->deadlines_prev || selector->deadlines_head == iocpd) {
      deadlines_remove(selector, iocpd);
      pni_events_update(iocpd, iocpd->events & ~PN_EXPIRED);
    }
    interests_update(iocpd, iocpd->interests | PN_EXPIRED);
    iocpdesc_t *dl_iocpd = LL_HEAD(selector, deadlines);
    while (dl_iocpd && dl_iocpd->deadline <= deadline)
      dl_iocpd = dl_iocpd->deadlines_next;
    if (dl_iocpd) {
      // insert
      iocpd->deadlines_prev = dl_iocpd->deadlines_prev;
      iocpd->deadlines_next = dl_iocpd;
      dl_iocpd->deadlines_prev = iocpd;
      if (selector->deadlines_head == dl_iocpd)
        selector->deadlines_head = iocpd;
    } else {
      LL_ADD(selector, deadlines, iocpd);  // append
    }
  }
}
