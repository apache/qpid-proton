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
#include <stdlib.h>
#include <assert.h>

#define PNI_ENTRY_FREE (0)
#define PNI_ENTRY_LINK (1)
#define PNI_ENTRY_TAIL (2)

typedef struct {
  void *key;
  void *value;
  size_t next;
  uint8_t state;
} pni_entry_t;

struct pn_map_t {
  const pn_class_t *key;
  const pn_class_t *value;
  pni_entry_t *entries;
  size_t capacity;
  size_t addressable;
  size_t size;
  uintptr_t (*hashcode)(void *key);
  bool (*equals)(void *a, void *b);
  float load_factor;
};

static void pn_map_finalize(void *object)
{
  pn_map_t *map = (pn_map_t *) object;

  for (size_t i = 0; i < map->capacity; i++) {
    if (map->entries[i].state != PNI_ENTRY_FREE) {
      pn_class_decref(map->key, map->entries[i].key);
      pn_class_decref(map->value, map->entries[i].value);
    }
  }

  free(map->entries);
}

static uintptr_t pn_map_hashcode(void *object)
{
  pn_map_t *map = (pn_map_t *) object;

  uintptr_t hashcode = 0;

  for (size_t i = 0; i < map->capacity; i++) {
    if (map->entries[i].state != PNI_ENTRY_FREE) {
      void *key = map->entries[i].key;
      void *value = map->entries[i].value;
      hashcode += pn_hashcode(key) ^ pn_hashcode(value);
    }
  }

  return hashcode;
}

static void pni_map_allocate(pn_map_t *map)
{
  map->entries = (pni_entry_t *) malloc(map->capacity * sizeof (pni_entry_t));
  for (size_t i = 0; i < map->capacity; i++) {
    map->entries[i].key = NULL;
    map->entries[i].value = NULL;
    map->entries[i].next = 0;
    map->entries[i].state = PNI_ENTRY_FREE;
  }
  map->size = 0;
}

static int pn_map_inspect(void *obj, pn_string_t *dst)
{
  assert(obj);
  pn_map_t *map = (pn_map_t *) obj;
  int err = pn_string_addf(dst, "{");
  if (err) return err;
  pn_handle_t entry = pn_map_head(map);
  bool first = true;
  while (entry) {
    if (first) {
      first = false;
    } else {
      err = pn_string_addf(dst, ", ");
      if (err) return err;
    }
    err = pn_class_inspect(map->key, pn_map_key(map, entry), dst);
    if (err) return err;
    err = pn_string_addf(dst, ": ");
    if (err) return err;
    err = pn_class_inspect(map->value, pn_map_value(map, entry), dst);
    if (err) return err;
    entry = pn_map_next(map, entry);
  }
  return pn_string_addf(dst, "}");
}

#define pn_map_initialize NULL
#define pn_map_compare NULL

pn_map_t *pn_map(const pn_class_t *key, const pn_class_t *value,
                 size_t capacity, float load_factor)
{
  static const pn_class_t clazz = PN_CLASS(pn_map);

  pn_map_t *map = (pn_map_t *) pn_class_new(&clazz, sizeof(pn_map_t));
  map->key = key;
  map->value = value;
  map->capacity = capacity ? capacity : 16;
  map->addressable = (size_t) (map->capacity * 0.86);
  if (!map->addressable) map->addressable = map->capacity;
  map->load_factor = load_factor;
  map->hashcode = pn_hashcode;
  map->equals = pn_equals;
  pni_map_allocate(map);
  return map;
}

size_t pn_map_size(pn_map_t *map)
{
  assert(map);
  return map->size;
}

static float pni_map_load(pn_map_t *map)
{
  return ((float) map->size) / ((float) map->addressable);
}

static bool pni_map_ensure(pn_map_t *map, size_t capacity)
{
  float load = pni_map_load(map);
  if (capacity <= map->capacity && load <= map->load_factor) {
    return false;
  }

  size_t oldcap = map->capacity;

  while (map->capacity < capacity || pni_map_load(map) > map->load_factor) {
    map->capacity *= 2;
    map->addressable = (size_t) (0.86 * map->capacity);
  }

  pni_entry_t *entries = map->entries;
  pni_map_allocate(map);

  for (size_t i = 0; i < oldcap; i++) {
    if (entries[i].state != PNI_ENTRY_FREE) {
      void *key = entries[i].key;
      void *value = entries[i].value;
      pn_map_put(map, key, value);
      pn_class_decref(map->key, key);
      pn_class_decref(map->value, value);
    }
  }

  free(entries);
  return true;
}

static pni_entry_t *pni_map_entry(pn_map_t *map, void *key, pni_entry_t **pprev, bool create)
{
  uintptr_t hashcode = map->hashcode(key);

  pni_entry_t *entry = &map->entries[hashcode % map->addressable];
  pni_entry_t *prev = NULL;

  if (entry->state == PNI_ENTRY_FREE) {
    if (create) {
      entry->state = PNI_ENTRY_TAIL;
      entry->key = key;
      pn_class_incref(map->key, key);
      map->size++;
      return entry;
    } else {
      return NULL;
    }
  }

  while (true) {
    if (map->equals(entry->key, key)) {
      if (pprev) *pprev = prev;
      return entry;
    }

    if (entry->state == PNI_ENTRY_TAIL) {
      break;
    } else {
      prev = entry;
      entry = &map->entries[entry->next];
    }
  }

  if (create) {
    if (pni_map_ensure(map, map->size + 1)) {
      // if we had to grow the table we need to start over
      return pni_map_entry(map, key, pprev, create);
    }

    size_t empty = 0;
    for (size_t i = 0; i < map->capacity; i++) {
      size_t idx = map->capacity - i - 1;
      if (map->entries[idx].state == PNI_ENTRY_FREE) {
        empty = idx;
        break;
      }
    }
    entry->next = empty;
    entry->state = PNI_ENTRY_LINK;
    map->entries[empty].state = PNI_ENTRY_TAIL;
    map->entries[empty].key = key;
    pn_class_incref(map->key, key);
    if (pprev) *pprev = entry;
    map->size++;
    return &map->entries[empty];
  } else {
    return NULL;
  }
}

