#ifndef PROTON_OBJECT_PRIVATE_H
#define PROTON_OBJECT_PRIVATE_H 1

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

#include "proton/object.h"

#include <proton/annotations.h>
#include <proton/types.h>
#include <stdarg.h>
#include <proton/type_compat.h>
#include <stddef.h>
#include <proton/import_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @cond INTERNAL
 */

typedef intptr_t pn_shandle_t;

typedef struct pn_list_t pn_list_t;
typedef struct pn_string_t pn_string_t;
typedef struct pn_map_t pn_map_t;
typedef struct pn_hash_t pn_hash_t;
typedef void *(*pn_iterator_next_t)(void *state);
typedef struct pn_iterator_t pn_iterator_t;

struct pn_fixed_string_t;
struct pn_class_t {
    const char *name;
    pn_cid_t cid;
    void *(*newinst)(const pn_class_t *, size_t);
    void (*initialize)(void *);
    void (*incref)(void *);
    void (*decref)(void *);
    int (*refcount)(void *);
    void (*finalize)(void *);
    void (*free)(void *);
    uintptr_t (*hashcode)(void *);
    intptr_t (*compare)(void *, void *);
    void (*inspect)(void *, struct pn_fixed_string_t*);
};

#define PN_CLASS(PREFIX) {                      \
    #PREFIX,                                    \
    CID_ ## PREFIX,                             \
    NULL,                                       \
    PREFIX ## _initialize,                      \
    NULL,                                       \
    NULL,                                       \
    NULL,                                       \
    PREFIX ## _finalize,                        \
    NULL,                                       \
    PREFIX ## _hashcode,                        \
    PREFIX ## _compare,                         \
    PREFIX ## _inspect                          \
}

#define PN_METACLASS(PREFIX) {                  \
    #PREFIX,                                    \
    CID_ ## PREFIX,                             \
    PREFIX ## _new,                             \
    PREFIX ## _initialize,                      \
    PREFIX ## _incref,                          \
    PREFIX ## _decref,                          \
    PREFIX ## _refcount,                        \
    PREFIX ## _finalize,                        \
    PREFIX ## _free,                            \
    PREFIX ## _hashcode,                        \
    PREFIX ## _compare,                         \
    PREFIX ## _inspect                          \
}

/* Hack alert: Declare these as arrays so we can treat the name of the single
   object as the address */
PN_EXTERN extern const pn_class_t PN_DEFAULT[];
PN_EXTERN extern const pn_class_t PN_WEAKREF[];

#define PN_CLASSCLASS(PREFIX) PREFIX ## __class

/* Class to identify a plain C struct in a pn_event_t. No refcounting or memory management. */
#define PN_STRUCT_CLASSDEF(PREFIX)                  \
const pn_class_t PN_CLASSCLASS(PREFIX)[] = {{       \
  #PREFIX,                                          \
  CID_ ## PREFIX,                                   \
  NULL, /*_new*/                                    \
  NULL, /*_initialize*/                             \
  pn_void_incref,                                   \
  pn_void_decref,                                   \
  pn_void_refcount,                                 \
  NULL, /* _finalize */                             \
  NULL, /* _free */                                 \
  NULL, /* _hashcode */                             \
  NULL, /* _compare */                              \
  NULL, /* _inspect */                              \
}};                                                 \

PN_EXTERN void *pn_class_incref(const pn_class_t *clazz, void *object);
PN_EXTERN int pn_class_refcount(const pn_class_t *clazz, void *object);
PN_EXTERN int pn_class_decref(const pn_class_t *clazz, void *object);

PN_EXTERN void pn_class_free(const pn_class_t *clazz, void *object);

PN_EXTERN intptr_t pn_class_compare(const pn_class_t *clazz, void *a, void *b);
PN_EXTERN bool pn_class_equals(const pn_class_t *clazz, void *a, void *b);
PN_EXTERN void pn_class_inspect(const pn_class_t *clazz, void *object, struct pn_fixed_string_t *dst);

PN_EXTERN void pn_object_incref(void *object);

PN_EXTERN void *pn_void_new(const pn_class_t *clazz, size_t size);
PN_EXTERN void pn_void_incref(void *object);
PN_EXTERN void pn_void_decref(void *object);
PN_EXTERN int pn_void_refcount(void *object);

void pn_finspect(void *object, struct pn_fixed_string_t *dst);
PN_EXTERN int pn_inspect(void *object, pn_string_t *dst);
PN_EXTERN uintptr_t pn_hashcode(void *object);
PN_EXTERN intptr_t pn_compare(void *a, void *b);
PN_EXTERN bool pn_equals(void *a, void *b);

