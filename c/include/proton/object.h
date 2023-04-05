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

#include <proton/cid.h>
#include <proton/types.h>
#include <proton/import_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @cond INTERNAL
 */
PN_EXTERN pn_class_t *pn_class_create(const char *name,
                                      void (*initialize)(void*),
                                      void (*finalize)(void*),
                                      void (*incref)(void*),
                                      void (*decref)(void*),
                                      int (*refcount)(void*));

PN_EXTERN void *pn_class_new(const pn_class_t *clazz, size_t size);
PN_EXTERN const char *pn_class_name(const pn_class_t *clazz);
PN_EXTERN pn_cid_t pn_class_id(const pn_class_t *clazz);

PN_EXTERN const pn_class_t *pn_class(void* object);
PN_EXTERN void *pn_incref(void *object);
PN_EXTERN int pn_decref(void *object);
PN_EXTERN int pn_refcount(void *object);
PN_EXTERN void pn_free(void *object);
PN_EXTERN char *pn_tostring(void *object);

#define PN_LEGCTX ((pn_handle_t) 0)

/**
 *   PN_HANDLE is a trick to define a unique identifier by using the address of a static variable.
 *   You MUST NOT use it in a .h file, since it must be defined uniquely in one compilation unit.
 *   Your .h file can provide access to the handle (if needed) via a function. For example:
 *
 *       /// my_thing.h
 *       pn_handle_t get_my_thing(void);
 *
 *       /// my_thing.c
 *       PN_HANDLE(MY_THING);
 *       pn_handle_t get_my_thing(void) { return MY_THING; }
 *
 *   Note that the name "MY_THING" is not exported and is not required to be
 *   unique except in the .c file. The linker will guarantee that the *address* of
 *   MY_THING, as returned by get_my_thing() *is* unique across the entire linked
 *   executable.
 */
#define PN_HANDLE(name) \
static const char _PN_HANDLE_ ## name = 0; \
static const pn_handle_t name = ((pn_handle_t) &_PN_HANDLE_ ## name);

PN_EXTERN extern const pn_class_t *PN_OBJECT;
PN_EXTERN extern const pn_class_t *PN_VOID;

PN_EXTERN pn_record_t *pn_record(void);
PN_EXTERN void pn_record_def(pn_record_t *record, pn_handle_t key, const pn_class_t *clazz);
PN_EXTERN bool pn_record_has(pn_record_t *record, pn_handle_t key);
PN_EXTERN void *pn_record_get(pn_record_t *record, pn_handle_t key);
PN_EXTERN void pn_record_set(pn_record_t *record, pn_handle_t key, void *value);
PN_EXTERN void pn_record_clear(pn_record_t *record);

/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* object.h */
