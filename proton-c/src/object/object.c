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

#include <proton/object.h>
#include <stdlib.h>
#include <assert.h>

#define pn_object_initialize NULL
#define pn_object_finalize NULL
#define pn_object_inspect NULL
uintptr_t pn_object_hashcode(void *object) { return (uintptr_t) object; }
intptr_t pn_object_compare(void *a, void *b) { return (intptr_t) b - (intptr_t) a; }

const pn_class_t PNI_OBJECT = PN_CLASS(pn_object);
const pn_class_t *PN_OBJECT = &PNI_OBJECT;

#define pn_void_initialize NULL
static void *pn_void_new(const pn_class_t *clazz, size_t size) { return malloc(size); }
static void pn_void_incref(void *object) {}
static void pn_void_decref(void *object) {}
static int pn_void_refcount(void *object) { return -1; }
#define pn_void_finalize NULL
static void pn_void_free(void *object) { free(object); }
static const pn_class_t *pn_void_reify(void *object) { return PN_VOID; }
uintptr_t pn_void_hashcode(void *object) { return (uintptr_t) object; }
intptr_t pn_void_compare(void *a, void *b) { return (intptr_t) b - (intptr_t) a; }
int pn_void_inspect(void *object, pn_string_t *dst) { return pn_string_addf(dst, "%p", object); }

const pn_class_t PNI_VOID = PN_METACLASS(pn_void);
const pn_class_t *PN_VOID = &PNI_VOID;

const char *pn_class_name(const pn_class_t *clazz)
{
  return clazz->name;
}

pn_cid_t pn_class_id(const pn_class_t *clazz)
{
  return clazz->cid;
}

void *pn_class_new(const pn_class_t *clazz, size_t size)
{
  assert(clazz);
  void *object = clazz->newinst(clazz, size);
  if (clazz->initialize) {
    clazz->initialize(object);
  }
  return object;
}

void *pn_class_incref(const pn_class_t *clazz, void *object)
{
  assert(clazz);
  if (object) {
    clazz = clazz->reify(object);
    clazz->incref(object);
  }
  return object;
}

int pn_class_refcount(const pn_class_t *clazz, void *object)
{
  assert(clazz);
  clazz = clazz->reify(object);
  return clazz->refcount(object);
}

int pn_class_decref(const pn_class_t *clazz, void *object)
{
  assert(clazz);

  if (object) {
    clazz = clazz->reify(object);
    clazz->decref(object);
    int rc = clazz->refcount(object);
    if (rc == 0) {
      if (clazz->finalize) {
        clazz->finalize(object);
        // check the refcount again in case the finalizer created a
        // new reference
        rc = clazz->refcount(object);
      }
      if (rc == 0) {
        clazz->free(object);
        return 0;
      }
    } else {
      return rc;
    }
  }

  return 0;
}

void pn_class_free(const pn_class_t *clazz, void *object)
{
  assert(clazz);
  if (object) {
    clazz = clazz->reify(object);
    int rc = clazz->refcount(object);
    assert(rc == 1 || rc == -1);
    if (rc == 1) {
      rc = pn_class_decref(clazz, object);
      assert(rc == 0);
    } else {
      if (clazz->finalize) {
        clazz->finalize(object);
      }
      clazz->free(object);
    }
  }
}

const pn_class_t *pn_class_reify(const pn_class_t *clazz, void *object)
{
  assert(clazz);
  return clazz->reify(object);
}

uintptr_t pn_class_hashcode(const pn_class_t *clazz, void *object)
{
  assert(clazz);

  if (!object) return 0;

  clazz = clazz->reify(object);

  if (clazz->hashcode) {
    return clazz->hashcode(object);
  } else {
    return (uintptr_t) object;
  }
}

intptr_t pn_class_compare(const pn_class_t *clazz, void *a, void *b)
{
  assert(clazz);

  if (a == b) return 0;

  clazz = clazz->reify(a);

  if (a && b && clazz->compare) {
    return clazz->compare(a, b);
  } else {
    return (intptr_t) b - (intptr_t) a;
  }
}

bool pn_class_equals(const pn_class_t *clazz, void *a, void *b)
{
  return pn_class_compare(clazz, a, b) == 0;
}

int pn_class_inspect(const pn_class_t *clazz, void *object, pn_string_t *dst)
{
  assert(clazz);

  clazz = clazz->reify(object);

  if (!pn_string_get(dst)) {
    pn_string_set(dst, "");
  }

  if (object && clazz->inspect) {
    return clazz->inspect(object, dst);
  }

  const char *name = clazz->name ? clazz->name : "<anon>";

  return pn_string_addf(dst, "%s<%p>", name, object);
}

typedef struct {
  const pn_class_t *clazz;
  int refcount;
} pni_head_t;

#define pni_head(PTR) \
  (((pni_head_t *) (PTR)) - 1)

void *pn_object_new(const pn_class_t *clazz, size_t size)
{
  pni_head_t *head = (pni_head_t *) malloc(sizeof(pni_head_t) + size);
  void *object = head + 1;
  head->clazz = clazz;
  head->refcount = 1;
  return object;
}

const pn_class_t *pn_object_reify(void *object)
{
  if (object) {
    return pni_head(object)->clazz;
  } else {
    return PN_OBJECT;
  }
}

void pn_object_incref(void *object)
{
  if (object) {
    pni_head(object)->refcount++;
  }
}

int pn_object_refcount(void *object)
{
  assert(object);
  return pni_head(object)->refcount;
}

void pn_object_decref(void *object)
{
  pni_head_t *head = pni_head(object);
  assert(head->refcount > 0);
  head->refcount--;
}

void pn_object_free(void *object)
{
  pni_head_t *head = pni_head(object);
  free(head);
}

void *pn_incref(void *object)
{
  return pn_class_incref(PN_OBJECT, object);
}

int pn_decref(void *object)
{
  return pn_class_decref(PN_OBJECT, object);
}

int pn_refcount(void *object)
{
  return pn_class_refcount(PN_OBJECT, object);
}

void pn_free(void *object)
{
  pn_class_free(PN_OBJECT, object);
}

const pn_class_t *pn_class(void *object)
{
  return pn_class_reify(PN_OBJECT, object);
}

uintptr_t pn_hashcode(void *object)
{
  return pn_class_hashcode(PN_OBJECT, object);
}

intptr_t pn_compare(void *a, void *b)
{
  return pn_class_compare(PN_OBJECT, a, b);
}

bool pn_equals(void *a, void *b)
{
  return !pn_compare(a, b);
}

int pn_inspect(void *object, pn_string_t *dst)
{
  return pn_class_inspect(PN_OBJECT, object, dst);
}

#define pn_weakref_new NULL
#define pn_weakref_initialize NULL
#define pn_weakref_finalize NULL
#define pn_weakref_free NULL

static void pn_weakref_incref(void *object) {}
static void pn_weakref_decref(void *object) {}
static int pn_weakref_refcount(void *object) { return -1; }
static const pn_class_t *pn_weakref_reify(void *object) {
  return PN_WEAKREF;
}
static uintptr_t pn_weakref_hashcode(void *object) {
  return pn_hashcode(object);
}
static intptr_t pn_weakref_compare(void *a, void *b) {
  return pn_compare(a, b);
}
static int pn_weakref_inspect(void *object, pn_string_t *dst) {
  return pn_inspect(object, dst);
}

const pn_class_t PNI_WEAKREF = PN_METACLASS(pn_weakref);
const pn_class_t *PN_WEAKREF = &PNI_WEAKREF;
