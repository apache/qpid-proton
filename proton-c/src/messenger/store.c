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

#include <proton/messenger.h>
#include <proton/engine.h>
#include <assert.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "store.h"

typedef struct pni_stream_t pni_stream_t;

typedef struct {
  size_t capacity;
  int window;
  pn_sequence_t lwm;
  pn_sequence_t hwm;
  pni_entry_t **entries;
} pni_queue_t;

struct pni_store_t {
  size_t size;
  pni_queue_t queue;
  pni_stream_t *streams;
  pni_entry_t *store_head;
  pni_entry_t *store_tail;
};

struct pni_stream_t {
  pni_store_t *store;
  char address[1024]; // XXX
  pni_entry_t *stream_head;
  pni_entry_t *stream_tail;
  pni_stream_t *next;
};

struct pni_entry_t {
  int refcount;
  pn_sequence_t id;
  pni_stream_t *stream;
  bool free;
  pni_entry_t *stream_next;
  pni_entry_t *stream_prev;
  pni_entry_t *store_next;
  pni_entry_t *store_prev;
  pn_status_t status;
  pn_buffer_t *bytes;
  pn_delivery_t *delivery;
  void *context;
};

static void pni_entry_incref(pni_entry_t *entry)
{
  entry->refcount++;
}

void pni_entry_reclaim(pni_entry_t *entry)
{
  assert(entry->free);
  pn_delivery_t *d = entry->delivery;
  if (d) {
    if (!pn_delivery_local_state(d)) {
      pn_delivery_update(d, PN_ACCEPTED);
    }
    pn_delivery_settle(d);
    pni_entry_set_delivery(entry, NULL);
  }
  free(entry);
}

static void pni_entry_decref(pni_entry_t *entry)
{
  if (entry) {
    assert(entry->refcount > 0);
    entry->refcount--;
    if (entry->refcount == 0) {
      pni_entry_reclaim(entry);
    }
  }
}

void pni_queue_init(pni_queue_t *queue)
{
  queue->capacity = 1024;
  queue->window = 0;
  queue->lwm = 0;
  queue->hwm = 0;
  queue->entries = (pni_entry_t **) calloc(queue->capacity, sizeof(pni_entry_t *));
}

void pni_queue_tini(pni_queue_t *queue)
{
  for (int i = 0; i < queue->hwm - queue->lwm; i++) {
    pni_entry_decref(queue->entries[i]);
  }
  free(queue->entries);
}

bool pni_queue_contains(pni_queue_t *queue, pn_sequence_t id)
{
  return (id - queue->lwm >= 0) && (queue->hwm - id > 0);
}

pni_entry_t *pni_queue_get(pni_queue_t *queue, pn_sequence_t id)
{
  if (pni_queue_contains(queue, id)) {
    size_t offset = id - queue->lwm;
    assert(offset >= 0 && offset < queue->capacity);
    return queue->entries[offset];
  } else {
    return NULL;
  }
}

void pni_queue_gc(pni_queue_t *queue)
{
  size_t count = queue->hwm - queue->lwm;
  size_t delta = 0;

  while (delta < count && !queue->entries[delta]) {
    delta++;
  }

  memmove(queue->entries, queue->entries + delta, (count - delta)*sizeof(pni_entry_t *));
  queue->lwm += delta;
}

void pni_queue_del(pni_queue_t *queue, pni_entry_t *entry)
{
  pn_sequence_t id = entry->id;
  if (pni_queue_contains(queue, id)) {
    size_t offset = id - queue->lwm;
    assert(offset >= 0 && offset < queue->capacity);
    queue->entries[offset] = NULL;
    pni_entry_decref(entry);
  }
}

void pni_queue_slide(pni_queue_t *queue)
{
  if (queue->window >= 0) {
    while (queue->hwm - queue->lwm > queue->window) {
      pni_entry_t *e = pni_queue_get(queue, queue->lwm);
      if (e) {
        pni_queue_del(queue, e);
      } else {
        pni_queue_gc(queue);
      }
    }
  }
  pni_queue_gc(queue);
}

