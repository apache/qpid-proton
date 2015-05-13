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
#include <proton/event.h>
#include <string.h>
#include <assert.h>

struct pn_handler_t {
  void (*dispatch) (pn_handler_t *, pn_event_t *, pn_event_type_t);
  void (*finalize) (pn_handler_t *);
  pn_list_t *children;
};

void pn_handler_initialize(void *object) {
  pn_handler_t *handler = (pn_handler_t *) object;
  handler->dispatch = NULL;
  handler->children = NULL;
}

void pn_handler_finalize(void *object) {
  pn_handler_t *handler = (pn_handler_t *) object;
  if (handler->finalize) {
    handler->finalize(handler);
  }
  pn_free(handler->children);
}

#define pn_handler_hashcode NULL
#define pn_handler_compare NULL
#define pn_handler_inspect NULL

pn_handler_t *pn_handler(void (*dispatch)(pn_handler_t *, pn_event_t *, pn_event_type_t)) {
  return pn_handler_new(dispatch, 0, NULL);
}

pn_handler_t *pn_handler_new(void (*dispatch)(pn_handler_t *, pn_event_t *, pn_event_type_t), size_t size,
                             void (*finalize)(pn_handler_t *)) {
  static const pn_class_t clazz = PN_CLASS(pn_handler);
  pn_handler_t *handler = (pn_handler_t *) pn_class_new(&clazz, sizeof(pn_handler_t) + size);
  handler->dispatch = dispatch;
  handler->finalize = finalize;
  memset(pn_handler_mem(handler), 0, size);
  return handler;
}

void pn_handler_free(pn_handler_t *handler) {
  if (handler) {
    if (handler->children) {
      size_t n = pn_list_size(handler->children);
      for (size_t i = 0; i < n; i++) {
        void *child = pn_list_get(handler->children, i);
        pn_decref(child);
      }
    }

    pn_decref(handler);
  }
}

void *pn_handler_mem(pn_handler_t *handler) {
  return (void *) (handler + 1);
}

void pn_handler_add(pn_handler_t *handler, pn_handler_t *child) {
  assert(handler);
  if (!handler->children) {
    handler->children = pn_list(PN_OBJECT, 0);
  }
  pn_list_add(handler->children, child);
}

void pn_handler_clear(pn_handler_t *handler) {
  assert(handler);
  if (handler->children) {
    pn_list_clear(handler->children);
  }
}

void pn_handler_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  assert(handler);
  if (handler->dispatch) {
    handler->dispatch(handler, event, type);
  }
  if (handler->children) {
    size_t n = pn_list_size(handler->children);
    for (size_t i = 0; i < n; i++) {
      pn_handler_t *child = (pn_handler_t *) pn_list_get(handler->children, i);
      pn_handler_dispatch(child, event, type);
    }
  }
}
