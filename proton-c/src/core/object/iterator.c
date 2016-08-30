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

struct pn_iterator_t {
  pn_iterator_next_t next;
  size_t size;
  void *state;
};

static void pn_iterator_initialize(void *object)
{
  pn_iterator_t *it = (pn_iterator_t *) object;
  it->next = NULL;
  it->size = 0;
  it->state = NULL;
}

static void pn_iterator_finalize(void *object)
{
  pn_iterator_t *it = (pn_iterator_t *) object;
  free(it->state);
}

#define CID_pn_iterator CID_pn_object
#define pn_iterator_hashcode NULL
#define pn_iterator_compare NULL
#define pn_iterator_inspect NULL

pn_iterator_t *pn_iterator()
{
  static const pn_class_t clazz = PN_CLASS(pn_iterator);
  pn_iterator_t *it = (pn_iterator_t *) pn_class_new(&clazz, sizeof(pn_iterator_t));
  return it;
}

void  *pn_iterator_start(pn_iterator_t *iterator, pn_iterator_next_t next,
                         size_t size) {
  assert(iterator);
  assert(next);
  iterator->next = next;
  if (iterator->size < size) {
    iterator->state = realloc(iterator->state, size);
  }
  return iterator->state;
}

void *pn_iterator_next(pn_iterator_t *iterator) {
  assert(iterator);
  if (iterator->next) {
    void *result = iterator->next(iterator->state);
    if (!result) iterator->next = NULL;
    return result;
  } else {
    return NULL;
  }
}