pn_sequence_t pni_queue_add(pni_queue_t *queue, pni_entry_t *entry)
{
  pn_sequence_t id = queue->hwm++;
  entry->id = id;
  size_t offset = id - queue->lwm;
  PN_ENSUREZ(queue->entries, queue->capacity, offset + 1, pni_entry_t *);
  assert(offset >= 0 && offset < queue->capacity);
  queue->entries[offset] = entry;
  pni_entry_incref(entry);
  pni_queue_slide(queue);
  return id;
}

int pni_queue_update(pni_queue_t *queue, pn_sequence_t id, pn_status_t status,
                    int flags, bool settle, bool match)
{
  if (!pni_queue_contains(queue, id)) {
    return 0;
  }

  size_t start;
  if (PN_CUMULATIVE & flags) {
    start = queue->lwm;
  } else {
    start = id;
  }

  for (pn_sequence_t i = start; i <= id; i++) {
    pni_entry_t *e = pni_queue_get(queue, i);
    if (e) {
      pn_delivery_t *d = e->delivery;
      if (d) {
        if (!pn_delivery_local_state(d)) {
          if (match) {
            pn_delivery_update(d, pn_delivery_remote_state(d));
          } else {
            switch (status) {
            case PN_STATUS_ACCEPTED:
              pn_delivery_update(d, PN_ACCEPTED);
              break;
            case PN_STATUS_REJECTED:
              pn_delivery_update(d, PN_REJECTED);
              break;
            default:
              break;
            }
          }
        }
      }
      if (settle) {
        if (d) {
          pn_delivery_settle(d);
        }
        pni_queue_del(queue, e);
      }
    }
  }

  pni_queue_gc(queue);

  return 0;
}

pni_store_t *pni_store()
{
  pni_store_t *store = (pni_store_t *) malloc(sizeof(pni_store_t));
  if (!store) return NULL;

  store->size = 0;
  store->streams = NULL;
  store->store_head = NULL;
  store->store_tail = NULL;
  pni_queue_init(&store->queue);

  return store;
}

size_t pni_store_size(pni_store_t *store)
{
  assert(store);
  return store->size;
}

pni_stream_t *pni_stream(pni_store_t *store, const char *address, bool create)
{
  assert(store);
  assert(address);
  // XXX
  if (strlen(address) >= 1024) return NULL;

  pni_stream_t *prev = NULL;
  pni_stream_t *stream = store->streams;
  while (stream) {
    if (!strcmp(stream->address, address)) {
      return stream;
    }
    prev = stream;
    stream = stream->next;
  }

  if (create) {
    stream = (pni_stream_t *) malloc(sizeof(pni_stream_t));
    stream->store = store;
    strcpy(stream->address, address);
    stream->stream_head = NULL;
    stream->stream_tail = NULL;
    stream->next = NULL;

    if (prev) {
      prev->next = stream;
    } else {
      store->streams = stream;
    }
  }

  return stream;
}

pni_stream_t *pni_stream_head(pni_store_t *store)
{
  assert(store);
  return store->streams;
}

pni_stream_t *pni_stream_next(pni_stream_t *stream)
{
  assert(stream);
  return stream->next;
}

void pni_entry_free(pni_entry_t *entry)
{
  if (!entry) return;
  pni_stream_t *stream = entry->stream;
  pni_store_t *store = stream->store;
  LL_REMOVE(stream, stream, entry);
  LL_REMOVE(store, store, entry);
  entry->free = true;

  pn_buffer_free(entry->bytes);
  entry->bytes = NULL;
  pni_entry_decref(entry);
  store->size--;
}

void pni_stream_free(pni_stream_t *stream)
{
  if (!stream) return;
  pni_entry_t *entry;
  while ((entry = LL_HEAD(stream, stream))) {
    pni_entry_free(entry);
  }
  free(stream);
}

void pni_store_free(pni_store_t *store)
{
  if (!store) return;
  pni_stream_t *stream = store->streams;
  while (stream) {
    pni_stream_t *next = stream->next;
    pni_stream_free(stream);
    stream = next;
  }
  pni_queue_tini(&store->queue);
  free(store);
}

