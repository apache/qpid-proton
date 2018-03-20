#ifndef PROTON_BUFFER_H
#define PROTON_BUFFER_H 1

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

#include <proton/import_export.h>
#include <proton/object.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_buffer_t pn_buffer_t;

pn_buffer_t *pn_buffer(size_t capacity);
void pn_buffer_free(pn_buffer_t *buf);
size_t pn_buffer_size(pn_buffer_t *buf);
size_t pn_buffer_capacity(pn_buffer_t *buf);
size_t pn_buffer_available(pn_buffer_t *buf);
int pn_buffer_ensure(pn_buffer_t *buf, size_t size);
int pn_buffer_append(pn_buffer_t *buf, const char *bytes, size_t size);
int pn_buffer_prepend(pn_buffer_t *buf, const char *bytes, size_t size);
size_t pn_buffer_get(pn_buffer_t *buf, size_t offset, size_t size, char *dst);
int pn_buffer_trim(pn_buffer_t *buf, size_t left, size_t right);
void pn_buffer_clear(pn_buffer_t *buf);
int pn_buffer_defrag(pn_buffer_t *buf);
pn_bytes_t pn_buffer_bytes(pn_buffer_t *buf);
pn_rwbytes_t pn_buffer_memory(pn_buffer_t *buf);
int pn_buffer_quote(pn_buffer_t *buf, pn_string_t *string, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* buffer.h */
