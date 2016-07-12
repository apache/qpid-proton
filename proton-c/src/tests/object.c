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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proton/object.h>

#define assert(E) ((E) ? 0 : (abort(), 0))

static char mem;
static void *END = &mem;

static pn_list_t *build_list(size_t capacity, ...)
{
  pn_list_t *result = pn_list(PN_OBJECT, capacity);
  va_list ap;

  va_start(ap, capacity);
  while (true) {
    void *arg = va_arg(ap, void *);
    if (arg == END) {
      break;
    }

    pn_list_add(result, arg);
    pn_class_decref(PN_OBJECT, arg);
  }
  va_end(ap);

  return result;
}

static pn_map_t *build_map(size_t capacity, float load_factor, ...)
{
  pn_map_t *result = pn_map(PN_OBJECT, PN_OBJECT, capacity, load_factor);
  va_list ap;

  void *prev = NULL;

  va_start(ap, load_factor);
  int count = 0;
  while (true) {
    void *arg = va_arg(ap, void *);
    bool last = arg == END;
    if (arg == END) {
      arg = NULL;
    }

    if (count % 2) {
      pn_map_put(result, prev, arg);
      pn_class_decref(PN_OBJECT, prev);
      pn_class_decref(PN_OBJECT, arg);
    } else {
      prev = arg;
    }

    if (last) {
      break;
    }

    count++;
  }
  va_end(ap);

  return result;
}

static void noop(void *o) {}
static uintptr_t zero(void *o) { return 0; }
static intptr_t delta(void *a, void *b) { return (uintptr_t) b - (uintptr_t) a; }

#define CID_noop CID_pn_object
#define noop_initialize noop
#define noop_finalize noop
#define noop_hashcode zero
#define noop_compare delta
#define noop_inspect NULL

static const pn_class_t noop_class = PN_CLASS(noop);

static void test_class(const pn_class_t *clazz, size_t size)
{
  void *a = pn_class_new(clazz, size);
  void *b = pn_class_new(clazz, size);

  assert(!pn_class_equals(clazz, a, b));
  assert(pn_class_equals(clazz, a, a));
  assert(pn_class_equals(clazz, b, b));
  assert(!pn_class_equals(clazz, a, NULL));
  assert(!pn_class_equals(clazz, NULL, a));

  int rca = pn_class_refcount(clazz, a);
  int rcb = pn_class_refcount(clazz, b);

  assert(rca == -1 || rca == 1);
  assert(rcb == -1 || rcb == 1);

  pn_class_incref(clazz, a);

  rca = pn_class_refcount(clazz, a);
  assert(rca == -1 || rca == 2);

  pn_class_decref(clazz, a);

  rca = pn_class_refcount(clazz, a);
  assert(rca == -1 || rca == 1);

  pn_class_free(clazz, a);
  pn_class_free(clazz, b);
}

static void test_new(size_t size, const pn_class_t *clazz)
{
  void *obj = pn_class_new(clazz, size);
  assert(obj);
  assert(pn_class_refcount(PN_OBJECT, obj) == 1);
  assert(pn_class(obj) == clazz);
  char *bytes = (char *) obj;
  for (size_t i = 0; i < size; i++) {
    // touch everything for valgrind
    bytes[i] = i;
  }
  pn_free(obj);
}

static void finalizer(void *object)
{
  int **called = (int **) object;
  (**called)++;
}

#define CID_finalizer CID_pn_object
#define finalizer_initialize NULL
#define finalizer_finalize finalizer
#define finalizer_hashcode NULL
#define finalizer_compare NULL
#define finalizer_inspect NULL

static void test_finalize(void)
{
  static pn_class_t clazz = PN_CLASS(finalizer);

  int **obj = (int **) pn_class_new(&clazz, sizeof(int *));
  assert(obj);

  int called = 0;
  *obj = &called;
  pn_free(obj);

  assert(called == 1);
}

static void test_free(void)
{
  // just to make sure it doesn't seg fault or anything
  pn_free(NULL);
}

static uintptr_t hashcode(void *obj) { return (uintptr_t) obj; }

#define CID_hashcode CID_pn_object
#define hashcode_initialize NULL
#define hashcode_finalize NULL
#define hashcode_compare NULL
#define hashcode_hashcode hashcode
#define hashcode_inspect NULL

