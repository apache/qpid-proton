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
#include <stdio.h>
#include <proton/object.h>
#include <proton/event.h>
#include <proton/reactor.h>
#include <assert.h>

struct pn_collector_t {
  pn_list_t *pool;
  pn_event_t *head;
  pn_event_t *tail;
  bool freed;
  bool head_returned;         /* Head has been returned by pn_collector_next() */
};

struct pn_event_t {
  pn_list_t *pool;
  const pn_class_t *clazz;
  void *context;    // depends on clazz
  pn_record_t *attachments;
  pn_event_t *next;
  pn_event_type_t type;
};

static void pn_collector_initialize(pn_collector_t *collector)
{
  collector->pool = pn_list(PN_OBJECT, 0);
  collector->head = NULL;
  collector->tail = NULL;
  collector->freed = false;
}

static void pn_collector_drain(pn_collector_t *collector)
{
  assert(collector);
  while (pn_collector_next(collector))
    ;
  assert(!collector->head);
  assert(!collector->tail);
}

static void pn_collector_shrink(pn_collector_t *collector)
{
  assert(collector);
  pn_list_clear(collector->pool);
}

static void pn_collector_finalize(pn_collector_t *collector)
{
  pn_collector_drain(collector);
  pn_decref(collector->pool);
}

static int pn_collector_inspect(pn_collector_t *collector, pn_string_t *dst)
{
  assert(collector);
  int err = pn_string_addf(dst, "EVENTS[");
  if (err) return err;
  pn_event_t *event = collector->head;
  bool first = true;
  while (event) {
    if (first) {
      first = false;
    } else {
      err = pn_string_addf(dst, ", ");
      if (err) return err;
    }
    err = pn_inspect(event, dst);
    if (err) return err;
    event = event->next;
  }
  return pn_string_addf(dst, "]");
}

#define pn_collector_hashcode NULL
#define pn_collector_compare NULL

PN_CLASSDEF(pn_collector)

pn_collector_t *pn_collector(void)
{
  return pn_collector_new();
}

void pn_collector_free(pn_collector_t *collector)
{
  assert(collector);
  pn_collector_release(collector);
  pn_decref(collector);
}

void pn_collector_release(pn_collector_t *collector)
{
  assert(collector);
  if (!collector->freed) {
    collector->freed = true;
    pn_collector_drain(collector);
    pn_collector_shrink(collector);
  }
}

pn_event_t *pn_event(void);

pn_event_t *pn_collector_put(pn_collector_t *collector,
                             const pn_class_t *clazz, void *context,
                             pn_event_type_t type)
{
  if (!collector) {
    return NULL;
  }

  assert(context);

  if (collector->freed) {
    return NULL;
  }

  pn_event_t *tail = collector->tail;
  if (tail && tail->type == type && tail->context == context) {
    return NULL;
  }

  clazz = clazz->reify(context);

  pn_event_t *event = (pn_event_t *) pn_list_pop(collector->pool);

  if (!event) {
    event = pn_event();
  }

  event->pool = collector->pool;
  pn_incref(event->pool);

  if (tail) {
    tail->next = event;
    collector->tail = event;
  } else {
    collector->tail = event;
    collector->head = event;
  }

  event->clazz = clazz;
  event->context = context;
  event->type = type;
  pn_class_incref(clazz, event->context);

  return event;
}

pn_event_t *pn_collector_peek(pn_collector_t *collector)
{
  return collector->head;
}

bool pn_collector_pop(pn_collector_t *collector)
{
  collector->head_returned = false;
  pn_event_t *event = collector->head;
  if (event) {
    collector->head = event->next;
  } else {
    return false;
  }

  if (!collector->head) {
    collector->tail = NULL;
  }

  pn_decref(event);
  return true;
}

pn_event_t *pn_collector_next(pn_collector_t *collector)
{
  if (collector->head_returned) {
    pn_collector_pop(collector);
  }
  collector->head_returned = collector->head;
  return collector->head;
}

pn_event_t *pn_collector_prev(pn_collector_t *collector) {
  return collector->head_returned ? collector->head : NULL;
}

bool pn_collector_more(pn_collector_t *collector)
{
  assert(collector);
  return collector->head && collector->head->next;
}

static void pn_event_initialize(pn_event_t *event)
{
  event->pool = NULL;
  event->type = PN_EVENT_NONE;
  event->clazz = NULL;
  event->context = NULL;
  event->next = NULL;
  event->attachments = pn_record();
}

static void pn_event_finalize(pn_event_t *event) {
  // decref before adding to the free list
  if (event->clazz && event->context) {
    pn_class_decref(event->clazz, event->context);
  }

  pn_list_t *pool = event->pool;

  if (pool && pn_refcount(pool) > 1) {
    event->pool = NULL;
    event->type = PN_EVENT_NONE;
    event->clazz = NULL;
    event->context = NULL;
    event->next = NULL;
    pn_record_clear(event->attachments);
    pn_list_add(pool, event);
  } else {
    pn_decref(event->attachments);
  }

  pn_decref(pool);
}

static int pn_event_inspect(pn_event_t *event, pn_string_t *dst)
{
  assert(event);
  assert(dst);
  const char *name = pn_event_type_name(event->type);
  int err;
  if (name) {
    err = pn_string_addf(dst, "(%s", pn_event_type_name(event->type));
  } else {
    err = pn_string_addf(dst, "(<%u>", (unsigned int) event->type);
  }
  if (err) return err;
  if (event->context) {
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    err = pn_class_inspect(event->clazz, event->context, dst);
    if (err) return err;
  }

  return pn_string_addf(dst, ")");
}

