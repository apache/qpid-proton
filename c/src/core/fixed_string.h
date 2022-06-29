#ifndef PROTON_FIXED_STRING_H
#define PROTON_FIXED_STRING_H 1

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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct pn_fixed_string_t {
  char    *bytes;
  uint32_t size;
  uint32_t position;
} pn_fixed_string_t;

typedef struct pn_string_const_t {
  const char *bytes;
  uint32_t size;
} pn_string_const_t;

static inline pn_fixed_string_t pn_fixed_string(char *bytes, uint32_t size) {
  return (pn_fixed_string_t) {.bytes=bytes, .size=size, .position=0};
}

static inline pn_string_const_t pn_string_const(const char* bytes, uint32_t size) {
  return (pn_string_const_t) {.bytes=bytes, .size=size};
}

#define STR_CONST(x) pn_string_const(#x, sizeof(#x)-1)

static inline void pn_fixed_string_append(pn_fixed_string_t *str, const pn_string_const_t chars) {
  uint32_t copy_size = pn_min(chars.size, str->size-str->position);
  if (copy_size==0) return;

  memcpy(&str->bytes[str->position], chars.bytes, copy_size);
  str->position += copy_size;
}

static inline void pn_fixed_string_vaddf(pn_fixed_string_t *str, const char *format, va_list ap){
  uint32_t bytes_left = str->size-str->position;
  if (bytes_left==0) return;

  char *out = &str->bytes[str->position];
  int out_size = vsnprintf(out, bytes_left, format, ap);
  if ( out_size<0 ) return;
  str->position += pn_min((uint32_t)out_size, bytes_left);
}

static inline void pn_fixed_string_addf(pn_fixed_string_t *str, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  pn_fixed_string_vaddf(str, format, ap);
  va_end(ap);
}

static inline void pn_fixed_string_quote(pn_fixed_string_t *str, const char *data, size_t size) {
  size_t bytes_left = str->size-str->position;
  if (bytes_left==0) return;

  char *out = &str->bytes[str->position];
  ssize_t out_size = pn_quote_data(out, bytes_left, data, size);
  // The only error (ie value less than 0) that can come from pn_quote_data is PN_OVERFLOW
  if ( out_size>0 ) {
    str->position += out_size;
  } else {
    str->position = str->size;
  }
}

static inline void pn_fixed_string_terminate(pn_fixed_string_t *str) {
  if (str->position==str->size) str->position--;
  str->bytes[str->position] = 0;
}

#endif // PROTON_FIXED_STRING_H
