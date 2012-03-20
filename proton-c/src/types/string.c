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

pn_string_t *pn_string(wchar_t *wcs)
{
  size_t size = wcslen(wcs);
  pn_string_t *str = malloc(sizeof(pn_string_t) + (size+1)*sizeof(wchar_t));
  str->size = size;
  wcscpy(str->wcs, wcs);
  return str;
}

void pn_free_string(pn_string_t *str)
{
  free(str);
}

size_t pn_string_size(pn_string_t *str)
{
  return str->size;
}

wchar_t *pn_string_wcs(pn_string_t *str)
{
  return str->wcs;
}

uintptr_t pn_hash_string(pn_string_t *s)
{
  wchar_t *c;
  uintptr_t hash = 1;
  for (c = s->wcs; *c; c++)
  {
    hash = 31*hash + *c;
  }
  return hash;
}

int pn_compare_string(pn_string_t *a, pn_string_t *b)
{
  if (a->size == b->size)
    return wmemcmp(a->wcs, b->wcs, a->size);
  else
    return b->size - a->size;
}
