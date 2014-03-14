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

#include "../platform.h"
#include <proton/error.h>
#include <proton/object.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

typedef struct {
  pn_class_t *clazz;
  int refcount;
} pni_head_t;

#define pni_head(PTR) \
  (((pni_head_t *) (PTR)) - 1)

void *pn_new(size_t size, pn_class_t *clazz)
{
  pni_head_t *head = (pni_head_t *) malloc(sizeof(pni_head_t) + size);
  void *object = head + 1;
  pn_initialize(object, clazz);
  return object;
}

void pn_initialize(void *object, pn_class_t *clazz)
{
  pni_head_t *head = pni_head(object);
  head->clazz = clazz;
  head->refcount = 1;
  if (clazz && clazz->initialize) {
    clazz->initialize(object);
  }
}

void *pn_incref(void *object)
{
  if (object) {
    pni_head(object)->refcount++;
  }
  return object;
}

void pn_decref(void *object)
{
  if (object) {
    pni_head_t *head = pni_head(object);
    assert(head->refcount > 0);
    head->refcount--;
    if (!head->refcount) {
      pn_finalize(object);
      free(head);
    }
  }
}

void pn_finalize(void *object)
{
  if (object) {
    pni_head_t *head = pni_head(object);
    assert(head->refcount == 0);
    if (head->clazz && head->clazz->finalize) {
      head->clazz->finalize(object);
    }
    head->refcount = 0;
  }
}

int pn_refcount(void *object)
{
  assert(object);
  return pni_head(object)->refcount;
}

void pn_free(void *object)
{
  if (object) {
    assert(pn_refcount(object) == 1);
    pn_decref(object);
  }
}

pn_class_t *pn_class(void *object)
{
  assert(object);
  return pni_head(object)->clazz;
}

uintptr_t pn_hashcode(void *object)
{
  if (!object) return 0;

  pni_head_t *head = pni_head(object);
  if (head->clazz && head->clazz->hashcode) {
    return head->clazz->hashcode(object);
  } else {
    return (uintptr_t) head;
  }
}

intptr_t pn_compare(void *a, void *b)
{
  if (a == b) return 0;
  if (a && b) {
    pni_head_t *ha = pni_head(a);
    pni_head_t *hb = pni_head(b);

    if (ha->clazz && hb->clazz && ha->clazz == hb->clazz) {
      pn_class_t *clazz = ha->clazz;
      if (clazz->compare) {
        return clazz->compare(a, b);
      }
    }
  }

  return (intptr_t) b - (intptr_t) a;
}

bool pn_equals(void *a, void *b)
{
  return !pn_compare(a, b);
}

int pn_inspect(void *object, pn_string_t *dst)
{
  if (!pn_string_get(dst)) {
    pn_string_set(dst, "");
  }

  if (object) {
    pni_head_t *head = pni_head(object);
    if (head->clazz) {
      pn_class_t *clazz = head->clazz;
      if (clazz->inspect) {
        return clazz->inspect(object, dst);
      }
    }
    return pn_string_addf(dst, "object<%p>", object);
  } else {
    return pn_string_addf(dst, "(null)");
  }
}

struct pn_list_t {
  size_t capacity;
  size_t size;
  void **elements;
  int options;
};

size_t pn_list_size(pn_list_t *list)
{
  assert(list);
  return list->size;
}

void *pn_list_get(pn_list_t *list, int index)
{
  assert(list); assert(list->size);
  return list->elements[index % list->size];
}

void pn_list_set(pn_list_t *list, int index, void *value)
{
  assert(list); assert(list->size);
  void *old = list->elements[index % list->size];
  if (list->options & PN_REFCOUNT) pn_decref(old);
  list->elements[index % list->size] = value;
  if (list->options & PN_REFCOUNT) pn_incref(value);
}

void pn_list_ensure(pn_list_t *list, size_t capacity)
{
  assert(list);
  if (list->capacity < capacity) {
    size_t newcap = list->capacity;
    while (newcap < capacity) { newcap *= 2; }
    list->elements = (void **) realloc(list->elements, newcap * sizeof(void *));
    assert(list->elements);
    list->capacity = newcap;
  }
}

int pn_list_add(pn_list_t *list, void *value)
{
  assert(list);
  pn_list_ensure(list, list->size + 1);
  list->elements[list->size++] = value;
  if (list->options & PN_REFCOUNT) pn_incref(value);
  return 0;
}

ssize_t pn_list_index(pn_list_t *list, void *value)
{
  for (size_t i = 0; i < list->size; i++) {
    if (pn_equals(list->elements[i], value)) {
      return i;
    }
  }

  return -1;
}

