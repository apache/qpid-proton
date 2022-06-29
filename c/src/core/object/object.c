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

#include "core/memory.h"
#include "core/fixed_string.h"
#include "core/object_private.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CID_pn_default CID_pn_object
#define pn_default_initialize NULL
#define pn_default_finalize NULL
#define pn_default_inspect NULL
#define pn_default_hashcode NULL
#define pn_default_compare NULL

const pn_class_t PN_DEFAULT[] = {PN_CLASS(pn_default)};

void *pn_void_new(const pn_class_t *clazz, size_t size) { return pni_mem_allocate(clazz, size); }
#define pn_void_initialize NULL
#define pn_void_finalize NULL
static void pn_void_free(void *object) { pni_mem_deallocate(PN_VOID, object); }

void pn_void_incref(void* p) {}
void pn_void_decref(void* p) {}
int pn_void_refcount(void *object) { return -1; }

#define pn_void_hashcode NULL
#define pn_void_compare NULL
#define pn_void_inspect NULL

static const pn_class_t PN_VOID_S = PN_METACLASS(pn_void);
const pn_class_t *PN_VOID = &PN_VOID_S;

typedef struct {
  const pn_class_t *clazz;
  int refcount;
} pni_head_t;

#define pni_head(PTR) \
(((pni_head_t *) (PTR)) - 1)

pn_class_t *pn_class_create(const char *name,
                            void (*initialize)(void*),
                            void (*finalize)(void*),
                            void (*incref)(void*),
                            void (*decref)(void*),
                            int (*refcount)(void*))
{
  pn_class_t *clazz = malloc(sizeof( pn_class_t));
  *clazz = (pn_class_t) {
    .name = name,
    .cid = CID_pn_void,
    .initialize = initialize,
    .finalize = finalize,
    .incref = incref,
    .decref = decref,
    .refcount = refcount
  };
  return clazz;
}

const char *pn_class_name(const pn_class_t *clazz)
{
  return clazz->name;
}

pn_cid_t pn_class_id(const pn_class_t *clazz)
{
  return clazz->cid;
}

static inline void *pni_default_new(const pn_class_t *clazz, size_t size)
{
  void *object = NULL;
  pni_head_t *head = (pni_head_t *) pni_mem_zallocate(clazz, sizeof(pni_head_t) + size);
  if (head != NULL) {
    object = head + 1;
    head->clazz = clazz;
    head->refcount = 1;
  }
  return object;
}

static inline void pni_default_incref(void *object) {
  if (object) {
    pni_head(object)->refcount++;
  }
}

static inline int pni_default_refcount(void *object)
{
  assert(object);
  return pni_head(object)->refcount;
}

static inline void pni_default_decref(void *object)
{
  pni_head_t *head = pni_head(object);
  assert(head->refcount > 0);
  head->refcount--;
}

static inline void pni_default_free(void *object)
{
  pni_head_t *head = pni_head(object);
  pni_mem_deallocate(head->clazz, head);
}

void pn_object_incref(void *object) {
  pni_default_incref(object);
}

static inline void *pni_class_new(const pn_class_t *clazz, size_t size) {
  if (clazz->newinst) {
    return clazz->newinst(clazz, size);
  } else {
    return pni_default_new(clazz, size);
  }
}

static inline void pni_class_incref(const pn_class_t *clazz, void *object) {
  if (clazz->incref) {
    clazz->incref(object);
  } else {
    pni_default_incref(object);
  }
}

static inline void pni_class_decref(const pn_class_t *clazz, void *object) {
  if (clazz->decref) {
    clazz->decref(object);
  } else {
    pni_default_decref(object);
  }
}

static inline int pni_class_refcount(const pn_class_t *clazz, void *object) {
  if (clazz->refcount) {
    return clazz->refcount(object);
  } else {
    return pni_default_refcount(object);
  }
}

static inline void pni_class_free(const pn_class_t *clazz, void *object) {
  if (clazz->free) {
    clazz->free(object);
  } else {
    pni_default_free(object);
  }
}