int pn_map_put(pn_map_t *map, void *key, void *value)
{
  assert(map);
  pni_entry_t *entry = pni_map_entry(map, key, NULL, true);
  pn_class_decref(map->value, entry->value);
  entry->value = value;
  pn_class_incref(map->value, value);
  return 0;
}

void *pn_map_get(pn_map_t *map, void *key)
{
  assert(map);
  pni_entry_t *entry = pni_map_entry(map, key, NULL, false);
  return entry ? entry->value : NULL;
}

void pn_map_del(pn_map_t *map, void *key)
{
  assert(map);
  pni_entry_t *prev = NULL;
  pni_entry_t *entry = pni_map_entry(map, key, &prev, false);
  if (entry) {
    void *dref_key = entry->key;
    void *dref_value = entry->value;
    if (prev) {
      prev->next = entry->next;
      prev->state = entry->state;
    } else if (entry->next) {
      assert(entry->state == PNI_ENTRY_LINK);
      pni_entry_t *next = &map->entries[entry->next];
      *entry = *next;
      entry = next;
    }
    entry->state = PNI_ENTRY_FREE;
    entry->next = 0;
    entry->key = NULL;
    entry->value = NULL;
    map->size--;
    pn_class_decref(map->key, dref_key);
    pn_class_decref(map->value, dref_value);
  }
}

pn_handle_t pn_map_head(pn_map_t *map)
{
  assert(map);
  for (size_t i = 0; i < map->capacity; i++)
  {
    if (map->entries[i].state != PNI_ENTRY_FREE) {
      return i + 1;
    }
  }

  return 0;
}

pn_handle_t pn_map_next(pn_map_t *map, pn_handle_t entry)
{
  for (size_t i = entry; i < map->capacity; i++) {
    if (map->entries[i].state != PNI_ENTRY_FREE) {
      return i + 1;
    }
  }

  return 0;
}

void *pn_map_key(pn_map_t *map, pn_handle_t entry)
{
  assert(map);
  assert(entry);
  return map->entries[entry - 1].key;
}

void *pn_map_value(pn_map_t *map, pn_handle_t entry)
{
  assert(map);
  assert(entry);
  return map->entries[entry - 1].value;
}

struct pn_hash_t {
  pn_map_t map;
};

static uintptr_t pni_identity_hashcode(void *obj)
{
  return (uintptr_t ) obj;
}

static bool pni_identity_equals(void *a, void *b)
{
  return a == b;
}

extern const pn_class_t *PN_UINTPTR;

#define CID_pni_uintptr CID_pn_void
static const pn_class_t *pni_uintptr_reify(void *object) { return PN_UINTPTR; }
#define pni_uintptr_new NULL
#define pni_uintptr_free NULL
#define pni_uintptr_initialize NULL
static void pni_uintptr_incref(void *object) {}
static void pni_uintptr_decref(void *object) {}
static int pni_uintptr_refcount(void *object) { return -1; }
#define pni_uintptr_finalize NULL
#define pni_uintptr_hashcode NULL
#define pni_uintptr_compare NULL
#define pni_uintptr_inspect NULL

const pn_class_t PNI_UINTPTR = PN_METACLASS(pni_uintptr);
const pn_class_t *PN_UINTPTR = &PNI_UINTPTR;

pn_hash_t *pn_hash(const pn_class_t *clazz, size_t capacity, float load_factor)
{
  pn_hash_t *hash = (pn_hash_t *) pn_map(PN_UINTPTR, clazz, capacity, load_factor);
  hash->map.hashcode = pni_identity_hashcode;
  hash->map.equals = pni_identity_equals;
  return hash;
}

size_t pn_hash_size(pn_hash_t *hash)
{
  return pn_map_size(&hash->map);
}

int pn_hash_put(pn_hash_t *hash, uintptr_t key, void *value)
{
  return pn_map_put(&hash->map, (void *) key, value);
}

void *pn_hash_get(pn_hash_t *hash, uintptr_t key)
{
  return pn_map_get(&hash->map, (void *) key);
}

void pn_hash_del(pn_hash_t *hash, uintptr_t key)
{
  pn_map_del(&hash->map, (void *) key);
}

pn_handle_t pn_hash_head(pn_hash_t *hash)
{
  return pn_map_head(&hash->map);
}

pn_handle_t pn_hash_next(pn_hash_t *hash, pn_handle_t entry)
{
  return pn_map_next(&hash->map, entry);
}

uintptr_t pn_hash_key(pn_hash_t *hash, pn_handle_t entry)
{
  return (uintptr_t) pn_map_key(&hash->map, entry);
}

void *pn_hash_value(pn_hash_t *hash, pn_handle_t entry)
{
  return pn_map_value(&hash->map, entry);
}