bool pn_list_remove(pn_list_t *list, void *value)
{
  assert(list);
  ssize_t idx = pn_list_index(list, value);
  if (idx < 0) {
    return false;
  } else {
    pn_list_del(list, idx, 1);
  }

  return true;
}

void pn_list_del(pn_list_t *list, int index, int n)
{
  assert(list);
  index %= list->size;

  if (list->options & PN_REFCOUNT) {
    for (int i = 0; i < n; i++) {
      pn_decref(list->elements[index + i]);
    }
  }

  size_t slide = list->size - (index + n);
  for (size_t i = 0; i < slide; i++) {
    list->elements[index + i] = list->elements[index + n + i];
  }

  list->size -= n;
}

void pn_list_clear(pn_list_t *list)
{
  assert(list);
  pn_list_del(list, 0, list->size);
}

void pn_list_fill(pn_list_t *list, void *value, int n)
{
  for (int i = 0; i < n; i++) {
    pn_list_add(list, value);
  }
}

typedef struct {
  pn_list_t *list;
  size_t index;
} pni_list_iter_t;

static void *pni_list_next(void *ctx)
{
  pni_list_iter_t *iter = (pni_list_iter_t *) ctx;
  if (iter->index < pn_list_size(iter->list)) {
    return pn_list_get(iter->list, iter->index++);
  } else {
    return NULL;
  }
}

void pn_list_iterator(pn_list_t *list, pn_iterator_t *iter)
{
  pni_list_iter_t *liter = (pni_list_iter_t *) pn_iterator_start(iter, pni_list_next, sizeof(pni_list_iter_t));
  liter->list = list;
  liter->index = 0;
}

static void pn_list_finalize(void *object)
{
  assert(object);
  pn_list_t *list = (pn_list_t *) object;
  for (size_t i = 0; i < list->size; i++) {
    if (list->options & PN_REFCOUNT) pn_decref(pn_list_get(list, i));
  }
  free(list->elements);
}

static uintptr_t pn_list_hashcode(void *object)
{
  assert(object);
  pn_list_t *list = (pn_list_t *) object;
  uintptr_t hash = 1;

  for (size_t i = 0; i < list->size; i++) {
    hash = hash * 31 + pn_hashcode(pn_list_get(list, i));
  }

  return hash;
}

static intptr_t pn_list_compare(void *oa, void *ob)
{
  assert(oa); assert(ob);
  pn_list_t *a = (pn_list_t *) oa;
  pn_list_t *b = (pn_list_t *) ob;

  size_t na = pn_list_size(a);
  size_t nb = pn_list_size(b);
  if (na != nb) {
    return nb - na;
  } else {
    for (size_t i = 0; i < na; i++) {
      intptr_t delta = pn_compare(pn_list_get(a, i), pn_list_get(b, i));
      if (delta) return delta;
    }
  }

  return 0;
}

static int pn_list_inspect(void *obj, pn_string_t *dst)
{
  assert(obj);
  pn_list_t *list = (pn_list_t *) obj;
  int err = pn_string_addf(dst, "[");
  if (err) return err;
  size_t n = pn_list_size(list);
  for (size_t i = 0; i < n; i++) {
    if (i > 0) {
      err = pn_string_addf(dst, ", ");
      if (err) return err;
    }
    err = pn_inspect(pn_list_get(list, i), dst);
    if (err) return err;
  }
  return pn_string_addf(dst, "]");
}

#define pn_list_initialize NULL

pn_list_t *pn_list(size_t capacity, int options)
{
  static pn_class_t clazz = PN_CLASS(pn_list);

  pn_list_t *list = (pn_list_t *) pn_new(sizeof(pn_list_t), &clazz);
  list->capacity = capacity ? capacity : 16;
  list->elements = (void **) malloc(list->capacity * sizeof(void *));
  list->size = 0;
  list->options = options;
  return list;
}

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
  pni_entry_t *entries;
  size_t capacity;
  size_t addressable;
  size_t size;
  float load_factor;
  uintptr_t (*hashcode)(void *key);
  bool (*equals)(void *a, void *b);
  bool count_keys;
  bool count_values;
};