static void test_hashcode(void)
{
  static pn_class_t clazz = PN_CLASS(hashcode);
  void *obj = pn_class_new(&clazz, 0);
  assert(obj);
  assert(pn_hashcode(obj) == (uintptr_t) obj);
  assert(pn_hashcode(NULL) == 0);
  pn_free(obj);
}

#define CID_compare CID_pn_object
#define compare_initialize NULL
#define compare_finalize NULL
#define compare_compare delta
#define compare_hashcode NULL
#define compare_inspect NULL

static void test_compare(void)
{
  static pn_class_t clazz = PN_CLASS(compare);

  void *a = pn_class_new(&clazz, 0);
  assert(a);
  void *b = pn_class_new(&clazz, 0);
  assert(b);

  assert(pn_compare(a, b));
  assert(!pn_equals(a, b));
  assert(!pn_compare(a, a));
  assert(pn_equals(a, a));
  assert(!pn_compare(b, b));
  assert(pn_equals(b, b));
  assert(pn_compare(a, b) == (intptr_t) ((uintptr_t) b - (uintptr_t) a));

  assert(pn_compare(NULL, b));
  assert(!pn_equals(NULL, b));

  assert(pn_compare(a, NULL));
  assert(!pn_equals(a, NULL));

  assert(!pn_compare(NULL, NULL));
  assert(pn_equals(NULL, NULL));

  pn_free(a);
  pn_free(b);
}

static void test_refcounting(int refs)
{
  void *obj = pn_class_new(PN_OBJECT, 0);

  assert(pn_refcount(obj) == 1);

  for (int i = 0; i < refs; i++) {
    pn_incref(obj);
    assert(pn_refcount(obj) == i + 2);
  }

  assert(pn_refcount(obj) == refs + 1);

  for (int i = 0; i < refs; i++) {
    pn_decref(obj);
    assert(pn_refcount(obj) == refs - i);
  }

  assert(pn_refcount(obj) == 1);

  pn_free(obj);
}

static void test_list(size_t capacity)
{
  pn_list_t *list = pn_list(PN_WEAKREF, 0);
  assert(pn_list_size(list) == 0);
  assert(!pn_list_add(list, (void *) 0));
  assert(!pn_list_add(list, (void *) 1));
  assert(!pn_list_add(list, (void *) 2));
  assert(!pn_list_add(list, (void *) 3));
  assert(pn_list_get(list, 0) == (void *) 0);
  assert(pn_list_get(list, 1) == (void *) 1);
  assert(pn_list_get(list, 2) == (void *) 2);
  assert(pn_list_get(list, 3) == (void *) 3);
  assert(pn_list_size(list) == 4);
  pn_list_del(list, 1, 2);
  assert(pn_list_size(list) == 2);
  assert(pn_list_get(list, 0) == (void *) 0);
  assert(pn_list_get(list, 1) == (void *) 3);
  pn_decref(list);
}

static void test_list_refcount(size_t capacity)
{
  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);
  void *four = pn_class_new(PN_OBJECT, 0);

  pn_list_t *list = pn_list(PN_OBJECT, 0);
  assert(!pn_list_add(list, one));
  assert(!pn_list_add(list, two));
  assert(!pn_list_add(list, three));
  assert(!pn_list_add(list, four));
  assert(pn_list_get(list, 0) == one);
  assert(pn_list_get(list, 1) == two);
  assert(pn_list_get(list, 2) == three);
  assert(pn_list_get(list, 3) == four);
  assert(pn_list_size(list) == 4);

  assert(pn_refcount(one) == 2);
  assert(pn_refcount(two) == 2);
  assert(pn_refcount(three) == 2);
  assert(pn_refcount(four) == 2);

  pn_list_del(list, 1, 2);
  assert(pn_list_size(list) == 2);

  assert(pn_refcount(one) == 2);
  assert(pn_refcount(two) == 1);
  assert(pn_refcount(three) == 1);
  assert(pn_refcount(four) == 2);

  assert(pn_list_get(list, 0) == one);
  assert(pn_list_get(list, 1) == four);

  assert(!pn_list_add(list, one));

  assert(pn_list_size(list) == 3);
  assert(pn_refcount(one) == 3);

  pn_decref(list);

  assert(pn_refcount(one) == 1);
  assert(pn_refcount(two) == 1);
  assert(pn_refcount(three) == 1);
  assert(pn_refcount(four) == 1);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);
  pn_decref(four);
}

static void check_list_index(pn_list_t *list, void *value, ssize_t idx)
{
  assert(pn_list_index(list, value) == idx);
}

