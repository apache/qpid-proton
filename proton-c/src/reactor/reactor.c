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
#include <proton/io.h>
#include <proton/selector.h>
#include <proton/event.h>
#include <proton/reactor.h>
#include <proton/transport.h>
#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "selectable.h"

struct pn_reactor_t {
  pn_record_t *attachments;
  pn_io_t *io;
  pn_selector_t *selector;
  pn_collector_t *collector;
  pn_handler_t *handler;
  pn_list_t *children;
};

void *pni_handler = NULL;

static void pn_dummy_dispatch(pn_handler_t *handler, pn_event_t *event) {
  /*pn_string_t *str = pn_string(NULL);
  pn_inspect(event, str);
  printf("%s\n", pn_string_get(str));
  pn_free(str);*/
}

static void pn_reactor_initialize(void *object) {
  pn_reactor_t *reactor = (pn_reactor_t *) object;
  reactor->attachments = pn_record();
  reactor->io = pn_io();
  reactor->selector = pn_io_selector(reactor->io);
  reactor->collector = pn_collector();
  reactor->handler = pn_handler(pn_dummy_dispatch);
  reactor->children = pn_list(PN_OBJECT, 0);
}

static void pn_reactor_finalize(void *object) {
  pn_reactor_t *reactor = (pn_reactor_t *) object;
  pn_decref(reactor->attachments);
  pn_decref(reactor->selector);
  pn_decref(reactor->io);
  pn_decref(reactor->collector);
  pn_decref(reactor->handler);
  pn_decref(reactor->children);
}

#define pn_reactor_hashcode NULL
#define pn_reactor_compare NULL
#define pn_reactor_inspect NULL

pn_reactor_t *pn_reactor() {
  static const pn_class_t clazz = PN_CLASS(pn_reactor);
  return (pn_reactor_t *) pn_class_new(&clazz, sizeof(pn_reactor_t));
}

pn_record_t *pn_reactor_attachments(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->attachments;
}

void pn_reactor_free(pn_reactor_t *reactor) {
  if (reactor) {
    pn_handler_free(reactor->handler);
    reactor->handler = NULL;
    pn_decref(reactor);
  }
}

pn_handler_t *pn_reactor_handler(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->handler;
}

pn_selector_t *pn_reactor_selector(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->selector;
}

pn_io_t *pn_reactor_io(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->io;
}

pn_collector_t *pn_reactor_collector(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->collector;
}

pn_list_t *pn_reactor_children(pn_reactor_t *reactor) {
  assert(reactor);
  return reactor->children;
}

pn_selectable_t *pn_reactor_selectable(pn_reactor_t *reactor) {
  assert(reactor);
  pn_selectable_t *sel = pn_selectable();
  pn_selector_add(reactor->selector, sel);
  pn_list_add(reactor->children, sel);
  pn_decref(sel);
  return sel;
}

void pn_reactor_update(pn_reactor_t *reactor, pn_selectable_t *selectable) {
  assert(reactor);
  pn_selector_update(reactor->selector, selectable);
}

void pni_handle_transport(pn_reactor_t *reactor, pn_event_t *event);
void pni_handle_open(pn_reactor_t *reactor, pn_event_t *event);
void pni_handle_final(pn_reactor_t *reactor, pn_event_t *event);

static void pni_reactor_dispatch(pn_reactor_t *reactor, pn_event_t *event) {
  assert(reactor);
  switch (pn_event_type(event)) {
  case PN_TRANSPORT:
    pni_handle_transport(reactor, event);
    break;
  case PN_CONNECTION_LOCAL_OPEN:
    pni_handle_open(reactor, event);
    break;
  case PN_CONNECTION_FINAL:
    pni_handle_final(reactor, event);
    break;
  default:
    break;
  }
}

pn_record_t *pni_attachments(const pn_class_t *clazz, void *instance) {
  switch (pn_class_id(clazz)) {
  case CID_pn_connection:
    return pn_connection_attachments((pn_connection_t *) instance);
  case CID_pn_session:
    return pn_session_attachments((pn_session_t *) instance);
  case CID_pn_link:
    return pn_link_attachments((pn_link_t *) instance);
  default:
    return NULL;
  }
}

pn_handler_t *pn_event_handler(pn_event_t *event) {
  pn_record_t *record = pni_attachments(pn_event_class(event), pn_event_context(event));
  if (record) {
    return (pn_handler_t *) pn_record_get(record, PN_HANDLER);
  } else {
    return NULL;
  }
}

void pn_reactor_process(pn_reactor_t *reactor) {
  assert(reactor);
  pn_event_t *event;
  while ((event = pn_collector_peek(reactor->collector))) {
    pn_handler_t *handler = pn_event_handler(event);
    if (!handler) {
      handler = reactor->handler;
    }
    pn_handler_dispatch(handler, event);
    pni_reactor_dispatch(reactor, event);
    pn_collector_pop(reactor->collector);
  }
}

void pn_reactor_start(pn_reactor_t *reactor) {
  assert(reactor);
  pn_collector_put(reactor->collector, PN_OBJECT, reactor, PN_REACTOR_INIT);
}

bool pn_reactor_work(pn_reactor_t *reactor, int timeout) {
  assert(reactor);
  pn_reactor_process(reactor);

  if (!pn_selector_size(reactor->selector)) {
    return false;
  }

  pn_selector_select(reactor->selector, timeout);
  pn_selectable_t *sel;
  int events;
  while ((sel = pn_selector_next(reactor->selector, &events))) {
    if (events & PN_READABLE) {
      pn_selectable_readable(sel);
    }
    if (events & PN_WRITABLE) {
      pn_selectable_writable(sel);
    }
    if (events & PN_EXPIRED) {
      pn_selectable_expired(sel);
    }
    if (pn_selectable_is_terminal(sel)) {
      pn_selector_remove(reactor->selector, sel);
      pn_list_remove(reactor->children, sel);
    }
  }

  return true;
}

void pn_reactor_stop(pn_reactor_t *reactor) {
  assert(reactor);
  pn_collector_put(reactor->collector, PN_OBJECT, reactor, PN_REACTOR_FINAL);
  pn_reactor_process(reactor);
}

void pn_reactor_run(pn_reactor_t *reactor) {
  assert(reactor);
  pn_reactor_start(reactor);
  while (pn_reactor_work(reactor, 1000)) {}
  pn_reactor_stop(reactor);
}

