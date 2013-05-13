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

#include <proton/error.h>
#include <proton/object.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
  pn_class_t *clazz;
  int refcount;
} pni_head_t;

#define pni_head(PTR) \
  (((pni_head_t *) (PTR)) - 1)

void *pn_new(size_t size, pn_class_t *clazz)
{
  pni_head_t *obj = (pni_head_t *) malloc(sizeof(pni_head_t) + size);
  obj->clazz = clazz;
  obj->refcount = 1;
  return obj + 1;
}

void pn_convert(void *object, pn_class_t *clazz)
{
  pni_head_t *head = pni_head(object);
  head->clazz = clazz;
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
    head->refcount--;
    if (!head->refcount) {
      if (head->clazz && head->clazz->finalize) {
        head->clazz->finalize(object);
      }
      free(head);
    }
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

void pn_list_fill(pn_list_t *list, void *value, int n)
{
  for (int i = 0; i < n; i++) {
    pn_list_add(list, value);
  }
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

pn_list_t *pn_list(size_t capacity, int options)
{
  static pn_class_t clazz = {pn_list_finalize, pn_list_hashcode};

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

pn_map_t *pn_map(size_t capacity, float load_factor, int options)
{
  static pn_class_t clazz = {pn_map_finalize, pn_map_hashcode};

  pn_map_t *map = (pn_map_t *) pn_new(sizeof(pn_map_t), &clazz);
  map->capacity = capacity ? capacity : 16;
  map->addressable = capacity * 0.86;
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
    map->addressable = 0.86 * map->capacity;
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

pn_string_t *pn_string(const char *bytes)
{
  return pn_stringn(bytes, bytes ? strlen(bytes) : 0);
}

static pn_class_t clazz = {pn_string_finalize, pn_string_hashcode,
                           pn_string_compare};

pn_string_t *pn_stringn(const char *bytes, size_t n)
{

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

static int pn_string_grow(pn_string_t *string, size_t capacity)
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

  while (true) {
    va_start(ap, format);
    int err = vsnprintf(string->bytes, string->capacity, format, ap);
    va_end(ap);
    if (err < 0) {
      return err;
    } else if ((size_t) err >= string->capacity) {
      pn_string_grow(string, err);
    } else {
      string->size = err;
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
