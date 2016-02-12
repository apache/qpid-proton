#ifndef _PROTON_SRC_UTIL_H
#define _PROTON_SRC_UTIL_H 1

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

#include <errno.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proton/types.h>
#include <proton/object.h>

void pni_parse_url(char *url, char **scheme, char **user, char **pass, char **host, char **port, char **path);
void pni_fatal(const char *fmt, ...);
void pni_vfatal(const char *fmt, va_list ap);
ssize_t pn_quote_data(char *dst, size_t capacity, const char *src, size_t size);
int pn_quote(pn_string_t *dst, const char *src, size_t size);
void pn_fprint_data(FILE *stream, const char *bytes, size_t size);
void pn_print_data(const char *bytes, size_t size);
bool pn_env_bool(const char *name);
pn_timestamp_t pn_timestamp_min(pn_timestamp_t a, pn_timestamp_t b);

char *pn_strdup(const char *src);
char *pn_strndup(const char *src, size_t n);
int pn_strcasecmp(const char* a, const char* b);
int pn_strncasecmp(const char* a, const char* b, size_t len);

#define DIE_IFR(EXPR, STRERR)                                           \
  do {                                                                  \
    int __code__ = (EXPR);                                              \
    if (__code__) {                                                     \
      fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__,       \
              #EXPR, STRERR(__code__), __code__);                       \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

#define DIE_IFE(EXPR)                                                   \
  do {                                                                  \
    if ((EXPR) == -1) {                                                 \
      int __code__ = errno;                                             \
      fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__,       \
              #EXPR, strerror(__code__), __code__);                     \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)


#define LL_HEAD(ROOT, LIST) ((ROOT)-> LIST ## _head)
#define LL_TAIL(ROOT, LIST) ((ROOT)-> LIST ## _tail)
#define LL_ADD(ROOT, LIST, NODE)                              \
  {                                                           \
    (NODE)-> LIST ## _next = NULL;                            \
    (NODE)-> LIST ## _prev = (ROOT)-> LIST ## _tail;          \
    if (LL_TAIL(ROOT, LIST))                                  \
      LL_TAIL(ROOT, LIST)-> LIST ## _next = (NODE);           \
    LL_TAIL(ROOT, LIST) = (NODE);                             \
    if (!LL_HEAD(ROOT, LIST)) LL_HEAD(ROOT, LIST) = (NODE);   \
  }

#define LL_POP(ROOT, LIST, TYPE)                              \
  {                                                           \
    if (LL_HEAD(ROOT, LIST)) {                                \
      TYPE *_old = LL_HEAD(ROOT, LIST);                       \
      LL_HEAD(ROOT, LIST) = LL_HEAD(ROOT, LIST)-> LIST ## _next; \
      _old-> LIST ## _next = NULL;                            \
      if (_old == LL_TAIL(ROOT, LIST)) {                      \
        LL_TAIL(ROOT, LIST) = NULL;                           \
      } else {                                                \
        LL_HEAD(ROOT, LIST)-> LIST ## _prev = NULL;           \
      }                                                       \
    }                                                         \
  }

#define LL_REMOVE(ROOT, LIST, NODE)                                    \
  {                                                                    \
    if ((NODE)-> LIST ## _prev)                                        \
      (NODE)-> LIST ## _prev-> LIST ## _next = (NODE)-> LIST ## _next; \
    if ((NODE)-> LIST ## _next)                                        \
      (NODE)-> LIST ## _next-> LIST ## _prev = (NODE)-> LIST ## _prev; \
    if ((NODE) == LL_HEAD(ROOT, LIST))                                 \
      LL_HEAD(ROOT, LIST) = (NODE)-> LIST ## _next;                    \
    if ((NODE) == LL_TAIL(ROOT, LIST))                                 \
      LL_TAIL(ROOT, LIST) = (NODE)-> LIST ## _prev;                    \
  }

#define pn_min(X,Y) ((X) > (Y) ? (Y) : (X))
#define pn_max(X,Y) ((X) < (Y) ? (Y) : (X))

#define PN_ENSURE(ARRAY, CAPACITY, COUNT, TYPE)                 \
  while ((CAPACITY) < (COUNT)) {                                \
    (CAPACITY) = (CAPACITY) ? 2 * (CAPACITY) : 16;              \
    (ARRAY) = (TYPE *) realloc((ARRAY), (CAPACITY) * sizeof (TYPE));    \
  }                                                             \

#define PN_ENSUREZ(ARRAY, CAPACITY, COUNT, TYPE)           \
  {                                                        \
    size_t _old_capacity = (CAPACITY);                     \
    PN_ENSURE(ARRAY, CAPACITY, COUNT, TYPE);               \
    memset((ARRAY) + _old_capacity, 0,                     \
           sizeof(TYPE)*((CAPACITY) - _old_capacity));     \
  }

#endif /* util.h */