static void test_list_index(void)
{
  pn_list_t *l = pn_list(PN_WEAKREF, 0);
  void *one = pn_string("one");
  void *two = pn_string("two");
  void *three = pn_string("three");
  void *dup1 = pn_string("dup");
  void *dup2 = pn_string("dup");
  void *last = pn_string("last");

  pn_list_add(l, one);
  pn_list_add(l, two);
  pn_list_add(l, three);
  pn_list_add(l, dup1);
  pn_list_add(l, dup2);
  pn_list_add(l, last);

  check_list_index(l, one, 0);
  check_list_index(l, two, 1);
  check_list_index(l, three, 2);
  check_list_index(l, dup1, 3);
  check_list_index(l, dup2, 3);
  check_list_index(l, last, 5);

  void *nonexistent = pn_string("nonexistent");

  check_list_index(l, nonexistent, -1);

  pn_free(l);
  pn_free(one);
  pn_free(two);
  pn_free(three);
  pn_free(dup1);
  pn_free(dup2);
  pn_free(last);
  pn_free(nonexistent);
}

static bool pn_strequals(const char *a, const char *b)
{
  return !strcmp(a, b);
}

static void test_build_list(void)
{
  pn_list_t *l = build_list(0,
                            pn_string("one"),
                            pn_string("two"),
                            pn_string("three"),
                            END);

  assert(pn_list_size(l) == 3);

  assert(pn_strequals(pn_string_get((pn_string_t *) pn_list_get(l, 0)),
                      "one"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_list_get(l, 1)),
                      "two"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_list_get(l, 2)),
                      "three"));

  pn_free(l);
}

static void test_build_map(void)
{
  pn_map_t *m = build_map(0, 0.75,
                          pn_string("key"),
                          pn_string("value"),
                          pn_string("key2"),
                          pn_string("value2"),
                          END);

  assert(pn_map_size(m) == 2);

  pn_string_t *key = pn_string(NULL);

  pn_string_set(key, "key");
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_map_get(m, key)),
                      "value"));
  pn_string_set(key, "key2");
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_map_get(m, key)),
                      "value2"));

  pn_free(m);
  pn_free(key);
}

static void test_build_map_odd(void)
{
  pn_map_t *m = build_map(0, 0.75,
                          pn_string("key"),
                          pn_string("value"),
                          pn_string("key2"),
                          pn_string("value2"),
                          pn_string("key3"),
                          END);

  assert(pn_map_size(m) == 3);

  pn_string_t *key = pn_string(NULL);

  pn_string_set(key, "key");
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_map_get(m, key)),
                      "value"));
  pn_string_set(key, "key2");
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_map_get(m, key)),
                      "value2"));
  pn_string_set(key, "key3");
  assert(pn_map_get(m, key) == NULL);

  pn_free(m);
  pn_free(key);
}

static void test_map(void)
{
  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);

  pn_map_t *map = pn_map(PN_OBJECT, PN_OBJECT, 4, 0.75);
  assert(pn_map_size(map) == 0);

  pn_string_t *key = pn_string("key");
  pn_string_t *dup = pn_string("key");
  pn_string_t *key1 = pn_string("key1");
  pn_string_t *key2 = pn_string("key2");

  assert(!pn_map_put(map, key, one));
  assert(pn_map_size(map) == 1);
  assert(!pn_map_put(map, key1, two));
  assert(pn_map_size(map) == 2);
  assert(!pn_map_put(map, key2, three));
  assert(pn_map_size(map) == 3);

  assert(pn_map_get(map, dup) == one);

  assert(!pn_map_put(map, dup, one));
  assert(pn_map_size(map) == 3);

  assert(!pn_map_put(map, dup, two));
  assert(pn_map_size(map) == 3);
  assert(pn_map_get(map, dup) == two);

  assert(pn_refcount(key) == 2);
  assert(pn_refcount(dup) == 1);
  assert(pn_refcount(key1) == 2);
  assert(pn_refcount(key2) == 2);

  assert(pn_refcount(one) == 1);
  assert(pn_refcount(two) == 3);
  assert(pn_refcount(three) == 2);

  pn_map_del(map, key1);
  assert(pn_map_size(map) == 2);

  assert(pn_refcount(key) == 2);
  assert(pn_refcount(dup) == 1);
  assert(pn_refcount(key1) == 1);
  assert(pn_refcount(key2) == 2);

  assert(pn_refcount(one) == 1);
  assert(pn_refcount(two) == 2);
  assert(pn_refcount(three) == 2);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);

  pn_decref(key);
  pn_decref(dup);
  pn_decref(key1);
  pn_decref(key2);

  pn_decref(map);
}

