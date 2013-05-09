#ifndef PROTON_OBJECT_H
#define PROTON_OBJECT_H 1

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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <proton/import_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void (*finalize)(void *);
  uintptr_t (*hashcode)(void *);
  intptr_t (*compare)(void *, void *);
} pn_class_t;

PN_EXTERN void *pn_new(size_t size, pn_class_t *clazz);
PN_EXTERN void *pn_incref(void *object);
PN_EXTERN void pn_decref(void *object);
PN_EXTERN int pn_refcount(void *object);
PN_EXTERN void pn_free(void *object);
PN_EXTERN pn_class_t *pn_class(void *object);
PN_EXTERN uintptr_t pn_hashcode(void *object);
PN_EXTERN intptr_t pn_compare(void *a, void *b);
PN_EXTERN bool pn_equals(void *a, void *b);

#define PN_REFCOUNT (0x1)

typedef struct pn_list_t pn_list_t;

PN_EXTERN pn_list_t *pn_list(size_t capacity, int options);
PN_EXTERN size_t pn_list_size(pn_list_t *list);
PN_EXTERN void *pn_list_get(pn_list_t *list, int index);
PN_EXTERN void pn_list_set(pn_list_t *list, int index, void *value);
PN_EXTERN int pn_list_add(pn_list_t *list, void *value);
PN_EXTERN void pn_list_del(pn_list_t *list, int index, int n);

typedef struct pn_map_t pn_map_t;

#define PN_REFCOUNT_KEY (0x2)
#define PN_REFCOUNT_VALUE (0x4)

PN_EXTERN pn_map_t *pn_map(size_t capacity, float load_factor, int options);
PN_EXTERN size_t pn_map_size(pn_map_t *map);
PN_EXTERN int pn_map_put(pn_map_t *map, void *key, void *value);
PN_EXTERN void *pn_map_get(pn_map_t *map, void *key);
PN_EXTERN void pn_map_del(pn_map_t *map, void *key);

typedef struct pn_hash_t pn_hash_t;

PN_EXTERN pn_hash_t *pn_hash(size_t capacity, float load_factor, int options);
PN_EXTERN size_t pn_hash_size(pn_hash_t *hash);
PN_EXTERN int pn_hash_put(pn_hash_t *hash, uintptr_t key, void *value);
PN_EXTERN void *pn_hash_get(pn_hash_t *hash, uintptr_t key);
PN_EXTERN void pn_hash_del(pn_hash_t *hash, uintptr_t key);

typedef struct pn_string_t pn_string_t;

PN_EXTERN pn_string_t *pn_string(const char *bytes);
PN_EXTERN pn_string_t *pn_stringn(const char *bytes, size_t n);
PN_EXTERN const char *pn_string_get(pn_string_t *string);
PN_EXTERN size_t pn_string_size(pn_string_t *string);
PN_EXTERN int pn_string_set(pn_string_t *string, const char *bytes);
PN_EXTERN int pn_string_setn(pn_string_t *string, const char *bytes, size_t n);
PN_EXTERN void pn_string_clear(pn_string_t *string);

#ifdef __cplusplus
}
#endif

#endif /* object.h */
