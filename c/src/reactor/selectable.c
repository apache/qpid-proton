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

#include "selectable.h"

#include <proton/error.h>

#include "io.h"

#include <assert.h>
#include <stdlib.h>

/*
 * These are totally unused (and unusable) but these definitions have been
 * retained to maintain the external linkage symbols
 */

PNX_EXTERN pn_iterator_t *pn_selectables(void)
{
  return pn_iterator();
}

PNX_EXTERN void *pn_selectables_next(pn_iterator_t *selectables)
{
  return pn_iterator_next(selectables);
}

PNX_EXTERN void pn_selectables_free(pn_iterator_t *selectables)
{
  pn_free(selectables);
}

struct pn_selectable_t {
  pn_socket_t fd;
  int index;
  pn_record_t *attachments;
  void (*readable)(pn_selectable_t *);
  void (*writable)(pn_selectable_t *);
  void (*error)(pn_selectable_t *);
  void (*expired)(pn_selectable_t *);
  void (*release) (pn_selectable_t *);
  void (*finalize)(pn_selectable_t *);
  pn_collector_t *collector;
  pn_timestamp_t deadline;
  bool reading;
  bool writing;
  bool registered;
  bool terminal;
};

void pn_selectable_initialize(pn_selectable_t *sel)
{
  sel->fd = PN_INVALID_SOCKET;
  sel->index = -1;
  sel->attachments = pn_record();
  sel->readable = NULL;
  sel->writable = NULL;
  sel->error = NULL;
  sel->expired = NULL;
  sel->release = NULL;
  sel->finalize = NULL;
  sel->collector = NULL;
  sel->deadline = 0;
  sel->reading = false;
  sel->writing = false;
  sel->registered = false;
  sel->terminal = false;
}

void pn_selectable_finalize(pn_selectable_t *sel)
{
  if (sel->finalize) {
    sel->finalize(sel);
  }
  pn_decref(sel->attachments);
  pn_decref(sel->collector);
}

#define pn_selectable_hashcode NULL
#define pn_selectable_inspect NULL
#define pn_selectable_compare NULL

PN_CLASSDEF(pn_selectable)

pn_selectable_t *pn_selectable(void)
{
  return pn_selectable_new();
}

bool pn_selectable_is_reading(pn_selectable_t *sel) {
  assert(sel);
  return sel->reading;
}

void pn_selectable_set_reading(pn_selectable_t *sel, bool reading) {
  assert(sel);
  sel->reading = reading;
}

bool pn_selectable_is_writing(pn_selectable_t *sel) {
  assert(sel);
  return sel->writing;
}

void pn_selectable_set_writing(pn_selectable_t *sel, bool writing) {
  assert(sel);
  sel->writing = writing;
}

pn_timestamp_t pn_selectable_get_deadline(pn_selectable_t *sel) {
  assert(sel);
  return sel->deadline;
}

void pn_selectable_set_deadline(pn_selectable_t *sel, pn_timestamp_t deadline) {
  assert(sel);
  sel->deadline = deadline;
}

void pn_selectable_on_readable(pn_selectable_t *sel, void (*readable)(pn_selectable_t *)) {
  assert(sel);
  sel->readable = readable;
}

void pn_selectable_on_writable(pn_selectable_t *sel, void (*writable)(pn_selectable_t *)) {
  assert(sel);
  sel->writable = writable;
}

void pn_selectable_on_error(pn_selectable_t *sel, void (*error)(pn_selectable_t *)) {
  assert(sel);
  sel->error = error;
}

void pn_selectable_on_expired(pn_selectable_t *sel, void (*expired)(pn_selectable_t *)) {
  assert(sel);
  sel->expired = expired;
}

void pn_selectable_on_release(pn_selectable_t *sel, void (*release)(pn_selectable_t *)) {
  assert(sel);
  sel->release = release;
}

void pn_selectable_on_finalize(pn_selectable_t *sel, void (*finalize)(pn_selectable_t *)) {
  assert(sel);
  sel->finalize = finalize;
}

pn_record_t *pn_selectable_attachments(pn_selectable_t *sel) {
  return sel->attachments;
}

void *pni_selectable_get_context(pn_selectable_t *selectable)
{
  assert(selectable);
  return pn_record_get(selectable->attachments, PN_LEGCTX);
}

void pni_selectable_set_context(pn_selectable_t *selectable, void *context)
{
  assert(selectable);
  pn_record_set(selectable->attachments, PN_LEGCTX, context);
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

pn_socket_t pn_selectable_get_fd(pn_selectable_t *selectable)
{
  assert(selectable);
  return selectable->fd;
}

void pn_selectable_set_fd(pn_selectable_t *selectable, pn_socket_t fd)
{
  assert(selectable);
  selectable->fd = fd;
}

void pn_selectable_readable(pn_selectable_t *selectable)
{
  assert(selectable);
  if (selectable->readable) {
    selectable->readable(selectable);
  }
}

void pn_selectable_writable(pn_selectable_t *selectable)
{
  assert(selectable);
  if (selectable->writable) {
    selectable->writable(selectable);
  }
}

void pn_selectable_error(pn_selectable_t *selectable)
{
  assert(selectable);
  if (selectable->error) {
    selectable->error(selectable);
  }
}

void pn_selectable_expired(pn_selectable_t *selectable)
{
  assert(selectable);
  if (selectable->expired) {
    selectable->expired(selectable);
  }
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
  return selectable->terminal;
}

void pn_selectable_terminate(pn_selectable_t *selectable)
{
  assert(selectable);
  selectable->terminal = true;
}

void pn_selectable_release(pn_selectable_t *selectable)
{
  assert(selectable);
  if (selectable->release) {
    selectable->release(selectable);
  }
}

void pn_selectable_free(pn_selectable_t *selectable)
{
  pn_decref(selectable);
}

static void pni_readable(pn_selectable_t *selectable) {
  pn_collector_put(selectable->collector, PN_OBJECT, selectable, PN_SELECTABLE_READABLE);
}

static void pni_writable(pn_selectable_t *selectable) {
  pn_collector_put(selectable->collector, PN_OBJECT, selectable, PN_SELECTABLE_WRITABLE);
}

static void pni_error(pn_selectable_t *selectable) {
  pn_collector_put(selectable->collector, PN_OBJECT, selectable, PN_SELECTABLE_ERROR);
}

static void pni_expired(pn_selectable_t *selectable) {
  pn_collector_put(selectable->collector, PN_OBJECT, selectable, PN_SELECTABLE_EXPIRED);
}

void pn_selectable_collect(pn_selectable_t *selectable, pn_collector_t *collector) {
  assert(selectable);
  pn_decref(selectable->collector);
  selectable->collector = collector;
  pn_incref(selectable->collector);

  if (collector) {
    pn_selectable_on_readable(selectable, pni_readable);
    pn_selectable_on_writable(selectable, pni_writable);
    pn_selectable_on_error(selectable, pni_error);
    pn_selectable_on_expired(selectable, pni_expired);
  }
}