pni_stream_t *pni_stream_put(pni_store_t *store, const char *address)
{
  assert(store); assert(address);
  return pni_stream(store, address, true);
}

pni_stream_t *pni_stream_get(pni_store_t *store, const char *address)
{
  assert(store); assert(address);
  return pni_stream(store, address, false);
}

pni_entry_t *pni_store_put(pni_store_t *store, const char *address)
{
  assert(store);
  if (!address) address = "";
  pni_stream_t *stream = pni_stream_put(store, address);
  if (!stream) return NULL;
  pni_entry_t *entry = (pni_entry_t *) malloc(sizeof(pni_entry_t));
  if (!entry) return NULL;
  entry->refcount = 0;
  entry->stream = stream;
  entry->free = false;
  entry->stream_next = NULL;
  entry->stream_prev = NULL;
  entry->store_next = NULL;
  entry->store_prev = NULL;
  entry->delivery = NULL;
  entry->bytes = pn_buffer(64);
  LL_ADD(stream, stream, entry);
  LL_ADD(store, store, entry);
  store->size++;

  pni_entry_incref(entry);
  pni_queue_add(&store->queue, entry);

  return entry;
}

pni_entry_t *pni_store_get(pni_store_t *store, const char *address)
{
  assert(store);
  if (address) {
    pni_stream_t *stream = pni_stream_get(store, address);
    if (!stream) return NULL;
    return LL_HEAD(stream, stream);
  } else {
    return LL_HEAD(store, store);
  }
}

pn_buffer_t *pni_entry_bytes(pni_entry_t *entry)
{
  assert(entry);
  return entry->bytes;
}

pn_status_t pni_entry_get_status(pni_entry_t *entry)
{
  assert(entry);
  return entry->status;
}

void pni_entry_set_status(pni_entry_t *entry, pn_status_t status)
{
  assert(entry);
  entry->status = status;
}

pn_delivery_t *pni_entry_get_delivery(pni_entry_t *entry)
{
  assert(entry);
  return entry->delivery;
}

void pni_entry_set_delivery(pni_entry_t *entry, pn_delivery_t *delivery)
{
  assert(entry);
  if (entry->delivery) {
    pn_delivery_set_context(entry->delivery, NULL);
  }
  entry->delivery = delivery;
  if (delivery) {
    pn_delivery_set_context(delivery, entry);
  }
  pni_entry_updated(entry);
}

void pni_entry_set_context(pni_entry_t *entry, void *context)
{
  assert(entry);
  entry->context = context;
}

void *pni_entry_get_context(pni_entry_t *entry)
{
  assert(entry);
  return entry->context;
}

static pn_status_t disp2status(pn_disposition_t disp)
{
  if (!disp) return PN_STATUS_UNKNOWN;

  switch (disp) {
  case PN_ACCEPTED:
    return PN_STATUS_ACCEPTED;
  case PN_REJECTED:
    return PN_STATUS_REJECTED;
  default:
    assert(0);
  }

  return (pn_status_t) 0;
}


void pni_entry_updated(pni_entry_t *entry)
{
  assert(entry);
  pn_delivery_t *d = entry->delivery;
  if (d) {
    if (pn_delivery_remote_state(d))
      entry->status = disp2status(pn_delivery_remote_state(d));
    else if (pn_delivery_settled(d))
      entry->status = disp2status(pn_delivery_local_state(d));
    else
      entry->status = PN_STATUS_PENDING;
  }
}

pn_sequence_t pni_entry_tracker(pni_entry_t *entry)
{
  assert(entry);
  return entry->id;
}

pni_entry_t *pni_store_track(pni_store_t *store, pn_sequence_t id)
{
  assert(store);
  return pni_queue_get(&store->queue, id);
}

int pni_store_update(pni_store_t *store, pn_sequence_t id, pn_status_t status,
                     int flags, bool settle, bool match)
{
  assert(store);
  return pni_queue_update(&store->queue, id, status, flags, settle, match);
}

int pni_store_get_window(pni_store_t *store)
{
  assert(store);
  return store->queue.window;
}

void pni_store_set_window(pni_store_t *store, int window)
{
  assert(store);
  store->queue.window = window;
}
