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
#include <string.h>
#include <stdlib.h>
#include "value-internal.h"

pn_map_t *pn_map(int capacity)
{
  pn_map_t *map = malloc(sizeof(pn_map_t) + 2*capacity*sizeof(pn_value_t));
  map->capacity = capacity;
  map->size = 0;
  return map;
}

void pn_free_map(pn_map_t *m)
{
  free(m);
}

pn_value_t pn_map_key(pn_map_t *map, int index)
{
  return map->pairs[2*index];
}

pn_value_t pn_map_value(pn_map_t *map, int index)
{
  return map->pairs[2*index+1];
}

void pn_visit_map(pn_map_t *m, void (*visitor)(pn_value_t))
{
  for (int i = 0; i < m->size; i++)
  {
    pn_visit(pn_map_key(m, i), visitor);
    pn_visit(pn_map_value(m, i), visitor);
  }
}

size_t pn_format_sizeof_map(pn_map_t *map)
{
  size_t result = 2;
  for (int i = 0; i < map->size; i++)
  {
    pn_value_t key = pn_map_key(map, i);
    pn_value_t value = pn_map_value(map, i);
    result += pn_format_sizeof(key) + 2 + pn_format_sizeof(value) + 2;
  }
  return result;
}

int pn_format_map(char **pos, char *limit, pn_map_t *map)
{
  bool first = true;
  int i, e;
  if ((e = pn_fmt(pos, limit, "{"))) return e;
  for (i = 0; i < map->size; i++)
  {
    pn_value_t key = pn_map_key(map, i);
    pn_value_t value = pn_map_value(map, i);
    if (first) first = false;
    else if ((e = pn_fmt(pos, limit, ", "))) return e;
    if ((e = pn_format_value(pos, limit, &key, 1))) return e;
    if ((e = pn_fmt(pos, limit, ": "))) return e;
    if ((e = pn_format_value(pos, limit, &value, 1))) return e;
  }
  if ((e = pn_fmt(pos, limit, "}"))) return e;
  return 0;
}

uintptr_t pn_hash_map(pn_map_t *map)
{
  uintptr_t hash = 0;
  int i;
  for (i = 0; i < map->size; i++)
  {
    hash += (pn_hash_value(pn_map_key(map, i)) ^
             pn_hash_value(pn_map_value(map, i)));
  }
  return hash;
}

static bool has_entry(pn_map_t *m, pn_value_t key, pn_value_t value)
{
  int i;
  for (i = 0; i < m->size; i++)
  {
    if (!pn_compare_value(pn_map_key(m, i), key) &&
        !pn_compare_value(pn_map_value(m, i), value))
      return true;
  }

  return false;
}

int pn_compare_map(pn_map_t *a, pn_map_t *b)
{
  int i;

  if (a->size != b->size)
    return b->size - a->size;

  for (i = 0; i < a->size; i++)
    if (!has_entry(b, pn_map_key(a, i), pn_map_value(a, i)))
      return -1;

  for (i = 0; i < b->size; i++)
    if (!has_entry(a, pn_map_key(b, i), pn_map_value(b, i)))
      return -1;

  return 0;
}

bool pn_map_has(pn_map_t *map, pn_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!pn_compare_value(key, pn_map_key(map, i)))
      return true;
  }

  return false;
}

pn_value_t pn_map_get(pn_map_t *map, pn_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!pn_compare_value(key, pn_map_key(map, i)))
      return pn_map_value(map, i);
  }

  return EMPTY_VALUE;
}

int pn_map_set(pn_map_t *map, pn_value_t key, pn_value_t value)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!pn_compare_value(key, pn_map_key(map, i)))
    {
      map->pairs[2*i + 1] = value;
      return 0;
    }
  }

  if (map->size < map->capacity)
  {
    map->pairs[2*map->size] = key;
    map->pairs[2*map->size+1] = value;
    map->size++;
    return 0;
  }
  else
  {
    return -1;
  }
}

pn_value_t pn_map_pop(pn_map_t *map, pn_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!pn_compare_value(key, pn_map_key(map, i)))
    {
      pn_value_t result = pn_map_value(map, i);
      memmove(&map->pairs[2*i], &map->pairs[2*(i+1)],
              (map->size - i - 1)*2*sizeof(pn_value_t));
      map->size--;
      return result;
    }
  }

  return EMPTY_VALUE;
}

int pn_map_size(pn_map_t *map)
{
  return map->size;
}

int pn_map_capacity(pn_map_t *map)
{
  return map->capacity;
}

size_t pn_encode_sizeof_map(pn_map_t *m)
{
  size_t result = 0;
  for (int i = 0; i < 2*m->size; i++)
  {
    result += pn_encode_sizeof(m->pairs[i]);
  }
  return result;
}

size_t pn_encode_map(pn_map_t *m, char *out)
{
  char *old = out;
  char *start;
  int count = 2*m->size;
  // XXX
  pn_write_start(&out, out + 1024, &start);
  for (int i = 0; i < count; i++)
  {
    out += pn_encode(m->pairs[i], out);
  }
  pn_write_map(&out, out + 1024, start, m->size);
  return out - old;
}
