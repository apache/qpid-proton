#ifndef MAX_ALIGN_H
#define MAX_ALIGN_H

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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __STDC_VERSION__ >= 201112
/* Use standard max_align_t for alignment on c11 */
typedef max_align_t pn_max_align_t;
#else
/* Align on a union of likely largest types for older compilers */
typedef union pn_max_align_t {
  uint64_t i;
  long double d;
  void *v;
  void (*fp)(void);
} pn_max_align_t;
#endif

#ifdef __cplusplus
}
#endif

#endif // MAX_ALIGN_H
