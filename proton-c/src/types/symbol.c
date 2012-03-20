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
#include <stdio.h>
#include "value-internal.h"

pn_symbol_t *pn_symbol(const char *name)
{
  return pn_symboln(name, name ? strlen(name) : 0);
}

pn_symbol_t *pn_symboln(const char *name, size_t size)
{
  if (name) {
    pn_symbol_t *sym = malloc(sizeof(pn_symbol_t) + size + 1);
    sym->size = size;
    strncpy(sym->name, name, size);
    sym->name[size] = '\0';
    return sym;
  } else {
    return NULL;
  }
}

void pn_free_symbol(pn_symbol_t *s)
{
  free(s);
}

size_t pn_symbol_size(pn_symbol_t *s)
{
  return s->size;
}

const char *pn_symbol_name(pn_symbol_t *s)
{
  return s->name;
}

uintptr_t pn_hash_symbol(pn_symbol_t *s)
{
  uintptr_t hash = 0;
  for (int i = 0; i < s->size; i++)
  {
    hash = 31*hash + s->name[i];
  }
  return hash;
}

int pn_compare_symbol(pn_symbol_t *a, pn_symbol_t *b)
{
  if (a->size == b->size)
    return strncmp(a->name, b->name, a->size);
  else
    return b->size - a->size;
}

pn_symbol_t *pn_symbol_dup(pn_symbol_t *s)
{
  return pn_symbol(s->name);
}

int pn_format_symbol(char **pos, char *limit, pn_symbol_t *sym)
{
  if (sym)
    return pn_fmt(pos, limit, "%s", sym->name);
  else
    return pn_fmt(pos, limit, "(null)");
}
