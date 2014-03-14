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
 * Copy of posix poll-based selector with minimal changes to use
 * select().  TODO: fully native implementaton with I/O completion
 * ports.
 *
 * This implementation comments out the posix max_fds arg to select
 * which has no meaning on windows.  The number of fd_set slots are
 * configured at compile time via FD_SETSIZE, chosen "large enough"
 * for the limited scalability of select() at the expense of
 * 3*N*sizeof(unsigned int) bytes per driver instance.  select (and
 * associated macros like FD_ZERO) are otherwise unaffected
 * performance-wise by increasing FD_SETSIZE.
 */

#define FD_SETSIZE 2048
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#define PN_WINAPI

#include "../platform.h"
#include <proton/io.h>
#include <proton/selector.h>
#include <proton/error.h>
#include <assert.h>
#include "../selectable.h"
#include "../util.h"

struct pn_selector_t {
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
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
  free(selector->deadlines);
  pn_free(selector->selectables);
  pn_error_free(selector->error);
}

#define pn_selector_hashcode NULL
#define pn_selector_compare NULL
#define pn_selector_inspect NULL

pn_selector_t *pn_selector(void)
{
  static pn_class_t clazz = PN_CLASS(pn_selector);
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
 /*
  selector->fds[idx].fd = pn_selectable_fd(selectable);
  selector->fds[idx].events = 0;
  selector->fds[idx].revents = 0;
  if (pn_selectable_capacity(selectable) > 0) {
    selector->fds[idx].events |= POLLIN;
  }
  if (pn_selectable_pending(selectable) > 0) {
    selector->fds[idx].events |= POLLOUT;
  }
 */
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
  }

  pni_selectable_set_index(selectable, -1);
}

int pn_selector_select(pn_selector_t *selector, int timeout)
{
  assert(selector);

  FD_ZERO(&selector->readfds);
  FD_ZERO(&selector->writefds);
  FD_ZERO(&selector->exceptfds);

  size_t size = pn_list_size(selector->selectables);
  if (size > FD_SETSIZE) {
    // This Windows limitation will go away when switching to completion ports
    pn_error_set(selector->error, PN_ERR, "maximum sockets exceeded for Windows selector");
    return PN_ERR;
  }

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

  struct timeval to = {0};
  struct timeval *to_arg = &to;
  // block only if (timeout == 0) and (closed_count == 0)
  if (timeout > 0) {
    // convert millisecs to sec and usec:
    to.tv_sec = timeout/1000;
    to.tv_usec = (timeout - (to.tv_sec * 1000)) * 1000;
  }
  else if (timeout < 0) {
    to_arg = NULL;
  }

  for (size_t i = 0; i < size; i++) {
    pn_selectable_t *sel = (pn_selectable_t *) pn_list_get(selector->selectables, i);
    pn_socket_t fd = pn_selectable_fd(sel);
    if (pn_selectable_capacity(sel) > 0) {
      FD_SET(fd, &selector->readfds);
    }
    if (pn_selectable_pending(sel) > 0) {
      FD_SET(fd, &selector->writefds);
    }
  }

  int result = select(0 /* ignored in win32 */, &selector->readfds, &selector->writefds, &selector->exceptfds, to_arg);
  if (result == -1) {
    pn_i_error_from_errno(selector->error, "select");
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
    pn_timestamp_t deadline = selector->deadlines[selector->current];
    int ev = 0;
    pn_socket_t fd = pn_selectable_fd(sel);
    if (FD_ISSET(fd, &selector->readfds)) {
      ev |= PN_READABLE;
    }
    if (FD_ISSET(fd, &selector->writefds)) {
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
