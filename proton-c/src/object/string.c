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

#include "platform.h"

#include <proton/error.h>
#include <proton/object.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

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

static int pn_string_inspect(void *obj, pn_string_t *dst)
{
  pn_string_t *str = (pn_string_t *) obj;
  if (str->size == PNI_NULL_SIZE) {
    return pn_string_addf(dst, "null");
  }

  int err = pn_string_addf(dst, "\"");

  for (int i = 0; i < str->size; i++) {
    uint8_t c = str->bytes[i];
    if (isprint(c)) {
      err = pn_string_addf(dst, "%c", c);
      if (err) return err;
    } else {
      err = pn_string_addf(dst, "\\x%.2x", c);
      if (err) return err;
    }
  }

  return pn_string_addf(dst, "\"");
}

pn_string_t *pn_string(const char *bytes)
{
  return pn_stringn(bytes, bytes ? strlen(bytes) : 0);
}

#define pn_string_initialize NULL


pn_string_t *pn_stringn(const char *bytes, size_t n)
{
  static const pn_class_t clazz = PN_CLASS(pn_string);
  pn_string_t *string = (pn_string_t *) pn_class_new(&clazz, sizeof(pn_string_t));
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

int pn_string_grow(pn_string_t *string, size_t capacity)
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

  va_start(ap, format);
  int err = pn_string_vformat(string, format, ap);
  va_end(ap);
  return err;
}

int pn_string_vformat(pn_string_t *string, const char *format, va_list ap)
{
  pn_string_set(string, "");
  return pn_string_vaddf(string, format, ap);
}

int pn_string_addf(pn_string_t *string, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  int err = pn_string_vaddf(string, format, ap);
  va_end(ap);
  return err;
}

int pn_string_vaddf(pn_string_t *string, const char *format, va_list ap)
{
  va_list copy;

  if (string->size == PNI_NULL_SIZE) {
    return PN_ERR;
  }

  while (true) {
    va_copy(copy, ap);
    int err = vsnprintf(string->bytes + string->size, string->capacity - string->size, format, copy);
    va_end(copy);
    if (err < 0) {
      return err;
    } else if ((size_t) err >= string->capacity - string->size) {
      pn_string_grow(string, string->size + err);
    } else {
      string->size += err;
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