static void test_hash(void)
{
  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);

  pn_hash_t *hash = pn_hash(PN_OBJECT, 4, 0.75);
  pn_hash_put(hash, 0, NULL);
  pn_hash_put(hash, 1, one);
  pn_hash_put(hash, 2, two);
  pn_hash_put(hash, 3, three);
  pn_hash_put(hash, 4, one);
  pn_hash_put(hash, 5, two);
  pn_hash_put(hash, 6, three);
  pn_hash_put(hash, 7, one);
  pn_hash_put(hash, 8, two);
  pn_hash_put(hash, 9, three);
  pn_hash_put(hash, 10, one);
  pn_hash_put(hash, 11, two);
  pn_hash_put(hash, 12, three);
  pn_hash_put(hash, 18, one);

  assert(pn_hash_get(hash, 2) == two);
  assert(pn_hash_get(hash, 5) == two);
  assert(pn_hash_get(hash, 18) == one);
  assert(pn_hash_get(hash, 0) == NULL);

  assert(pn_hash_size(hash) == 14);

  pn_hash_del(hash, 5);
  assert(pn_hash_get(hash, 5) == NULL);
  assert(pn_hash_size(hash) == 13);
  pn_hash_del(hash, 18);
  assert(pn_hash_get(hash, 18) == NULL);
  assert(pn_hash_size(hash) == 12);

  pn_decref(hash);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);
}


// collider class: all objects have same hash, no two objects compare equal
static intptr_t collider_compare(void *a, void *b)
{
  if (a == b) return 0;
  return (a > b) ? 1 : -1;
}

static uintptr_t collider_hashcode(void *obj)
{
  return 23;
}

#define CID_collider CID_pn_object
#define collider_initialize NULL
#define collider_finalize NULL
#define collider_inspect NULL

static void test_map_links(void)
{
  const pn_class_t collider_clazz = PN_CLASS(collider);
  void *keys[3];
  for (int i = 0; i < 3; i++)
    keys[i] = pn_class_new(&collider_clazz, 0);

  // test deleting a head, middle link, tail

  for (int delete_idx=0; delete_idx < 3; delete_idx++) {
    pn_map_t *map = pn_map(PN_WEAKREF, PN_WEAKREF, 0, 0.75);
    // create a chain of entries that have same head (from identical key hashcode)
    for (int i = 0; i < 3; i++) {
      pn_map_put(map, keys[i], keys[i]);
    }
    pn_map_del(map, keys[delete_idx]);
    for (int i = 0; i < 3; i++) {
      void *value = (i == delete_idx) ? NULL : keys[i];
      assert (pn_map_get(map, keys[i]) == value);
    }
    pn_free(map);
  }
  for (int i = 0; i < 3; i++)
    pn_free(keys[i]);
}


static bool equals(const char *a, const char *b)
{
  if (a == NULL && b == NULL) {
    return true;
  }

  if (a == NULL || b == NULL) {
    return false;
  }

  return !strcmp(a, b);
}

static void test_string(const char *value)
{
  size_t size = value ? strlen(value) : 0;

  pn_string_t *str = pn_string(value);
  assert(equals(pn_string_get(str), value));
  assert(pn_string_size(str) == size);

  pn_string_t *strn = pn_stringn(value, size);
  assert(equals(pn_string_get(strn), value));
  assert(pn_string_size(strn) == size);

  pn_string_t *strset = pn_string(NULL);
  pn_string_set(strset, value);
  assert(equals(pn_string_get(strset), value));
  assert(pn_string_size(strset) == size);

  pn_string_t *strsetn = pn_string(NULL);
  pn_string_setn(strsetn, value, size);
  assert(equals(pn_string_get(strsetn), value));
  assert(pn_string_size(strsetn) == size);

  assert(pn_hashcode(str) == pn_hashcode(strn));
  assert(pn_hashcode(str) == pn_hashcode(strset));
  assert(pn_hashcode(str) == pn_hashcode(strsetn));

  assert(!pn_compare(str, str));
  assert(!pn_compare(str, strn));
  assert(!pn_compare(str, strset));
  assert(!pn_compare(str, strsetn));

  pn_free(str);
  pn_free(strn);
  pn_free(strset);
  pn_free(strsetn);
}

