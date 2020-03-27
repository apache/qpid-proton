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

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "buffer.h"
#include "memory.h"
#include "util.h"

struct pn_buffer_t {
  size_t capacity;
  size_t start;
  size_t size;
  char *bytes;
};

PN_STRUCT_CLASSDEF(pn_buffer)

pn_buffer_t *pn_buffer(size_t capacity)
{
  pn_buffer_t *buf = (pn_buffer_t *) pni_mem_allocate(PN_CLASSCLASS(pn_buffer), sizeof(pn_buffer_t));
  if (buf != NULL) {
    buf->capacity = capacity;
    buf->start = 0;
    buf->size = 0;
    if (capacity > 0) {
        buf->bytes = (char *) pni_mem_suballocate(PN_CLASSCLASS(pn_buffer), buf, capacity);
        if (buf->bytes == NULL) {
            pni_mem_deallocate(PN_CLASSCLASS(pn_buffer), buf);
            buf = NULL;
        }
    }
    else {
        buf->bytes = NULL;
    }
  }
  return buf;
}

void pn_buffer_free(pn_buffer_t *buf)
{
  if (buf) {
    pni_mem_subdeallocate(PN_CLASSCLASS(pn_buffer), buf, buf->bytes);
    pni_mem_deallocate(PN_CLASSCLASS(pn_buffer), buf);
  }
}

size_t pn_buffer_size(pn_buffer_t *buf)
{
  return buf->size;
}

size_t pn_buffer_capacity(pn_buffer_t *buf)
{
  return buf->capacity;
}

size_t pn_buffer_available(pn_buffer_t *buf)
{
  return buf->capacity - buf->size;
}

static size_t pni_buffer_head(pn_buffer_t *buf)
{
  return buf->start;
}

static size_t pni_buffer_tail(pn_buffer_t *buf)
{
  size_t tail = buf->start + buf->size;
  if (tail >= buf->capacity)
    tail -= buf->capacity;
  return tail;
}

static bool pni_buffer_wrapped(pn_buffer_t *buf)
{
  return buf->size && pni_buffer_head(buf) >= pni_buffer_tail(buf);
}

static size_t pni_buffer_tail_space(pn_buffer_t *buf)
{
  if (pni_buffer_wrapped(buf)) {
    return pn_buffer_available(buf);
  } else {
    return buf->capacity - pni_buffer_tail(buf);
  }
}

static size_t pni_buffer_head_space(pn_buffer_t *buf)
{
  if (pni_buffer_wrapped(buf)) {
    return pn_buffer_available(buf);
  } else {
    return pni_buffer_head(buf);
  }
}

static size_t pni_buffer_head_size(pn_buffer_t *buf)
{
  if (pni_buffer_wrapped(buf)) {
    return buf->capacity - pni_buffer_head(buf);
  } else {
    return pni_buffer_tail(buf) - pni_buffer_head(buf);
  }
}

static size_t pni_buffer_tail_size(pn_buffer_t *buf)
{
  if (pni_buffer_wrapped(buf)) {
    return pni_buffer_tail(buf);
  } else {
    return 0;
  }
}

int pn_buffer_ensure(pn_buffer_t *buf, size_t size)
{
  size_t old_capacity = buf->capacity;
  size_t old_head = pni_buffer_head(buf);
  bool wrapped = pni_buffer_wrapped(buf);

  while (pn_buffer_available(buf) < size) {
    buf->capacity = 2*(buf->capacity ? buf->capacity : 16);
  }

  if (buf->capacity != old_capacity) {
    char* new_bytes = (char *) pni_mem_subreallocate(PN_CLASSCLASS(pn_buffer), buf, buf->bytes, buf->capacity);
    if (new_bytes) {
      buf->bytes = new_bytes;

      if (wrapped) {
          size_t n = old_capacity - old_head;
          memmove(buf->bytes + buf->capacity - n, buf->bytes + old_head, n);
          buf->start = buf->capacity - n;
      }
    }
  }

  return 0;
}

