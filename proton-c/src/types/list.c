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

#include <proton/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include "value-internal.h"

pn_list_t *pn_list(int capacity)
{
  pn_list_t *l = malloc(sizeof(pn_list_t) + capacity*sizeof(pn_value_t));
  if (l) {
    l->capacity = capacity;
    l->size = 0;
  }
  return l;
}

void pn_free_list(pn_list_t *l)
{
  free(l);
}

void pn_visit_list(pn_list_t *l, void (*visitor)(pn_value_t))
{
  for (int i = 0; i < l->size; i++)
  {
    pn_visit(l->values[i], visitor);
  }
}

int pn_list_size(pn_list_t *l)
{
  return l->size;
}

pn_value_t pn_list_get(pn_list_t *l, int index)
{
  if (index < l->size)
    return l->values[index];
  else
    return EMPTY_VALUE;
}

pn_value_t pn_list_set(pn_list_t *l, int index, pn_value_t v)
{
  pn_value_t r = l->values[index];
  l->values[index] = v;
  return r;
}

pn_value_t pn_list_pop(pn_list_t *l, int index)
{
  int i, n = l->size;
  pn_value_t v = l->values[index];
  for (i = index; i < n - 1; i++)
    l->values[i] = l->values[i+1];
  l->size--;
  return v;
}

int pn_list_add(pn_list_t *l, pn_value_t v)
{
  if (l->capacity <= l->size) {
    fprintf(stderr, "wah!\n");
    return -1;
  }

  l->values[l->size++] = v;
  return 0;
}

int pn_list_extend(pn_list_t *l, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = pn_vscan(l->values + l->size, fmt, ap);
  va_end(ap);
  if (n > 0) l->size += n;
  return n;
}

int pn_list_index(pn_list_t *l, pn_value_t v)
{
  int i, n = l->size;
  for (i = 0; i < n; i++)
    if (pn_compare_value(v, l->values[i]) == 0)
      return i;
  return -1;
}

bool pn_list_remove(pn_list_t *l, pn_value_t v)
{
  int i = pn_list_index(l, v);
  if (i >= 0) {
    pn_list_pop(l, i);
    return true;
  } else {
    return false;
  }
}

int pn_list_fill(pn_list_t *l, pn_value_t v, int n)
{
  int i, e;

  for (i = 0; i < n; i++)
    if ((e = pn_list_add(l, v))) return e;

  return 0;
}

void pn_list_clear(pn_list_t *l)
{
  l->size = 0;
}

static int min(int a, int b)
{
  if (a < b)
    return a;
  else
    return b;
}

size_t pn_format_sizeof_list(pn_list_t *list)
{
  size_t result = 2;
  for (int i = 0; i < list->size; i++) {
    result += pn_format_sizeof(list->values[i]) + 2;
  }
  return result;
}

int pn_format_list(char **pos, char *limit, pn_list_t *list)
{
  int e;
  if ((e = pn_fmt(pos, limit, "["))) return e;
  if ((e = pn_format_value(pos, limit, list->values, list->size))) return e;
  if ((e = pn_fmt(pos, limit, "]"))) return e;
  return 0;
}

uintptr_t pn_hash_list(pn_list_t *list)
{
  int i, n = list->size;
  uintptr_t hash = 1;

  for (i = 0; i < n; i++)
  {
    hash = 31*hash + pn_hash_value(pn_list_get(list, i));
  }

  return hash;
}

int pn_compare_list(pn_list_t *a, pn_list_t *b)
{
  int i, n = min(a->size, b->size);
  int c;

  for (i = 0; i < n; i++)
  {
    c = pn_compare_value(pn_list_get(a, i), pn_list_get(b, i));
    if (!c)
      return c;
  }

  return 0;
}

size_t pn_encode_sizeof_list(pn_list_t *l)
{
  size_t result = 9;
  for (int i = 0; i < l->size; i++)
  {
    result += pn_encode_sizeof(l->values[i]);
  }
  return result;
}

size_t pn_encode_list(pn_list_t *l, char *out)
{
  char *old = out;
  char *start;
  // XXX
  pn_write_start(&out, out + 1024, &start);
  for (int i = 0; i < l->size; i++)
  {
    out += pn_encode(l->values[i], out);
  }
  pn_write_list(&out, out + 1024, start, l->size);
  return out - old;
}