static void test_stringn(const char *value, size_t size)
{
  pn_string_t *strn = pn_stringn(value, size);
  assert(equals(pn_string_get(strn), value));
  assert(pn_string_size(strn) == size);

  pn_string_t *strsetn = pn_string(NULL);
  pn_string_setn(strsetn, value, size);
  assert(equals(pn_string_get(strsetn), value));
  assert(pn_string_size(strsetn) == size);

  assert(pn_hashcode(strn) == pn_hashcode(strsetn));
  assert(!pn_compare(strn, strsetn));

  pn_free(strn);
  pn_free(strsetn);
}

static void test_string_format(void)
{
  pn_string_t *str = pn_string("");
  assert(str);
  int err = pn_string_format(str, "%s", "this is a string that should be long "
                             "enough to force growth but just in case we'll "
                             "tack this other really long string on for the "
                             "heck of it");
  assert(err == 0);
  pn_free(str);
}

static void test_string_addf(void)
{
  pn_string_t *str = pn_string("hello ");
  assert(str);
  int err = pn_string_addf(str, "%s", "this is a string that should be long "
                           "enough to force growth but just in case we'll "
                           "tack this other really long string on for the "
                           "heck of it");
  assert(err == 0);
  pn_free(str);
}

static void test_map_iteration(int n)
{
  pn_list_t *pairs = pn_list(PN_OBJECT, 2*n);
  for (int i = 0; i < n; i++) {
    void *key = pn_class_new(PN_OBJECT, 0);
    void *value = pn_class_new(PN_OBJECT, 0);
    pn_list_add(pairs, key);
    pn_list_add(pairs, value);
    pn_decref(key);
    pn_decref(value);
  }

  pn_map_t *map = pn_map(PN_OBJECT, PN_OBJECT, 0, 0.75);

  assert(pn_map_head(map) == 0);

  for (int i = 0; i < n; i++) {
    pn_map_put(map, pn_list_get(pairs, 2*i), pn_list_get(pairs, 2*i + 1));
  }

  for (pn_handle_t entry = pn_map_head(map); entry; entry = pn_map_next(map, entry))
  {
    void *key = pn_map_key(map, entry);
    void *value = pn_map_value(map, entry);
    ssize_t idx = pn_list_index(pairs, key);
    assert(idx >= 0);

    assert(pn_list_get(pairs, idx) == key);
    assert(pn_list_get(pairs, idx + 1) == value);

    pn_list_del(pairs, idx, 2);
  }

  assert(pn_list_size(pairs) == 0);

  pn_decref(map);
  pn_decref(pairs);
}

void test_inspect(void *o, const char *expected)
{
  pn_string_t *dst = pn_string(NULL);
  pn_inspect(o, dst);
  assert(pn_strequals(pn_string_get(dst), expected));
  pn_free(dst);
}

void test_list_inspect(void)
{
  pn_list_t *l = build_list(0, END);
  test_inspect(l, "[]");
  pn_free(l);

  l = build_list(0, pn_string("one"), END);
  test_inspect(l, "[\"one\"]");
  pn_free(l);

  l = build_list(0,
                 pn_string("one"),
                 pn_string("two"),
                 END);
  test_inspect(l, "[\"one\", \"two\"]");
  pn_free(l);

  l = build_list(0,
                 pn_string("one"),
                 pn_string("two"),
                 pn_string("three"),
                 END);
  test_inspect(l, "[\"one\", \"two\", \"three\"]");
  pn_free(l);
}