PN_EXTERN pn_list_t *pn_list(const pn_class_t *clazz, size_t capacity);
PN_EXTERN size_t pn_list_size(pn_list_t *list);
PN_EXTERN void *pn_list_get(pn_list_t *list, int index);
PN_EXTERN int pn_list_add(pn_list_t *list, void *value);
PN_EXTERN bool pn_list_remove(pn_list_t *list, void *value);
void pn_list_set(pn_list_t *list, int index, void *value);
void *pn_list_pop(pn_list_t *list);
PN_EXTERN ssize_t pn_list_index(pn_list_t *list, void *value);
PN_EXTERN void pn_list_del(pn_list_t *list, int index, int n);
void pn_list_clear(pn_list_t *list);
void pn_list_iterator(pn_list_t *list, pn_iterator_t *iter);
PN_EXTERN void pn_list_minpush(pn_list_t *list, void *value);
PN_EXTERN void *pn_list_minpop(pn_list_t *list);

PN_EXTERN pn_map_t *pn_map(const pn_class_t *key, const pn_class_t *value,
                           size_t capacity, float load_factor);
PN_EXTERN size_t pn_map_size(pn_map_t *map);
PN_EXTERN int pn_map_put(pn_map_t *map, void *key, void *value);
PN_EXTERN void *pn_map_get(pn_map_t *map, void *key);
PN_EXTERN void pn_map_del(pn_map_t *map, void *key);
PN_EXTERN pn_handle_t pn_map_head(pn_map_t *map);
PN_EXTERN pn_handle_t pn_map_next(pn_map_t *map, pn_handle_t entry);
PN_EXTERN void *pn_map_key(pn_map_t *map, pn_handle_t entry);
PN_EXTERN void *pn_map_value(pn_map_t *map, pn_handle_t entry);

PN_EXTERN pn_hash_t *pn_hash(const pn_class_t *clazz, size_t capacity, float load_factor);
PN_EXTERN size_t pn_hash_size(pn_hash_t *hash);
PN_EXTERN int pn_hash_put(pn_hash_t *hash, uintptr_t key, void *value);
PN_EXTERN void *pn_hash_get(pn_hash_t *hash, uintptr_t key);
PN_EXTERN void pn_hash_del(pn_hash_t *hash, uintptr_t key);
PN_EXTERN pn_handle_t pn_hash_head(pn_hash_t *hash);
PN_EXTERN pn_handle_t pn_hash_next(pn_hash_t *hash, pn_handle_t entry);
PN_EXTERN uintptr_t pn_hash_key(pn_hash_t *hash, pn_handle_t entry);
PN_EXTERN void *pn_hash_value(pn_hash_t *hash, pn_handle_t entry);

PN_EXTERN pn_string_t *pn_string(const char *bytes);
PN_EXTERN const char *pn_string_get(pn_string_t *string);
pn_bytes_t pn_string_bytes(pn_string_t *string);
PN_EXTERN pn_string_t *pn_stringn(const char *bytes, size_t n);
PN_EXTERN size_t pn_string_size(pn_string_t *string);
PN_EXTERN int pn_string_set(pn_string_t *string, const char *bytes);
PN_EXTERN int pn_string_setn(pn_string_t *string, const char *bytes, size_t n);
ssize_t pn_string_put(pn_string_t *string, char *dst);
void pn_string_clear(pn_string_t *string);
PN_EXTERN int pn_string_format(pn_string_t *string, PN_PRINTF_FORMAT const char *format, ...)
        PN_PRINTF_FORMAT_ATTR(2, 3);
int pn_string_vformat(pn_string_t *string, const char *format, va_list ap);
PN_EXTERN int pn_string_addf(pn_string_t *string, PN_PRINTF_FORMAT const char *format, ...)
        PN_PRINTF_FORMAT_ATTR(2, 3);
int pn_string_vaddf(pn_string_t *string, const char *format, va_list ap);
int pn_string_grow(pn_string_t *string, size_t capacity);
char *pn_string_buffer(pn_string_t *string);
size_t pn_string_capacity(pn_string_t *string);
int pn_string_resize(pn_string_t *string, size_t size);
int pn_string_copy(pn_string_t *string, pn_string_t *src);

PN_EXTERN pn_iterator_t *pn_iterator(void);
PN_EXTERN void *pn_iterator_start(pn_iterator_t *iterator,
                        pn_iterator_next_t next, size_t size);
PN_EXTERN void *pn_iterator_next(pn_iterator_t *iterator);

/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif // PROTON_OBJECT_PRIVATE_H