static void pn_map_finalize(void *object)
{
  pn_map_t *map = (pn_map_t *) object;

  if (map->count_keys || map->count_values) {
    for (size_t i = 0; i < map->capacity; i++) {
      if (map->entries[i].state != PNI_ENTRY_FREE) {
        if (map->count_keys) pn_decref(map->entries[i].key);
        if (map->count_values) pn_decref(map->entries[i].value);
      }
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
    err = pn_inspect(pn_map_key(map, entry), dst);
    if (err) return err;
    err = pn_string_addf(dst, ": ");
    if (err) return err;
    err = pn_inspect(pn_map_value(map, entry), dst);
    if (err) return err;
    entry = pn_map_next(map, entry);
  }
  return pn_string_addf(dst, "}");
}

#define pn_map_initialize NULL
#define pn_map_compare NULL

pn_map_t *pn_map(size_t capacity, float load_factor, int options)
{
  static pn_class_t clazz = PN_CLASS(pn_map);

  pn_map_t *map = (pn_map_t *) pn_new(sizeof(pn_map_t), &clazz);
  map->capacity = capacity ? capacity : 16;
  map->addressable = (size_t) (map->capacity * 0.86);
  if (!map->addressable) map->addressable = map->capacity;
  map->load_factor = load_factor;
  map->hashcode = pn_hashcode;
  map->equals = pn_equals;
  map->count_keys = (options & PN_REFCOUNT) || (options & PN_REFCOUNT_KEY);
  map->count_values = (options & PN_REFCOUNT) || (options & PN_REFCOUNT_VALUE);
  pni_map_allocate(map);
  return map;
}

size_t pn_map_size(pn_map_t *map)
{
  assert(map);
  return map->size;
}

static bool pni_map_ensure(pn_map_t *map, size_t capacity)
{
  float load = map->size / map->addressable;
  if (capacity <= map->capacity && load < map->load_factor) {
    return false;
  }

  size_t oldcap = map->capacity;

  while (map->capacity < capacity ||
         (map->size / map->addressable) > map->load_factor) {
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
      if (map->count_keys) pn_decref(key);
      if (map->count_values) pn_decref(value);
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
      if (map->count_keys) pn_incref(key);
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
    if (map->count_keys) pn_incref(key);
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
  if (map->count_values) pn_decref(entry->value);
  entry->value = value;
  if (map->count_values) pn_incref(value);
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
    if (prev) {
      prev->next = entry->next;
      prev->state = entry->state;
    }
    entry->state = PNI_ENTRY_FREE;
    entry->next = 0;
    if (map->count_keys) pn_decref(entry->key);
    entry->key = NULL;
    if (map->count_values) pn_decref(entry->value);
    entry->value = NULL;
    map->size--;
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

pn_hash_t *pn_hash(size_t capacity, float load_factor, int options)
{
  pn_hash_t *hash = (pn_hash_t *) pn_map(capacity, load_factor, 0);
  hash->map.hashcode = pni_identity_hashcode;
  hash->map.equals = pni_identity_equals;
  hash->map.count_keys = false;
  hash->map.count_values = options & PN_REFCOUNT;
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


#define PNI_NULL_SIZE (-1)

struct pn_string_t {
  char *bytes;
  ssize_t size;       // PNI_NULL_SIZE (-1) means null
  size_t capacity;
};

static void pn_string_finalize(void *object)
{
  pn_string_t *string = (pn_string_t *) object;
  free(string->bytes);
}

static uintptr_t pn_string_hashcode(void *object)
{
  pn_string_t *string = (pn_string_t *) object;
  if (string->size == PNI_NULL_SIZE) {
    return 0;
  }

  uintptr_t hashcode = 1;
  for (ssize_t i = 0; i < string->size; i++) {
    hashcode = hashcode * 31 + string->bytes[i];
  }
  return hashcode;
}

static intptr_t pn_string_compare(void *oa, void *ob)
{
  pn_string_t *a = (pn_string_t *) oa;
  pn_string_t *b = (pn_string_t *) ob;
  if (a->size != b->size) {
    return b->size - a->size;
  }

  if (a->size == PNI_NULL_SIZE) {
    return 0;
  } else {
    return memcmp(a->bytes, b->bytes, a->size);
  }
}

static int pn_string_inspect(void *obj, pn_string_t *dst)
{
  pn_string_t *str = (pn_string_t *) obj;
  if (str->size == PNI_NULL_SIZE) {
    return pn_string_addf(dst, "null");
  }

  int err = pn_string_addf(dst, "\"");

  for (int i = 0; i < str->size; i++) {
    uint8_t c = str->bytes[i];
    if (isprint(c)) {
      err = pn_string_addf(dst, "%c", c);
      if (err) return err;
    } else {
      err = pn_string_addf(dst, "\\x%.2x", c);
      if (err) return err;
    }
  }

  return pn_string_addf(dst, "\"");
}

pn_string_t *pn_string(const char *bytes)
{
  return pn_stringn(bytes, bytes ? strlen(bytes) : 0);
}

#define pn_string_initialize NULL


pn_string_t *pn_stringn(const char *bytes, size_t n)
{
  static pn_class_t clazz = PN_CLASS(pn_string);
  pn_string_t *string = (pn_string_t *) pn_new(sizeof(pn_string_t), &clazz);
  string->capacity = n ? n * sizeof(char) : 16;
  string->bytes = (char *) malloc(string->capacity);
  pn_string_setn(string, bytes, n);
  return string;
}

const char *pn_string_get(pn_string_t *string)
{
  assert(string);
  if (string->size == PNI_NULL_SIZE) {
    return NULL;
  } else {
    return string->bytes;
  }
}

size_t pn_string_size(pn_string_t *string)
{
  assert(string);
  if (string->size == PNI_NULL_SIZE) {
    return 0;
  } else {
    return string->size;
  }
}

int pn_string_set(pn_string_t *string, const char *bytes)
{
  return pn_string_setn(string, bytes, bytes ? strlen(bytes) : 0);
}

int pn_string_grow(pn_string_t *string, size_t capacity)
{
  bool grow = false;
  while (string->capacity < (capacity*sizeof(char) + 1)) {
    string->capacity *= 2;
    grow = true;
  }

  if (grow) {
    char *growed = (char *) realloc(string->bytes, string->capacity);
    if (growed) {
      string->bytes = growed;
    } else {
      return PN_ERR;
    }
  }

  return 0;
}

int pn_string_setn(pn_string_t *string, const char *bytes, size_t n)
{
  int err = pn_string_grow(string, n);
  if (err) return err;

  if (bytes) {
    memcpy(string->bytes, bytes, n*sizeof(char));
    string->bytes[n] = '\0';
    string->size = n;
  } else {
    string->size = PNI_NULL_SIZE;
  }

  return 0;
}

ssize_t pn_string_put(pn_string_t *string, char *dst)
{
  assert(string);
  assert(dst);

  if (string->size != PNI_NULL_SIZE) {
    memcpy(dst, string->bytes, string->size + 1);
  }

  return string->size;
}

void pn_string_clear(pn_string_t *string)
{
  pn_string_set(string, NULL);
}

int pn_string_format(pn_string_t *string, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  int err = pn_string_vformat(string, format, ap);
  va_end(ap);
  return err;
}

int pn_string_vformat(pn_string_t *string, const char *format, va_list ap)
{
  pn_string_set(string, "");
  return pn_string_vaddf(string, format, ap);
}

int pn_string_addf(pn_string_t *string, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  int err = pn_string_vaddf(string, format, ap);
  va_end(ap);
  return err;
}

int pn_string_vaddf(pn_string_t *string, const char *format, va_list ap)
{
  va_list copy;

  if (string->size == PNI_NULL_SIZE) {
    return PN_ERR;
  }

  while (true) {
    va_copy(copy, ap);
    int err = vsnprintf(string->bytes + string->size, string->capacity - string->size, format, copy);
    va_end(copy);
    if (err < 0) {
      return err;
    } else if ((size_t) err >= string->capacity - string->size) {
      pn_string_grow(string, string->size + err);
    } else {
      string->size += err;
      return 0;
    }
  }
}

char *pn_string_buffer(pn_string_t *string)
{
  assert(string);
  return string->bytes;
}

size_t pn_string_capacity(pn_string_t *string)
{
  assert(string);
  return string->capacity - 1;
}

int pn_string_resize(pn_string_t *string, size_t size)
{
  assert(string);
  int err = pn_string_grow(string, size);
  if (err) return err;
  string->size = size;
  string->bytes[size] = '\0';
  return 0;
}

int pn_string_copy(pn_string_t *string, pn_string_t *src)
{
  assert(string);
  return pn_string_setn(string, pn_string_get(src), pn_string_size(src));
}

struct pn_iterator_t {
  pn_iterator_next_t next;
  size_t size;
  void *state;
};

static void pn_iterator_initialize(void *object)
{
  pn_iterator_t *it = (pn_iterator_t *) object;
  it->next = NULL;
  it->size = 0;
  it->state = NULL;
}

static void pn_iterator_finalize(void *object)
{
  pn_iterator_t *it = (pn_iterator_t *) object;
  free(it->state);
}

#define pn_iterator_hashcode NULL
#define pn_iterator_compare NULL
#define pn_iterator_inspect NULL

pn_iterator_t *pn_iterator()
{
  static pn_class_t clazz = PN_CLASS(pn_iterator);
  pn_iterator_t *it = (pn_iterator_t *) pn_new(sizeof(pn_iterator_t), &clazz);
  return it;
}

void  *pn_iterator_start(pn_iterator_t *iterator, pn_iterator_next_t next,
                         size_t size) {
  assert(iterator);
  assert(next);
  iterator->next = next;
  if (iterator->size < size) {
    iterator->state = realloc(iterator->state, size);
  }
  return iterator->state;
}

void *pn_iterator_next(pn_iterator_t *iterator) {
  assert(iterator);
  if (iterator->next) {
    void *result = iterator->next(iterator->state);
    if (!result) iterator->next = NULL;
    return result;
  } else {
    return NULL;
  }
}