void test_map_inspect(void)
{
  // note that when there is more than one entry in a map, the order
  // of the entries is dependent on the hashes involved, it will be
  // deterministic though
  pn_map_t *m = build_map(0, 0.75, END);
  test_inspect(m, "{}");
  pn_free(m);

  m = build_map(0, 0.75,
                pn_string("key"), pn_string("value"),
                END);
  test_inspect(m, "{\"key\": \"value\"}");
  pn_free(m);

  m = build_map(0, 0.75,
                pn_string("k1"), pn_string("v1"),
                pn_string("k2"), pn_string("v2"),
                END);
  test_inspect(m, "{\"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);

  m = build_map(0, 0.75,
                pn_string("k1"), pn_string("v1"),
                pn_string("k2"), pn_string("v2"),
                pn_string("k3"), pn_string("v3"),
                END);
  test_inspect(m, "{\"k3\": \"v3\", \"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);
}

void test_map_coalesced_chain(void)
{
  pn_hash_t *map = pn_hash(PN_OBJECT, 16, 0.75);
  pn_string_t *values[9] = {
      pn_string("a"),
      pn_string("b"),
      pn_string("c"),
      pn_string("d"),
      pn_string("e"),
      pn_string("f"),
      pn_string("g"),
      pn_string("h"),
      pn_string("i")
  };
  //add some items:
  pn_hash_put(map, 1, values[0]);
  pn_hash_put(map, 2, values[1]);
  pn_hash_put(map, 3, values[2]);

  //use up all non-addressable elements:
  pn_hash_put(map, 14, values[3]);
  pn_hash_put(map, 15, values[4]);
  pn_hash_put(map, 16, values[5]);

  //use an addressable element for a key that doesn't map to it:
  pn_hash_put(map, 4, values[6]);
  pn_hash_put(map, 17, values[7]);
  assert(pn_hash_size(map) == 8);

  //free up one non-addressable entry:
  pn_hash_del(map, 16);
  assert(pn_hash_get(map, 16) == NULL);
  assert(pn_hash_size(map) == 7);

  //add a key whose addressable slot is already taken (by 17),
  //generating a coalesced chain:
  pn_hash_put(map, 12, values[8]);

  //remove an entry from the coalesced chain:
  pn_hash_del(map, 4);
  assert(pn_hash_get(map, 4) == NULL);

  //test lookup of all entries:
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 1)), "a"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 2)), "b"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 3)), "c"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 14)), "d"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 15)), "e"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 17)), "h"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 12)), "i"));
  assert(pn_hash_size(map) == 7);

  //cleanup:
  for (pn_handle_t i = pn_hash_head(map); i; i = pn_hash_head(map)) {
    pn_hash_del(map, pn_hash_key(map, i));
  }
  assert(pn_hash_size(map) == 0);

  for (size_t i = 0; i < 9; ++i) {
      pn_free(values[i]);
  }
  pn_free(map);
}

void test_map_coalesced_chain2(void)
{
  pn_hash_t *map = pn_hash(PN_OBJECT, 16, 0.75);
  pn_string_t *values[10] = {
      pn_string("a"),
      pn_string("b"),
      pn_string("c"),
      pn_string("d"),
      pn_string("e"),
      pn_string("f"),
      pn_string("g"),
      pn_string("h"),
      pn_string("i"),
      pn_string("j")
  };
  //add some items:
  pn_hash_put(map, 1, values[0]);//a
  pn_hash_put(map, 2, values[1]);//b
  pn_hash_put(map, 3, values[2]);//c

  //use up all non-addressable elements:
  pn_hash_put(map, 14, values[3]);//d
  pn_hash_put(map, 15, values[4]);//e
  pn_hash_put(map, 16, values[5]);//f
  //take slot from addressable region
  pn_hash_put(map, 29, values[6]);//g, goes into slot 12

  //free up one non-addressable entry:
  pn_hash_del(map, 14);
  assert(pn_hash_get(map, 14) == NULL);

  //add a key whose addressable slot is already taken (by 29),
  //generating a coalesced chain:
  pn_hash_put(map, 12, values[7]);//h
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 12)), "h"));
  //delete from tail of coalesced chain:
  pn_hash_del(map, 12);
  assert(pn_hash_get(map, 12) == NULL);

  //extend chain into cellar again, then coalesce again extending back
  //into addressable region
  pn_hash_put(map, 42, values[8]);//i
  pn_hash_put(map, 25, values[9]);//j
  //delete entry from coalesced chain, where next element in chain is
  //in cellar:
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 29)), "g"));
  pn_hash_del(map, 29);

  //test lookup of all entries:
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 1)), "a"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 2)), "b"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 3)), "c"));
  //d was deleted
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 15)), "e"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 16)), "f"));
  //g was deleted, h was deleted
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 42)), "i"));
  assert(pn_strequals(pn_string_get((pn_string_t *) pn_hash_get(map, 25)), "j"));
  assert(pn_hash_size(map) == 7);

  //cleanup:
  for (pn_handle_t i = pn_hash_head(map); i; i = pn_hash_head(map)) {
    pn_hash_del(map, pn_hash_key(map, i));
  }
  assert(pn_hash_size(map) == 0);

  for (size_t i = 0; i < 10; ++i) {
      pn_free(values[i]);
  }
  pn_free(map);
}

