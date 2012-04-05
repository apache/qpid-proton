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

#include <proton/errors.h>
#include <proton/codec.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "value-internal.h"
#include "../util.h"

pn_binary_t *pn_binary(const char *bytes, size_t size)
{
  pn_binary_t *bin = malloc(sizeof(pn_binary_t) + size);
  bin->size = size;
  memmove(bin->bytes, bytes, size);
  return bin;
}

void pn_free_binary(pn_binary_t *b)
{
  free(b);
}

size_t pn_binary_size(pn_binary_t *b)
{
  return b->size;
}

const char *pn_binary_bytes(pn_binary_t *b)
{
  return b->bytes;
}

ssize_t pn_binary_get(pn_binary_t *b, char *bytes, size_t size)
{
  if (size < b->size) {
    return PN_OVERFLOW;
  } else {
    memmove(bytes, b->bytes, b->size);
    return b->size;
  }
}

uintptr_t pn_hash_binary(pn_binary_t *b)
{
  uintptr_t hash = 0;
  for (int i = 0; i < b->size; i++)
  {
    hash = 31*hash + b->bytes[i];
  }
  return hash;
}

int pn_compare_binary(pn_binary_t *a, pn_binary_t *b)
{
  if (a->size == b->size)
    return memcmp(a->bytes, b->bytes, a->size);
  else
    return b->size - a->size;
}

pn_binary_t *pn_binary_dup(pn_binary_t *b)
{
  if (b)
    return pn_binary(b->bytes, b->size);
  else
    return NULL;
}

int pn_format_binary(char **pos, char *limit, pn_binary_t *binary)
{
  if (!binary) return pn_fmt(pos, limit, "(null)");
  ssize_t n = pn_quote_data(*pos, limit - *pos, binary->bytes, binary->size);
  if (n < 0) {
    return n;
  } else {
    *pos += n;
    return 0;
  }
}
