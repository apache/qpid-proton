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
  pn_event_t *prev;         /* event returned by previous call to pn_collector_next() */
  bool freed;
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
  collector->prev = NULL;
  collector->freed = false;
}

void pn_collector_drain(pn_collector_t *collector)
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

// Advance head pointer for pop or next, return the old head.
static pn_event_t *pop_internal(pn_collector_t *collector) {
  pn_event_t *event = collector->head;
  if (event) {
    collector->head = event->next;
    if (!collector->head) {
      collector->tail = NULL;
    }
  }
  return event;
}

bool pn_collector_pop(pn_collector_t *collector) {
  pn_event_t *event = pop_internal(collector);
  if (event) {
    pn_decref(event);
  }
  return event;
}

pn_event_t *pn_collector_next(pn_collector_t *collector) {
  if (collector->prev) {
    pn_decref(collector->prev);
  }
  collector->prev = pop_internal(collector);
  return collector->prev;
}

pn_event_t *pn_collector_prev(pn_collector_t *collector) {
  return collector->prev;
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
  return event ? event->type : PN_EVENT_NONE;
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
  #define CASE(X) case X: return #X
  switch (type) {
  CASE(PN_EVENT_NONE);
  CASE(PN_REACTOR_INIT);
  CASE(PN_REACTOR_QUIESCED);
  CASE(PN_REACTOR_FINAL);
  CASE(PN_TIMER_TASK);
  CASE(PN_CONNECTION_INIT);
  CASE(PN_CONNECTION_BOUND);
  CASE(PN_CONNECTION_UNBOUND);
  CASE(PN_CONNECTION_REMOTE_OPEN);
  CASE(PN_CONNECTION_LOCAL_OPEN);
  CASE(PN_CONNECTION_REMOTE_CLOSE);
  CASE(PN_CONNECTION_LOCAL_CLOSE);
  CASE(PN_CONNECTION_FINAL);
  CASE(PN_SESSION_INIT);
  CASE(PN_SESSION_REMOTE_OPEN);
  CASE(PN_SESSION_LOCAL_OPEN);
  CASE(PN_SESSION_REMOTE_CLOSE);
  CASE(PN_SESSION_LOCAL_CLOSE);
  CASE(PN_SESSION_FINAL);
  CASE(PN_LINK_INIT);
  CASE(PN_LINK_REMOTE_OPEN);
  CASE(PN_LINK_LOCAL_OPEN);
  CASE(PN_LINK_REMOTE_CLOSE);
  CASE(PN_LINK_LOCAL_DETACH);
  CASE(PN_LINK_REMOTE_DETACH);
  CASE(PN_LINK_LOCAL_CLOSE);
  CASE(PN_LINK_FLOW);
  CASE(PN_LINK_FINAL);
  CASE(PN_DELIVERY);
  CASE(PN_TRANSPORT);
  CASE(PN_TRANSPORT_AUTHENTICATED);
  CASE(PN_TRANSPORT_ERROR);
  CASE(PN_TRANSPORT_HEAD_CLOSED);
  CASE(PN_TRANSPORT_TAIL_CLOSED);
  CASE(PN_TRANSPORT_CLOSED);
  CASE(PN_SELECTABLE_INIT);
  CASE(PN_SELECTABLE_UPDATED);
  CASE(PN_SELECTABLE_READABLE);
  CASE(PN_SELECTABLE_WRITABLE);
  CASE(PN_SELECTABLE_ERROR);
  CASE(PN_SELECTABLE_EXPIRED);
  CASE(PN_SELECTABLE_FINAL);
  CASE(PN_CONNECTION_WAKE);
  CASE(PN_LISTENER_ACCEPT);
  CASE(PN_LISTENER_CLOSE);
  CASE(PN_PROACTOR_INTERRUPT);
  CASE(PN_PROACTOR_TIMEOUT);
  CASE(PN_PROACTOR_INACTIVE);
  CASE(PN_LISTENER_OPEN);
  CASE(PN_RAW_CONNECTION_CONNECTED);
  CASE(PN_RAW_CONNECTION_DISCONNECTED);
  CASE(PN_RAW_CONNECTION_CLOSED_READ);
  CASE(PN_RAW_CONNECTION_CLOSED_WRITE);
  CASE(PN_RAW_CONNECTION_NEED_READ_BUFFERS);
  CASE(PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
  CASE(PN_RAW_CONNECTION_READ);
  CASE(PN_RAW_CONNECTION_WRITTEN);
  CASE(PN_RAW_CONNECTION_WAKE);
  CASE(PN_RAW_CONNECTION_DRAIN_BUFFERS);
  default:
    return "PN_UNKNOWN";
  }
  return NULL;
#undef CASE
}
