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

#include <proton/error.h>
#include <proton/io.h>
#include "selectable.h"
#include <stdlib.h>
#include <assert.h>

pn_selectables_t *pn_selectables(void)
{
  return pn_iterator();
}

pn_selectable_t *pn_selectables_next(pn_selectables_t *selectables)
{
  return (pn_selectable_t *) pn_iterator_next(selectables);
}

void pn_selectables_free(pn_selectables_t *selectables)
{
  pn_free(selectables);
}

struct pn_selectable_t {
  pn_socket_t fd;
  int index;
  void *context;
  ssize_t (*capacity)(pn_selectable_t *);
  ssize_t (*pending)(pn_selectable_t *);
  pn_timestamp_t (*deadline)(pn_selectable_t *);
  void (*readable)(pn_selectable_t *);
  void (*writable)(pn_selectable_t *);
  void (*expired)(pn_selectable_t *);
  void (*finalize)(pn_selectable_t *);
  bool registered;
  bool terminal;
};

void pn_selectable_initialize(void *obj)
{
  pn_selectable_t *sel = (pn_selectable_t *) obj;
  sel->fd = PN_INVALID_SOCKET;
  sel->index = -1;
  sel->context = NULL;
  sel->capacity = NULL;
  sel->deadline = NULL;
  sel->pending = NULL;
  sel->readable = NULL;
  sel->writable = NULL;
  sel->expired = NULL;
  sel->finalize = NULL;
  sel->registered = false;
  sel->terminal = false;
}

void pn_selectable_finalize(void *obj)
{
  pn_selectable_t *sel = (pn_selectable_t *) obj;
  sel->finalize(sel);
}

#define pn_selectable_hashcode NULL
#define pn_selectable_inspect NULL
#define pn_selectable_compare NULL

pn_selectable_t *pni_selectable(ssize_t (*capacity)(pn_selectable_t *),
                                ssize_t (*pending)(pn_selectable_t *),
                                pn_timestamp_t (*deadline)(pn_selectable_t *),
                                void (*readable)(pn_selectable_t *),
                                void (*writable)(pn_selectable_t *),
                                void (*expired)(pn_selectable_t *),
                                void (*finalize)(pn_selectable_t *))
{
  static pn_class_t clazz = PN_CLASS(pn_selectable);
  pn_selectable_t *selectable = (pn_selectable_t *) pn_new(sizeof(pn_selectable_t), &clazz);
  selectable->capacity = capacity;
  selectable->pending = pending;
  selectable->readable = readable;
  selectable->deadline = deadline;
  selectable->writable = writable;
  selectable->expired = expired;
  selectable->finalize = finalize;
  return selectable;
}

void *pni_selectable_get_context(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->context;
}

void pni_selectable_set_context(pn_selectable_t *selectable, void *context)
{
  assert(selectable);
  selectable->context = context;
}

int pni_selectable_get_index(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->index;
}

void pni_selectable_set_index(pn_selectable_t *selectable, int index)
{
  assert(selectable);
  selectable->index = index;
}

pn_socket_t pn_selectable_fd(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->fd;
}

void pni_selectable_set_fd(pn_selectable_t *selectable, pn_socket_t fd)
{
  assert(selectable);
  selectable->fd = fd;
}

ssize_t pn_selectable_capacity(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->capacity(selectable);
}

ssize_t pn_selectable_pending(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->pending(selectable);
}

pn_timestamp_t pn_selectable_deadline(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->deadline(selectable);
}

void pn_selectable_readable(pn_selectable_t *selectable)
{
  assert(selectable);
  selectable->readable(selectable);
}

void pn_selectable_writable(pn_selectable_t *selectable)
{
  assert(selectable);
  selectable->writable(selectable);
}

void pn_selectable_expired(pn_selectable_t *selectable)
{
  assert(selectable);
  selectable->expired(selectable);
}

bool pn_selectable_is_registered(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->registered;
}

void pn_selectable_set_registered(pn_selectable_t *selectable, bool registered)
{
  assert(selectable);
  selectable->registered = registered;
}

bool pn_selectable_is_terminal(pn_selectable_t *selectable)
{
  assert(selectable);
  if (!selectable->terminal) {
    selectable->terminal = (pn_selectable_capacity(selectable) < 0 &&
                            pn_selectable_pending(selectable) < 0);
  }
  return selectable->terminal;
}

void pni_selectable_set_terminal(pn_selectable_t *selectable, bool terminal)
{
  assert(selectable);
  selectable->terminal = terminal;
}

void pn_selectable_free(pn_selectable_t *selectable)
{
  pn_free(selectable);
}