int pn_buffer_append(pn_buffer_t *buf, const char *bytes, size_t size)
{
  if (!size) return 0;
  int err = pn_buffer_ensure(buf, size);
  if (err) return err;

  size_t tail = pni_buffer_tail(buf);
  size_t tail_space = pni_buffer_tail_space(buf);
  size_t n = pn_min(tail_space, size);

  memmove(buf->bytes + tail, bytes, n);
  memmove(buf->bytes, bytes + n, size - n);

  buf->size += size;

  return 0;
}

int pn_buffer_prepend(pn_buffer_t *buf, const char *bytes, size_t size)
{
  int err = pn_buffer_ensure(buf, size);
  if (err) return err;

  size_t head = pni_buffer_head(buf);
  size_t head_space = pni_buffer_head_space(buf);
  size_t n = pn_min(head_space, size);

  memmove(buf->bytes + head - n, bytes + size - n, n);
  memmove(buf->bytes + buf->capacity - (size - n), bytes, size - n);

  if (buf->start >= size) {
    buf->start -= size;
  } else {
    buf->start = buf->capacity - (size - buf->start);
  }

  buf->size += size;

  return 0;
}

static size_t pni_buffer_index(pn_buffer_t *buf, size_t index)
{
  size_t result = buf->start + index;
  if (result >= buf->capacity) result -= buf->capacity;
  return result;
}

size_t pn_buffer_get(pn_buffer_t *buf, size_t offset, size_t size, char *dst)
{
  size = pn_min(size, buf->size);
  size_t start = pni_buffer_index(buf, offset);
  size_t stop = pni_buffer_index(buf, offset + size);

  if (size == 0) return 0;

  size_t sz1;
  size_t sz2;

  if (start >= stop) {
    sz1 = buf->capacity - start;
    sz2 = stop;
  } else {
    sz1 = stop - start;
    sz2 = 0;
  }

  memmove(dst, buf->bytes + start, sz1);
  memmove(dst + sz1, buf->bytes, sz2);

  return sz1 + sz2;
}

int pn_buffer_trim(pn_buffer_t *buf, size_t left, size_t right)
{
  if (left + right > buf->size) return PN_ARG_ERR;

  // In special case where we trim everything just clear buffer
  if (left + right == buf->size) {
    pn_buffer_clear(buf);
    return 0;
  }
  buf->start += left;
  if (buf->start >= buf->capacity)
    buf->start -= buf->capacity;

  buf->size -= left + right;

  return 0;
}

void pn_buffer_clear(pn_buffer_t *buf)
{
  buf->start = 0;
  buf->size = 0;
}

static void pn_buffer_rotate (pn_buffer_t *buf, size_t sz) {
  if (sz == 0) return;

  unsigned c = 0, v = 0;
  for (; c < buf->capacity; v++) {
    unsigned t = v, tp = v + sz;
    char tmp = buf->bytes[v];
    c++;
    while (tp != v) {
      buf->bytes[t] = buf->bytes[tp];
      t = tp;
      tp += sz;
      if (tp >= buf->capacity) tp -= buf->capacity;
      c++;
    }
    buf->bytes[t] = tmp;
  }
}

int pn_buffer_defrag(pn_buffer_t *buf)
{
  pn_buffer_rotate(buf, buf->start);
  buf->start = 0;
  return 0;
}

pn_bytes_t pn_buffer_bytes(pn_buffer_t *buf)
{
  if (buf) {
    pn_buffer_defrag(buf);
    return pn_bytes(buf->size, buf->bytes);
  } else {
    return pn_bytes(0, NULL);
  }
}

pn_rwbytes_t pn_buffer_memory(pn_buffer_t *buf)
{
  if (buf) {
    pn_buffer_defrag(buf);
    pn_rwbytes_t r = {buf->size, buf->bytes};
    return r;
  } else {
    pn_rwbytes_t r = {0, NULL};
    return r;
  }
}

int pn_buffer_quote(pn_buffer_t *buf, pn_string_t *str, size_t n)
{
  size_t hsize = pni_buffer_head_size(buf);
  size_t tsize = pni_buffer_tail_size(buf);
  if (hsize >= n) {
    pn_quote(str, buf->bytes + pni_buffer_head(buf), n);
    return 0;
  }
  pn_quote(str, buf->bytes + pni_buffer_head(buf), hsize);
  pn_quote(str, buf->bytes, pn_min(tsize, n-hsize));
  return 0;
}