void test_list_compare(void)
{
  pn_list_t *a = pn_list(PN_OBJECT, 0);
  pn_list_t *b = pn_list(PN_OBJECT, 0);

  assert(pn_equals(a, b));

  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);

  pn_list_add(a, one);
  assert(!pn_equals(a, b));
  pn_list_add(b, one);
  assert(pn_equals(a, b));

  pn_list_add(b, two);
  assert(!pn_equals(a, b));
  pn_list_add(a, two);
  assert(pn_equals(a, b));

  pn_list_add(a, three);
  assert(!pn_equals(a, b));
  pn_list_add(b, three);
  assert(pn_equals(a, b));

  pn_free(a); pn_free(b);
  pn_free(one); pn_free(two); pn_free(three);
}

typedef struct {
  pn_list_t *list;
  size_t index;
} pn_it_state_t;

static void *pn_it_next(void *state) {
  pn_it_state_t *it = (pn_it_state_t *) state;
  if (it->index < pn_list_size(it->list)) {
    return pn_list_get(it->list, it->index++);
  } else {
    return NULL;
  }
}

void test_iterator(void)
{
  pn_list_t *list = build_list(0,
                               pn_string("one"),
                               pn_string("two"),
                               pn_string("three"),
                               pn_string("four"),
                               END);
  pn_iterator_t *it = pn_iterator();
  pn_it_state_t *state = (pn_it_state_t *) pn_iterator_start
    (it, pn_it_next, sizeof(pn_it_state_t));
  state->list = list;
  state->index = 0;

  void *obj;
  int index = 0;
  while ((obj = pn_iterator_next(it))) {
    assert(obj == pn_list_get(list, index));
    ++index;
  }
  assert(index == 4);

  pn_free(list);
  pn_free(it);
}

void test_heap(int seed, int size)
{
  srand(seed);
  pn_list_t *list = pn_list(PN_VOID, 0);

  intptr_t min = 0;
  intptr_t max = 0;

  for (int i = 0; i < size; i++) {
    intptr_t r = rand();

    if (i == 0) {
      min = r;
      max = r;
    } else {
      if (r < min) {
        min = r;
      }
      if (r > max) {
        max = r;
      }
    }

    pn_list_minpush(list, (void *) r);
  }

  intptr_t prev = (intptr_t) pn_list_minpop(list);
  assert(prev == min);
  assert(pn_list_size(list) == (size_t)(size - 1));
  int count = 0;
  while (pn_list_size(list)) {
    intptr_t r = (intptr_t) pn_list_minpop(list);
    assert(r >= prev);
    prev = r;
    count++;
  }
  assert(count == size - 1);
  assert(prev == max);

  pn_free(list);
}

int main(int argc, char **argv)
{
  for (size_t i = 0; i < 128; i++) {
    test_class(PN_OBJECT, i);
    test_class(PN_VOID, i);
    test_class(&noop_class, i);
  }

  for (size_t i = 0; i < 128; i++) {
    test_new(i, PN_OBJECT);
    test_new(i, &noop_class);
  }

  test_finalize();
  test_free();
  test_hashcode();
  test_compare();

  for (int i = 0; i < 1024; i++) {
    test_refcounting(i);
  }

  for (size_t i = 0; i < 4; i++) {
    test_list(i);
  }

  for (size_t i = 0; i < 4; i++) {
    test_list_refcount(i);
  }

  test_list_index();

  test_map();
  test_map_links();

  test_hash();

  test_string(NULL);
  test_string("");
  test_string("this is a test");
  test_string("012345678910111213151617181920212223242526272829303132333435363"
              "738394041424344454647484950515253545556575859606162636465666768");
  test_string("this has an embedded \000 in it");
  test_stringn("this has an embedded \000 in it", 28);

  test_string_format();
  test_string_addf();

  test_build_list();
  test_build_map();
  test_build_map_odd();

  for (int i = 0; i < 64; i++)
  {
    test_map_iteration(i);
  }

  test_list_inspect();
  test_map_inspect();
  test_list_compare();
  test_iterator();
  for (int seed = 0; seed < 64; seed++) {
    for (int size = 1; size <= 64; size++) {
      test_heap(seed, size);
    }
  }

  test_map_coalesced_chain();
  test_map_coalesced_chain2();

  return 0;
}