void *pn_class_new(const pn_class_t *clazz, size_t size)
{
  assert(clazz);
  void *object = pni_class_new(clazz, size);
  if (object && clazz->initialize) {
    clazz->initialize(object);
  }
  return object;
}

void *pn_class_incref(const pn_class_t *clazz, void *object)
{
  if (object) {
    pni_class_incref(clazz, object);
  }
  return object;
}

int pn_class_refcount(const pn_class_t *clazz, void *object)
{
  return pni_class_refcount(clazz, object);
}

int pn_class_decref(const pn_class_t *clazz, void *object)
{
  if (object) {
    pni_class_decref(clazz, object);
    int rc = pni_class_refcount(clazz, object);
    if (rc == 0) {
      if (clazz->finalize) {
        clazz->finalize(object);
        // check the refcount again in case the finalizer created a
        // new reference
        rc = pni_class_refcount(clazz, object);
      }
      if (rc == 0) {
        pni_class_free(clazz, object);
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
  if (object) {
    int rc = pni_class_refcount(clazz, object);
    assert(rc == 1 || rc == -1);
    if (rc == 1) {
      rc = pn_class_decref(clazz, object);
      assert(rc == 0);
    } else {
      if (clazz->finalize) {
        clazz->finalize(object);
      }
      pni_class_free(clazz, object);
    }
  }
}

intptr_t pn_class_compare(const pn_class_t *clazz, void *a, void *b)
{
  if (a == b) return 0;

  if (a && b) {
    if (clazz->compare) {
      return clazz->compare(a, b);
    }
  }
  return (intptr_t) a - (intptr_t) b;
}

bool pn_class_equals(const pn_class_t *clazz, void *a, void *b)
{
  return pn_class_compare(clazz, a, b) == 0;
}

void pn_class_inspect(const pn_class_t *clazz, void *object, pn_fixed_string_t *dst)
{
  if (object && clazz->inspect) {
    clazz->inspect(object, dst);
    return;
  }

  const char *name = clazz->name ? clazz->name : "<anon>";

  pn_fixed_string_addf(dst, "%s<%p>", name, object);
  return;
}

void *pn_incref(void *object)
{
  if (object) {
    const pn_class_t *clazz = pni_head(object)->clazz;
    pni_class_incref(clazz, object);
  }
  return object;
}

int pn_decref(void *object)
{
  if (object) {
    const pn_class_t *clazz = pni_head(object)->clazz;
    pni_class_decref(clazz, object);
    int rc = pni_class_refcount(clazz, object);
    if (rc == 0) {
      if (clazz->finalize) {
        clazz->finalize(object);
        // check the refcount again in case the finalizer created a
        // new reference
        rc = pni_class_refcount(clazz, object);
      }
      if (rc == 0) {
        pni_class_free(clazz, object);
        return 0;
      }
    } else {
      return rc;
    }
  }
  return 0;
}

int pn_refcount(void *object)
{
  assert(object);
  const pn_class_t *clazz = pni_head(object)->clazz;
  return pni_class_refcount(clazz, object);
}

void pn_free(void *object)
{
  if (object) {
    const pn_class_t *clazz = pni_head(object)->clazz;
    int rc = pni_class_refcount(clazz, object);
    assert(rc == 1 || rc == -1);
    if (rc == 1) {
      pni_class_decref(clazz, object);
      assert(pni_class_refcount(clazz, object) == 0);
      if (clazz->finalize) {
        clazz->finalize(object);
      }
      if (pni_class_refcount(clazz, object) == 0) {
        pni_class_free(clazz, object);
      }
    } else {
      if (clazz->finalize) {
        clazz->finalize(object);
      }
      pni_class_free(clazz, object);
    }
  }
}

const pn_class_t *pn_class(void *object)
{
  if (object) {
    return pni_head(object)->clazz;
  } else {
    return PN_DEFAULT;
  }
}

uintptr_t pn_hashcode(void *object)
{
  if (!object) return 0;

  const pn_class_t *clazz = pni_head(object)->clazz;

  if (clazz->hashcode) {
    return clazz->hashcode(object);
  } else {
    return (uintptr_t) object;
  }
}

intptr_t pn_compare(void *a, void *b)
{
  if (a == b) return 0;

  if (a && b) {
    const pn_class_t *clazz = pni_head(a)->clazz;
    if (clazz->compare) {
      return clazz->compare(a, b);
    }
  }
  return (intptr_t) a - (intptr_t) b;
}

bool pn_equals(void *a, void *b)
{
  return !pn_compare(a, b);
}

int pn_inspect(void *object, pn_string_t *dst)
{
  if (!pn_string_get(dst)) {
    pn_string_set(dst, "");
  }

  if (!object) {
    return pn_string_addf(dst, "pn_object<%p>", object);
  }

  const pn_class_t *clazz = pni_head(object)->clazz;

  if (clazz->inspect) {
    char buf[1024];
    pn_fixed_string_t str = pn_fixed_string(buf, sizeof(buf));
    clazz->inspect(object, &str);
    return pn_string_setn(dst, buf, str.position);
  }

  const char *name = clazz->name ? clazz->name : "<anon>";
  return pn_string_addf(dst, "%s<%p>", name, object);
}

void pn_finspect(void *object, pn_fixed_string_t *dst)
{
  if (!object) {
    pn_fixed_string_addf(dst, "pn_object<%p>", object);
    return;
  }

  const pn_class_t *clazz = pni_head(object)->clazz;

  if (clazz->inspect) {
    clazz->inspect(object, dst);
    return;
  }

  const char *name = clazz->name ? clazz->name : "<anon>";
  pn_fixed_string_addf(dst, "%s<%p>", name, object);
  return;
}

char *pn_tostring(void *object)
{
  char buf[1024];
  pn_fixed_string_t s = pn_fixed_string(buf, sizeof(buf));
  pn_finspect(object, &s);
  pn_fixed_string_terminate(&s);

  int l = s.position+1; // include final null
  char *r = malloc(l);
  strncpy(r, buf, l);
  return r;
}

#define pn_weakref_new NULL
#define pn_weakref_initialize NULL
#define pn_weakref_finalize NULL
#define pn_weakref_free NULL

#define pn_weakref_incref pn_void_incref
#define pn_weakref_decref pn_void_decref
#define pn_weakref_refcount pn_void_refcount

#define pn_weakref_hashcode pn_hashcode
#define pn_weakref_compare pn_compare
#define pn_weakref_inspect pn_finspect

const pn_class_t PN_WEAKREF[] = {PN_METACLASS(pn_weakref)};

#define CID_pn_strongref CID_pn_object
#define pn_strongref_new NULL

void pn_strongref_initialize(void *object) {
  const pn_class_t *clazz = pni_head(object)->clazz;
  if (clazz->initialize) {
    clazz->initialize(object);
  }
}

void pn_strongref_finalize(void *object) {
  const pn_class_t *clazz = pni_head(object)->clazz;
  if (clazz->finalize) {
    clazz->finalize(object);
  }
}

void pn_strongref_free(void *object)
{
  const pn_class_t *clazz = pni_head(object)->clazz;
  pni_class_free(clazz, object);
}

void pn_strongref_incref(void *object)
{
  const pn_class_t *clazz = pni_head(object)->clazz;
  pni_class_incref(clazz, object);
}
void pn_strongref_decref(void *object)
{
  const pn_class_t *clazz = pni_head(object)->clazz;
  pni_class_decref(clazz, object);
}
int pn_strongref_refcount(void *object)
{
  const pn_class_t *clazz = pni_head(object)->clazz;
  return pni_class_refcount(clazz, object);
}

#define pn_strongref_hashcode pn_hashcode
#define pn_strongref_compare pn_compare
#define pn_strongref_inspect pn_finspect

static const pn_class_t PN_OBJECT_S = PN_METACLASS(pn_strongref);
const pn_class_t *PN_OBJECT = &PN_OBJECT_S;
