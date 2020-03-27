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

#include <stddef.h>
#include <assert.h>

typedef struct {
  pn_handle_t key;
  const pn_class_t *clazz;
  void *value;
} pni_field_t;

struct pn_record_t {
  size_t size;
  size_t capacity;
  pni_field_t *fields;
};

static void pn_record_initialize(void *object)
{
  pn_record_t *record = (pn_record_t *) object;
  record->size = 0;
  record->capacity = 0;
  record->fields = NULL;
}

static void pn_record_finalize(void *object)
{
  pn_record_t *record = (pn_record_t *) object;
  for (size_t i = 0; i < record->size; i++) {
    pni_field_t *v = &record->fields[i];
    pn_class_decref(v->clazz, v->value);
  }
  pni_mem_subdeallocate(pn_class(record), record, record->fields);
}

#define pn_record_hashcode NULL
#define pn_record_compare NULL
#define pn_record_inspect NULL

pn_record_t *pn_record(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_record);
  pn_record_t *record = (pn_record_t *) pn_class_new(&clazz, sizeof(pn_record_t));
  pn_record_def(record, PN_LEGCTX, PN_VOID);
  return record;
}

static pni_field_t *pni_record_find(pn_record_t *record, pn_handle_t key) {
  for (size_t i = 0; i < record->size; i++) {
    pni_field_t *field = &record->fields[i];
    if (field->key == key) {
      return field;
    }
  }
  return NULL;
}

static pni_field_t *pni_record_create(pn_record_t *record) {
  record->size++;
  if (record->size > record->capacity) {
    record->fields = (pni_field_t *) pni_mem_subreallocate(pn_class(record), record, record->fields, record->size * sizeof(pni_field_t));
    record->capacity = record->size;
  }
  pni_field_t *field = &record->fields[record->size - 1];
  field->key = 0;
  field->clazz = NULL;
  field->value = NULL;
  return field;
}

void pn_record_def(pn_record_t *record, pn_handle_t key, const pn_class_t *clazz)
{
  assert(record);
  assert(clazz);

  pni_field_t *field = pni_record_find(record, key);
  if (field) {
    assert(field->clazz == clazz);
  } else {
    field = pni_record_create(record);
    field->key = key;
    field->clazz = clazz;
  }
}

bool pn_record_has(pn_record_t *record, pn_handle_t key)
{
  assert(record);
  pni_field_t *field = pni_record_find(record, key);
  if (field) {
    return true;
  } else {
    return false;
  }
}

void *pn_record_get(pn_record_t *record, pn_handle_t key)
{
  assert(record);
  pni_field_t *field = pni_record_find(record, key);
  if (field) {
    return field->value;
  } else {
    return NULL;
  }
}

void pn_record_set(pn_record_t *record, pn_handle_t key, void *value)
{
  assert(record);

  pni_field_t *field = pni_record_find(record, key);
  if (field) {
    void *old = field->value;
    field->value = value;
    pn_class_incref(field->clazz, value);
    pn_class_decref(field->clazz, old);
  }
}

void pn_record_clear(pn_record_t *record)
{
  assert(record);
  for (size_t i = 0; i < record->size; i++) {
    pni_field_t *field = &record->fields[i];
    pn_class_decref(field->clazz, field->value);
    field->key = 0;
    field->clazz = NULL;
    field->value = NULL;
  }
  record->size = 0;
  pn_record_def(record, PN_LEGCTX, PN_VOID);
}
