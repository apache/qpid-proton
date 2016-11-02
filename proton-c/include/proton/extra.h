#ifndef EXTRA_H
#define EXTRA_H

/*
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
 */

#include <proton/type_compat.h>
#include <proton/types.h>
#include <stddef.h>
#include <stdlib.h>

/**
 * @cond INTERNAL
 * Support for allocating extra aligned memory after a type.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * extra_t contains a size and is maximally aligned so the memory immediately
 * after it can store any type of value.
 */
typedef union pn_extra_t {
  size_t size;
#if __STDC_VERSION__ >= 201112
  max_align_t max;
#else
/* Not standard but fairly safe */
  uint64_t i;
  long double d;
  void *v;
  void (*fp)(void);
#endif
} pn_extra_t;

static inline pn_rwbytes_t pn_extra_rwbytes(pn_extra_t *x) {
    return pn_rwbytes(x->size, (char*)(x+1));
}

/* Declare private helper struct for T */
#define PN_EXTRA_DECLARE(T) typedef struct T##__extra { T base; pn_extra_t extra; } T##__extra
#define PN_EXTRA_SIZEOF(T, N) (sizeof(T##__extra)+(N))
#define PN_EXTRA_GET(T, P) pn_extra_rwbytes(&((T##__extra*)(P))->extra)

#ifdef __cplusplus
}
#endif

/** @endcond */

#endif // EXTRA_H
