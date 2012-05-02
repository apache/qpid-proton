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

pn_string_t *pn_string(const char *utf8)
{
  return pn_stringn(utf8, strlen(utf8));
}

pn_string_t *pn_stringn(const char *utf8, size_t size)
{
  pn_string_t *str = malloc(sizeof(pn_string_t) + (size+1)*sizeof(char));
  str->size = size;
  strncpy(str->utf8, utf8, size);
  str->utf8[size] = '\0';
  return str;
}

void pn_free_string(pn_string_t *str)
{
  free(str);
}

const char *pn_string_utf8(pn_string_t *str)
{
  return str->utf8;
}

uintptr_t pn_hash_string(pn_string_t *s)
{
  char *c;
  uintptr_t hash = 1;
  for (c = s->utf8; *c; c++)
  {
    hash = 31*hash + *c;
  }
  return hash;
}

int pn_compare_string(pn_string_t *a, pn_string_t *b)
{
  if (a->size == b->size)
    return memcmp(a->utf8, b->utf8, a->size);
  else
    return b->size - a->size;
}