#define pn_event_hashcode NULL
#define pn_event_compare NULL

PN_CLASSDEF(pn_event)

pn_event_t *pn_event(void)
{
  return pn_event_new();
}

pn_event_type_t pn_event_type(pn_event_t *event)
{
  return event->type;
}

const pn_class_t *pn_event_class(pn_event_t *event)
{
  assert(event);
  return event->clazz;
}

void *pn_event_context(pn_event_t *event)
{
  assert(event);
  return event->context;
}

pn_record_t *pn_event_attachments(pn_event_t *event)
{
  assert(event);
  return event->attachments;
}

const char *pn_event_type_name(pn_event_type_t type)
{
  switch (type) {
  case PN_EVENT_NONE:
    return "PN_EVENT_NONE";
  case PN_REACTOR_INIT:
    return "PN_REACTOR_INIT";
  case PN_REACTOR_QUIESCED:
    return "PN_REACTOR_QUIESCED";
  case PN_REACTOR_FINAL:
    return "PN_REACTOR_FINAL";
  case PN_TIMER_TASK:
    return "PN_TIMER_TASK";
  case PN_CONNECTION_INIT:
    return "PN_CONNECTION_INIT";
  case PN_CONNECTION_BOUND:
    return "PN_CONNECTION_BOUND";
  case PN_CONNECTION_UNBOUND:
    return "PN_CONNECTION_UNBOUND";
  case PN_CONNECTION_REMOTE_OPEN:
    return "PN_CONNECTION_REMOTE_OPEN";
  case PN_CONNECTION_LOCAL_OPEN:
    return "PN_CONNECTION_LOCAL_OPEN";
  case PN_CONNECTION_REMOTE_CLOSE:
    return "PN_CONNECTION_REMOTE_CLOSE";
  case PN_CONNECTION_LOCAL_CLOSE:
    return "PN_CONNECTION_LOCAL_CLOSE";
  case PN_CONNECTION_FINAL:
    return "PN_CONNECTION_FINAL";
  case PN_SESSION_INIT:
    return "PN_SESSION_INIT";
  case PN_SESSION_REMOTE_OPEN:
    return "PN_SESSION_REMOTE_OPEN";
  case PN_SESSION_LOCAL_OPEN:
    return "PN_SESSION_LOCAL_OPEN";
  case PN_SESSION_REMOTE_CLOSE:
    return "PN_SESSION_REMOTE_CLOSE";
  case PN_SESSION_LOCAL_CLOSE:
    return "PN_SESSION_LOCAL_CLOSE";
  case PN_SESSION_FINAL:
    return "PN_SESSION_FINAL";
  case PN_LINK_INIT:
    return "PN_LINK_INIT";
  case PN_LINK_REMOTE_OPEN:
    return "PN_LINK_REMOTE_OPEN";
  case PN_LINK_LOCAL_OPEN:
    return "PN_LINK_LOCAL_OPEN";
  case PN_LINK_REMOTE_CLOSE:
    return "PN_LINK_REMOTE_CLOSE";
  case PN_LINK_LOCAL_DETACH:
    return "PN_LINK_LOCAL_DETACH";
  case PN_LINK_REMOTE_DETACH:
    return "PN_LINK_REMOTE_DETACH";
  case PN_LINK_LOCAL_CLOSE:
    return "PN_LINK_LOCAL_CLOSE";
  case PN_LINK_FLOW:
    return "PN_LINK_FLOW";
  case PN_LINK_FINAL:
    return "PN_LINK_FINAL";
  case PN_DELIVERY:
    return "PN_DELIVERY";
  case PN_TRANSPORT:
    return "PN_TRANSPORT";
  case PN_TRANSPORT_AUTHENTICATED:
    return "PN_TRANSPORT_AUTHENTICATED";
  case PN_TRANSPORT_ERROR:
    return "PN_TRANSPORT_ERROR";
  case PN_TRANSPORT_HEAD_CLOSED:
    return "PN_TRANSPORT_HEAD_CLOSED";
  case PN_TRANSPORT_TAIL_CLOSED:
    return "PN_TRANSPORT_TAIL_CLOSED";
  case PN_TRANSPORT_CLOSED:
    return "PN_TRANSPORT_CLOSED";
  case PN_SELECTABLE_INIT:
    return "PN_SELECTABLE_INIT";
  case PN_SELECTABLE_UPDATED:
    return "PN_SELECTABLE_UPDATED";
  case PN_SELECTABLE_READABLE:
    return "PN_SELECTABLE_READABLE";
  case PN_SELECTABLE_WRITABLE:
    return "PN_SELECTABLE_WRITABLE";
  case PN_SELECTABLE_ERROR:
    return "PN_SELECTABLE_ERROR";
  case PN_SELECTABLE_EXPIRED:
    return "PN_SELECTABLE_EXPIRED";
  case PN_SELECTABLE_FINAL:
    return "PN_SELECTABLE_FINAL";
   case PN_CONNECTION_WAKE:
    return "PN_CONNECTION_WAKE";
   case PN_LISTENER_CLOSE:
    return "PN_LISTENER_CLOSE";
   case PN_PROACTOR_INTERRUPT:
    return "PN_PROACTOR_INTERRUPT";
   case PN_PROACTOR_TIMEOUT:
    return "PN_PROACTOR_TIMEOUT";
   case PN_PROACTOR_INACTIVE:
    return "PN_PROACTOR_INACTIVE";
   default:
    return "PN_UNKNOWN";
  }
  return NULL;
}

pn_event_t *pn_event_batch_next(pn_event_batch_t *batch) {
  return batch->next_event(batch);
}
