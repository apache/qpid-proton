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
#include <proton/object.h>
#include <assert.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "store.h"

typedef struct pni_stream_t pni_stream_t;

struct pni_store_t {
  pni_stream_t *streams;
  pni_entry_t *store_head;
  pni_entry_t *store_tail;
  pn_hash_t *tracked;
  size_t size;
  int window;
  pn_sequence_t lwm;
  pn_sequence_t hwm;
};

struct pni_stream_t {
  pni_store_t *store;
  pn_string_t *address;
  pni_entry_t *stream_head;
  pni_entry_t *stream_tail;
  pni_stream_t *next;
};

struct pni_entry_t {
  pni_stream_t *stream;
  pni_entry_t *stream_next;
  pni_entry_t *stream_prev;
  pni_entry_t *store_next;
  pni_entry_t *store_prev;
  pn_buffer_t *bytes;
  pn_delivery_t *delivery;
  void *context;
  pn_status_t status;
  pn_sequence_t id;
  bool free;
};

void pni_entry_finalize(void *object)
{
  pni_entry_t *entry = (pni_entry_t *) object;
  assert(entry->free);
  pn_delivery_t *d = entry->delivery;
  if (d) {
    pn_delivery_settle(d);
    pni_entry_set_delivery(entry, NULL);
  }
}

pni_store_t *pni_store()
{
  pni_store_t *store = (pni_store_t *) malloc(sizeof(pni_store_t));
  if (!store) return NULL;

  store->size = 0;
  store->streams = NULL;
  store->store_head = NULL;
  store->store_tail = NULL;
  store->window = 0;
  store->lwm = 0;
  store->hwm = 0;
  store->tracked = pn_hash(PN_OBJECT, 0, 0.75);

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

  pni_stream_t *prev = NULL;
  pni_stream_t *stream = store->streams;
  while (stream) {
    if (!strcmp(pn_string_get(stream->address), address)) {
      return stream;
    }
    prev = stream;
    stream = stream->next;
  }

  if (create) {
    stream = (pni_stream_t *) malloc(sizeof(pni_stream_t));
    stream->store = store;
    stream->address = pn_string(address);
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
  pn_decref(entry);
  store->size--;
}

void pni_stream_free(pni_stream_t *stream)
{
  if (!stream) return;
  pni_entry_t *entry;
  while ((entry = LL_HEAD(stream, stream))) {
    pni_entry_free(entry);
  }
  pn_free(stream->address);
  stream->address = NULL;
  free(stream);
}

void pni_store_free(pni_store_t *store)
{
  if (!store) return;
  pn_free(store->tracked);
  pni_stream_t *stream = store->streams;
  while (stream) {
    pni_stream_t *next = stream->next;
    pni_stream_free(stream);
    stream = next;
  }
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

#define CID_pni_entry CID_pn_object
#define pni_entry_initialize NULL
#define pni_entry_hashcode NULL
#define pni_entry_compare NULL
#define pni_entry_inspect NULL

pni_entry_t *pni_store_put(pni_store_t *store, const char *address)
{
  assert(store);
  static const pn_class_t clazz = PN_CLASS(pni_entry);

  if (!address) address = "";
  pni_stream_t *stream = pni_stream_put(store, address);
  if (!stream) return NULL;
  pni_entry_t *entry = (pni_entry_t *) pn_class_new(&clazz, sizeof(pni_entry_t));
  if (!entry) return NULL;
  entry->stream = stream;
  entry->free = false;
  entry->stream_next = NULL;
  entry->stream_prev = NULL;
  entry->store_next = NULL;
  entry->store_prev = NULL;
  entry->delivery = NULL;
  entry->bytes = pn_buffer(64);
  entry->status = PN_STATUS_UNKNOWN;
  LL_ADD(stream, stream, entry);
  LL_ADD(store, store, entry);
  store->size++;
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

static pn_status_t disp2status(uint64_t disp)
{
  if (!disp) return PN_STATUS_PENDING;

  switch (disp) {
  case PN_RECEIVED:
    return PN_STATUS_PENDING;
  case PN_ACCEPTED:
    return PN_STATUS_ACCEPTED;
  case PN_REJECTED:
    return PN_STATUS_REJECTED;
  case PN_RELEASED:
    return PN_STATUS_RELEASED;
  case PN_MODIFIED:
    return PN_STATUS_MODIFIED;
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
    if (pn_delivery_remote_state(d)) {
      entry->status = disp2status(pn_delivery_remote_state(d));
    } else if (pn_delivery_settled(d)) {
      uint64_t disp = pn_delivery_local_state(d);
      if (disp) {
        entry->status = disp2status(disp);
      } else {
        entry->status = PN_STATUS_SETTLED;
      }
    } else {
      entry->status = PN_STATUS_PENDING;
    }
  }
}

pn_sequence_t pni_entry_id(pni_entry_t *entry)
{
  assert(entry);
  return entry->id;
}

pni_entry_t *pni_store_entry(pni_store_t *store, pn_sequence_t id)
{
  assert(store);
  return (pni_entry_t *) pn_hash_get(store->tracked, id);
}

bool pni_store_tracking(pni_store_t *store, pn_sequence_t id)
{
  return (id - store->lwm >= 0) && (store->hwm - id > 0);
}

pn_sequence_t pni_entry_track(pni_entry_t *entry)
{
  assert(entry);

  pni_store_t *store = entry->stream->store;
  entry->id = store->hwm++;
  pn_hash_put(store->tracked, entry->id, entry);

  if (store->window >= 0) {
    while (store->hwm - store->lwm > store->window) {
      pni_entry_t *e = pni_store_entry(store, store->lwm);
      if (e) {
        pn_hash_del(store->tracked, store->lwm);
      }
      store->lwm++;
    }
  }

  return entry->id;
}

int pni_store_update(pni_store_t *store, pn_sequence_t id, pn_status_t status,
                     int flags, bool settle, bool match)
{
  assert(store);

  if (!pni_store_tracking(store, id)) {
    return 0;
  }

  size_t start;
  if (PN_CUMULATIVE & flags) {
    start = store->lwm;
  } else {
    start = id;
  }

  for (pn_sequence_t i = start; i <= id; i++) {
    pni_entry_t *e = pni_store_entry(store, i);
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

          pni_entry_updated(e);
        }
      }
      if (settle) {
        if (d) {
          pn_delivery_settle(d);
        }
        pn_hash_del(store->tracked, e->id);
      }
    }
  }

  while (store->hwm - store->lwm > 0 &&
         !pn_hash_get(store->tracked, store->lwm)) {
    store->lwm++;
  }

  return 0;
}

int pni_store_get_window(pni_store_t *store)
{
  assert(store);
  return store->window;
}

void pni_store_set_window(pni_store_t *store, int window)
{
  assert(store);
  store->window = window;
}
