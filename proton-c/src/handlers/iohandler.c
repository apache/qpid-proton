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

#include "reactor/io.h"
#include "reactor/reactor.h"
#include "reactor/selector.h"

#include <proton/handlers.h>
#include <proton/transport.h>
#include <assert.h>

static const char pni_selector_handle = 0;

#define PN_SELECTOR ((pn_handle_t) &pni_selector_handle)

void pni_handle_quiesced(pn_reactor_t *reactor, pn_selector_t *selector) {
  // check if we are still quiesced, other handlers of
  // PN_REACTOR_QUIESCED could have produced more events to process
  if (!pn_reactor_quiesced(reactor)) { return; }
  pn_selector_select(selector, pn_reactor_get_timeout(reactor));
  pn_selectable_t *sel;
  int events;
  pn_reactor_mark(reactor);
  while ((sel = pn_selector_next(selector, &events))) {
    if (events & PN_READABLE) {
      pn_selectable_readable(sel);
    }
    if (events & PN_WRITABLE) {
      pn_selectable_writable(sel);
    }
    if (events & PN_EXPIRED) {
      pn_selectable_expired(sel);
    }
    if (events & PN_ERROR) {
      pn_selectable_error(sel);
    }
  }
  pn_reactor_yield(reactor);
}

void pni_handle_transport(pn_reactor_t *reactor, pn_event_t *event);
void pni_handle_open(pn_reactor_t *reactor, pn_event_t *event);
void pni_handle_bound(pn_reactor_t *reactor, pn_event_t *event);

static void pn_iodispatch(pn_iohandler_t *handler, pn_event_t *event, pn_event_type_t type) {
  pn_reactor_t *reactor = pn_event_reactor(event);
  pn_record_t *record = pn_reactor_attachments(reactor);
  pn_selector_t *selector = (pn_selector_t *) pn_record_get(record, PN_SELECTOR);
  if (!selector) {
    selector = pn_io_selector(pni_reactor_io(reactor));
    pn_record_def(record, PN_SELECTOR, PN_OBJECT);
    pn_record_set(record, PN_SELECTOR, selector);
    pn_decref(selector);
  }
  switch (type) {
  case PN_SELECTABLE_INIT:
    {
      pn_selectable_t *sel = (pn_selectable_t *) pn_event_context(event);
      pn_selector_add(selector, sel);
    }
    break;
  case PN_SELECTABLE_UPDATED:
    {
      pn_selectable_t *sel = (pn_selectable_t *) pn_event_context(event);
      pn_selector_update(selector, sel);
    }
    break;
  case PN_SELECTABLE_FINAL:
    {
      pn_selectable_t *sel = (pn_selectable_t *) pn_event_context(event);
      pn_selector_remove(selector, sel);
      pn_selectable_release(sel);
    }
    break;
  case PN_CONNECTION_LOCAL_OPEN:
    pni_handle_open(reactor, event);
    break;
  case PN_CONNECTION_BOUND:
    pni_handle_bound(reactor, event);
    break;
  case PN_TRANSPORT:
    pni_handle_transport(reactor, event);
    break;
  case PN_TRANSPORT_CLOSED:
    pn_transport_unbind(pn_event_transport(event));
    break;
  case PN_REACTOR_QUIESCED:
    pni_handle_quiesced(reactor, selector);
    break;
  default:
    break;
  }
}

pn_iohandler_t *pn_iohandler(void) {
  return pn_handler(pn_iodispatch);
}
