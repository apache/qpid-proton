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

#include "util.h"

#include "buffer.h"
#include "memory.h"

#include <proton/error.h>
#include <proton/types.h>
#include <proton/type_compat.h>

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

ssize_t pn_quote_data(char *dst, size_t capacity, const char *src, size_t size)
{
  int idx = 0;
  for (unsigned i = 0; i < size; i++)
  {
    uint8_t c = src[i];
    // output printable ASCII, ensure '\' always introduces hex escape
    if (c < 128 && c != '\\' && isprint(c)) {
      if (idx < (int) (capacity - 1)) {
        dst[idx++] = c;
      } else {
        if (idx > 0) {
          dst[idx - 1] = '\0';
        }
        return PN_OVERFLOW;
      }
    } else {
      if (idx < (int) (capacity - 4)) {
        idx += sprintf(dst + idx, "\\x%.2x", c);
      } else {
        if (idx > 0) {
          dst[idx - 1] = '\0';
        }
        return PN_OVERFLOW;
      }
    }
  }

  dst[idx] = '\0';
  return idx;
}

int pn_quote(pn_string_t *dst, const char *src, size_t size)
{
  while (true) {
    size_t str_size = pn_string_size(dst);
    char *str = pn_string_buffer(dst) + str_size;
    size_t capacity = pn_string_capacity(dst) - str_size;
    ssize_t ssize = pn_quote_data(str, capacity, src, size);
    if (ssize == PN_OVERFLOW) {
      int err = pn_string_grow(dst, (str_size + capacity) ? 2*(str_size + capacity) : 16);
      if (err) return err;
    } else if (ssize >= 0) {
      return pn_string_resize(dst, str_size + ssize);
    } else {
      return ssize;
    }
  }
}

int pn_strcasecmp(const char *a, const char *b)
{
  int diff;
  while (*b) {
    char aa = *a++, bb = *b++;
    diff = tolower(aa)-tolower(bb);
    if ( diff!=0 ) return diff;
  }
  return *a;
}

int pn_strncasecmp(const char* a, const char* b, size_t len)
{
  int diff = 0;
  while (*b && len > 0) {
    char aa = *a++, bb = *b++;
    diff = tolower(aa)-tolower(bb);
    if ( diff!=0 ) return diff;
    --len;
  };
  return len==0 ? diff : *a;
}

bool pn_env_bool(const char *name)
{
  char *v = getenv(name);
  return v && (!pn_strcasecmp(v, "true") || !pn_strcasecmp(v, "1") ||
               !pn_strcasecmp(v, "yes")  || !pn_strcasecmp(v, "on"));
}

PN_STRUCT_CLASSDEF(pn_strdup)

char *pn_strdup(const char *src)
{
  if (!src) return NULL;
  char *dest = (char *) pni_mem_allocate(PN_CLASSCLASS(pn_strdup), strlen(src)+1);
  if (!dest) return NULL;
  return strcpy(dest, src);
}

char *pn_strndup(const char *src, size_t n)
{
  if (src) {
    unsigned size = 0;
    for (const char *c = src; size < n && *c; c++) {
      size++;
    }

    char *dest = (char *) pni_mem_allocate(PN_CLASSCLASS(pn_strdup), size + 1);
    if (!dest) return NULL;
    strncpy(dest, src, pn_min(n, size));
    dest[size] = '\0';
    return dest;
  } else {
    return NULL;
  }
}

// which timestamp will expire next, or zero if none set
pn_timestamp_t pn_timestamp_min( pn_timestamp_t a, pn_timestamp_t b )
{
  if (a && b) return pn_min(a, b);
  if (a) return a;
  return b;
}

