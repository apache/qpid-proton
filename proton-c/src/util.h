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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

ssize_t pn_quote_data(char *dst, size_t capacity, const char *src, size_t size);
void pn_fprint_data(FILE *stream, const char *bytes, size_t size);
void pn_print_data(const char *bytes, size_t size);
bool pn_env_bool(const char *name);

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

#define __EMPTY__next next
#define __EMPTY__prev prev
#define LL_ADD(HEAD, TAIL, NODE) LL_ADD_PFX(HEAD, TAIL, NODE, __EMPTY__)
#define LL_ADD_PFX(HEAD, TAIL, NODE, PFX)    \
  {                                          \
    (NODE)-> PFX ## next = NULL;             \
    (NODE)-> PFX ## prev = (TAIL);           \
    if (TAIL) (TAIL)-> PFX ## next = (NODE); \
    (TAIL) = (NODE);                         \
    if (!(HEAD)) (HEAD) = NODE;              \
  }

#define LL_POP(HEAD, TAIL) LL_POP_PFX(HEAD, TAIL, __EMPTY__)
#define LL_POP_PFX(HEAD, TAIL, PFX)  \
  {                                  \
    if (HEAD) {                      \
      void *_head = (HEAD);          \
      (HEAD) = (HEAD)-> PFX ## next; \
      if (_head == (TAIL))           \
        (TAIL) = NULL;               \
    }                                \
  }

#define LL_REMOVE(HEAD, TAIL, NODE) LL_REMOVE_PFX(HEAD, TAIL, NODE, __EMPTY__)
#define LL_REMOVE_PFX(HEAD, TAIL, NODE, PFX)                     \
  {                                                              \
    if ((NODE)-> PFX ## prev)                                    \
      (NODE)-> PFX ## prev-> PFX ## next = (NODE)-> PFX ## next; \
    if ((NODE)-> PFX ## next)                                    \
      (NODE)-> PFX ## next-> PFX ## prev = (NODE)-> PFX ## prev; \
    if ((NODE) == (HEAD))                                        \
      (HEAD) = (NODE)-> PFX ## next;                             \
    if ((NODE) == (TAIL))                                        \
      (TAIL) = (NODE)-> PFX ## prev;                             \
  }

char *pn_strdup(const char *src);
#define strdup pn_strdup

#endif /* util.h */
