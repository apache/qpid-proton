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

struct pn_list_t {
  const pn_class_t *clazz;
  size_t capacity;
  size_t size;
  void **elements;
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
  pn_class_decref(list->clazz, old);
  list->elements[index % list->size] = value;
  pn_class_incref(list->clazz, value);
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
  pn_class_incref(list->clazz, value);
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

  for (int i = 0; i < n; i++) {
    pn_class_decref(list->clazz, list->elements[index + i]);
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
    pn_class_decref(list->clazz, pn_list_get(list, i));
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
    err = pn_class_inspect(list->clazz, pn_list_get(list, i), dst);
    if (err) return err;
  }
  return pn_string_addf(dst, "]");
}

#define pn_list_initialize NULL

pn_list_t *pn_list(const pn_class_t *clazz, size_t capacity)
{
  static const pn_class_t list_clazz = PN_CLASS(pn_list);

  pn_list_t *list = (pn_list_t *) pn_class_new(&list_clazz, sizeof(pn_list_t));
  list->clazz = clazz;
  list->capacity = capacity ? capacity : 16;
  list->elements = (void **) malloc(list->capacity * sizeof(void *));
  list->size = 0;
  return list;
}

