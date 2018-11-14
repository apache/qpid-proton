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

#include "./pn_test.hpp"

#include <proton/object.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using Catch::Matchers::Equals;

static char mem;
static void *END = &mem;

static pn_list_t *build_list(size_t capacity, ...) {
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

static pn_map_t *build_map(float load_factor, size_t capacity, ...) {
  pn_map_t *result = pn_map(PN_OBJECT, PN_OBJECT, capacity, load_factor);
  va_list ap;

  void *prev = NULL;

  va_start(ap, capacity);
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
static intptr_t delta(void *a, void *b) { return (uintptr_t)b - (uintptr_t)a; }

#define CID_noop CID_pn_object
#define noop_initialize noop
#define noop_finalize noop
#define noop_hashcode zero
#define noop_compare delta
#define noop_inspect NULL

static const pn_class_t noop_class = PN_CLASS(noop);

static void test_class(const pn_class_t *clazz, size_t size) {
  INFO("class=" << pn_class_name(clazz) << " size=" << size);
  void *a = pn_class_new(clazz, size);
  void *b = pn_class_new(clazz, size);

  CHECK(!pn_class_equals(clazz, a, b));
  CHECK(pn_class_equals(clazz, a, a));
  CHECK(pn_class_equals(clazz, b, b));
  CHECK(!pn_class_equals(clazz, a, NULL));
  CHECK(!pn_class_equals(clazz, NULL, a));

  int rca = pn_class_refcount(clazz, a);
  int rcb = pn_class_refcount(clazz, b);

  CHECK((rca == -1 || rca == 1));
  CHECK((rcb == -1 || rcb == 1));

  pn_class_incref(clazz, a);

  rca = pn_class_refcount(clazz, a);
  CHECK((rca == -1 || rca == 2));

  pn_class_decref(clazz, a);

  rca = pn_class_refcount(clazz, a);
  CHECK((rca == -1 || rca == 1));

  pn_class_free(clazz, a);
  pn_class_free(clazz, b);
}

TEST_CASE("object_class") {
  test_class(PN_OBJECT, 0);
  test_class(PN_VOID, 5);
  test_class(&noop_class, 128);
}

static void test_new(size_t size, const pn_class_t *clazz) {
  INFO("class=" << pn_class_name(clazz) << " size=" << size);
  void *obj = pn_class_new(clazz, size);
  REQUIRE(obj);
  CHECK(pn_class_refcount(PN_OBJECT, obj) == 1);
  CHECK(pn_class(obj) == clazz);
  char *bytes = (char *)obj;
  for (size_t i = 0; i < size; i++) {
    // touch everything for valgrind
    bytes[i] = i;
  }
  pn_free(obj);
}

TEST_CASE("object_class new") {
  test_new(0, PN_OBJECT);
  test_new(5, PN_OBJECT);
  test_new(128, &noop_class);
}

static void finalizer(void *object) {
  int **called = (int **)object;
  (**called)++;
}

#define CID_finalizer CID_pn_object
#define finalizer_initialize NULL
#define finalizer_finalize finalizer
#define finalizer_hashcode NULL
#define finalizer_compare NULL
#define finalizer_inspect NULL

TEST_CASE("object_finalize") {
  static pn_class_t clazz = PN_CLASS(finalizer);

  int **obj = (int **)pn_class_new(&clazz, sizeof(int *));
  REQUIRE(obj);

  int called = 0;
  *obj = &called;
  pn_free(obj);

  CHECK(called == 1);
}

TEST_CASE("object_free") {
  // just to make sure it doesn't seg fault or anything
  pn_free(NULL);
}

static uintptr_t hashcode(void *obj) { return (uintptr_t)obj; }

#define CID_hashcode CID_pn_object
#define hashcode_initialize NULL
#define hashcode_finalize NULL
#define hashcode_compare NULL
#define hashcode_hashcode hashcode
#define hashcode_inspect NULL

TEST_CASE("object_hashcode") {
  static pn_class_t clazz = PN_CLASS(hashcode);
  void *obj = pn_class_new(&clazz, 0);
  REQUIRE(obj);
  CHECK(pn_hashcode(obj) == (uintptr_t)obj);
  CHECK(pn_hashcode(NULL) == 0);
  pn_free(obj);
}

#define CID_compare CID_pn_object
#define compare_initialize NULL
#define compare_finalize NULL
#define compare_compare delta
#define compare_hashcode NULL
#define compare_inspect NULL

TEST_CASE("object_compare") {
  static pn_class_t clazz = PN_CLASS(compare);

  void *a = pn_class_new(&clazz, 0);
  REQUIRE(a);
  void *b = pn_class_new(&clazz, 0);
  REQUIRE(b);

  CHECK(pn_compare(a, b));
  CHECK(!pn_equals(a, b));
  CHECK(!pn_compare(a, a));
  CHECK(pn_equals(a, a));
  CHECK(!pn_compare(b, b));
  CHECK(pn_equals(b, b));
  CHECK(pn_compare(a, b) == (intptr_t)((uintptr_t)b - (uintptr_t)a));

  CHECK(pn_compare(NULL, b));
  CHECK(!pn_equals(NULL, b));

  CHECK(pn_compare(a, NULL));
  CHECK(!pn_equals(a, NULL));

  CHECK(!pn_compare(NULL, NULL));
  CHECK(pn_equals(NULL, NULL));

  pn_free(a);
  pn_free(b);
}

TEST_CASE("object_refcounting") {
  int refs = 3;
  void *obj = pn_class_new(PN_OBJECT, 0);

  CHECK(pn_refcount(obj) == 1);

  for (int i = 0; i < refs; i++) {
    pn_incref(obj);
    CHECK(pn_refcount(obj) == i + 2);
  }

  CHECK(pn_refcount(obj) == refs + 1);

  for (int i = 0; i < refs; i++) {
    pn_decref(obj);
    CHECK(pn_refcount(obj) == refs - i);
  }

  CHECK(pn_refcount(obj) == 1);

  pn_free(obj);
}

TEST_CASE("list") {
  pn_list_t *list = pn_list(PN_WEAKREF, 0);
  CHECK(pn_list_size(list) == 0);
  CHECK(!pn_list_add(list, (void *)0));
  CHECK(!pn_list_add(list, (void *)1));
  CHECK(!pn_list_add(list, (void *)2));
  CHECK(!pn_list_add(list, (void *)3));
  CHECK(pn_list_get(list, 0) == (void *)0);
  CHECK(pn_list_get(list, 1) == (void *)1);
  CHECK(pn_list_get(list, 2) == (void *)2);
  CHECK(pn_list_get(list, 3) == (void *)3);
  CHECK(pn_list_size(list) == 4);
  pn_list_del(list, 1, 2);
  CHECK(pn_list_size(list) == 2);
  CHECK(pn_list_get(list, 0) == (void *)0);
  CHECK(pn_list_get(list, 1) == (void *)3);
  pn_decref(list);
}

TEST_CASE("list_refcount") {
  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);
  void *four = pn_class_new(PN_OBJECT, 0);

  pn_list_t *list = pn_list(PN_OBJECT, 0);
  CHECK(!pn_list_add(list, one));
  CHECK(!pn_list_add(list, two));
  CHECK(!pn_list_add(list, three));
  CHECK(!pn_list_add(list, four));
  CHECK(pn_list_get(list, 0) == one);
  CHECK(pn_list_get(list, 1) == two);
  CHECK(pn_list_get(list, 2) == three);
  CHECK(pn_list_get(list, 3) == four);
  CHECK(pn_list_size(list) == 4);

  CHECK(pn_refcount(one) == 2);
  CHECK(pn_refcount(two) == 2);
  CHECK(pn_refcount(three) == 2);
  CHECK(pn_refcount(four) == 2);

  pn_list_del(list, 1, 2);
  CHECK(pn_list_size(list) == 2);

  CHECK(pn_refcount(one) == 2);
  CHECK(pn_refcount(two) == 1);
  CHECK(pn_refcount(three) == 1);
  CHECK(pn_refcount(four) == 2);

  CHECK(pn_list_get(list, 0) == one);
  CHECK(pn_list_get(list, 1) == four);

  CHECK(!pn_list_add(list, one));

  CHECK(pn_list_size(list) == 3);
  CHECK(pn_refcount(one) == 3);

  pn_decref(list);

  CHECK(pn_refcount(one) == 1);
  CHECK(pn_refcount(two) == 1);
  CHECK(pn_refcount(three) == 1);
  CHECK(pn_refcount(four) == 1);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);
  pn_decref(four);
}

#define check_list_index(list, value, idx)                                     \
  CHECK(pn_list_index(list, value) == idx)

TEST_CASE("list_index") {
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

TEST_CASE("list_build") {
  pn_list_t *l = build_list(0, pn_string("one"), pn_string("two"),
                            pn_string("three"), END);

  REQUIRE(pn_list_size(l) == 3);

  CHECK_THAT(pn_string_get((pn_string_t *)pn_list_get(l, 0)), Equals("one"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_list_get(l, 1)), Equals("two"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_list_get(l, 2)), Equals("three"));
  pn_free(l);
}

TEST_CASE("map_build") {
  pn_map_t *m = build_map(0.75, 0, pn_string("key"), pn_string("value"),
                          pn_string("key2"), pn_string("value2"), END);

  CHECK(pn_map_size(m) == 2);

  pn_string_t *key = pn_string(NULL);

  pn_string_set(key, "key");
  CHECK_THAT(pn_string_get((pn_string_t *)pn_map_get(m, key)), Equals("value"));
  pn_string_set(key, "key2");
  CHECK_THAT(pn_string_get((pn_string_t *)pn_map_get(m, key)),
             Equals("value2"));

  pn_free(m);
  pn_free(key);
}

TEST_CASE("map_build_odd") {
  pn_map_t *m =
      build_map(0.75, 0, pn_string("key"), pn_string("value"),
                pn_string("key2"), pn_string("value2"), pn_string("key3"), END);

  CHECK(pn_map_size(m) == 3);

  pn_string_t *key = pn_string(NULL);

  pn_string_set(key, "key");
  CHECK_THAT(pn_string_get((pn_string_t *)pn_map_get(m, key)), Equals("value"));
  pn_string_set(key, "key2");
  CHECK_THAT(pn_string_get((pn_string_t *)pn_map_get(m, key)),
             Equals("value2"));
  pn_string_set(key, "key3");
  CHECK(pn_map_get(m, key) == NULL);

  pn_free(m);
  pn_free(key);
}

TEST_CASE("map") {
  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);

  pn_map_t *map = pn_map(PN_OBJECT, PN_OBJECT, 4, 0.75);
  CHECK(pn_map_size(map) == 0);

  pn_string_t *key = pn_string("key");
  pn_string_t *dup = pn_string("key");
  pn_string_t *key1 = pn_string("key1");
  pn_string_t *key2 = pn_string("key2");

  CHECK(!pn_map_put(map, key, one));
  CHECK(pn_map_size(map) == 1);
  CHECK(!pn_map_put(map, key1, two));
  CHECK(pn_map_size(map) == 2);
  CHECK(!pn_map_put(map, key2, three));
  CHECK(pn_map_size(map) == 3);

  CHECK(pn_map_get(map, dup) == one);

  CHECK(!pn_map_put(map, dup, one));
  CHECK(pn_map_size(map) == 3);

  CHECK(!pn_map_put(map, dup, two));
  CHECK(pn_map_size(map) == 3);
  CHECK(pn_map_get(map, dup) == two);

  CHECK(pn_refcount(key) == 2);
  CHECK(pn_refcount(dup) == 1);
  CHECK(pn_refcount(key1) == 2);
  CHECK(pn_refcount(key2) == 2);

  CHECK(pn_refcount(one) == 1);
  CHECK(pn_refcount(two) == 3);
  CHECK(pn_refcount(three) == 2);

  pn_map_del(map, key1);
  CHECK(pn_map_size(map) == 2);

  CHECK(pn_refcount(key) == 2);
  CHECK(pn_refcount(dup) == 1);
  CHECK(pn_refcount(key1) == 1);
  CHECK(pn_refcount(key2) == 2);

  CHECK(pn_refcount(one) == 1);
  CHECK(pn_refcount(two) == 2);
  CHECK(pn_refcount(three) == 2);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);

  pn_decref(key);
  pn_decref(dup);
  pn_decref(key1);
  pn_decref(key2);

  pn_decref(map);
}

TEST_CASE("hash") {
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

  CHECK(pn_hash_get(hash, 2) == two);
  CHECK(pn_hash_get(hash, 5) == two);
  CHECK(pn_hash_get(hash, 18) == one);
  CHECK(pn_hash_get(hash, 0) == NULL);

  CHECK(pn_hash_size(hash) == 14);

  pn_hash_del(hash, 5);
  CHECK(pn_hash_get(hash, 5) == NULL);
  CHECK(pn_hash_size(hash) == 13);
  pn_hash_del(hash, 18);
  CHECK(pn_hash_get(hash, 18) == NULL);
  CHECK(pn_hash_size(hash) == 12);

  pn_decref(hash);

  pn_decref(one);
  pn_decref(two);
  pn_decref(three);
}

// collider class: all objects have same hash, no two objects compare equal
static intptr_t collider_compare(void *a, void *b) {
  if (a == b) return 0;
  return (a > b) ? 1 : -1;
}

static uintptr_t collider_hashcode(void *obj) { return 23; }

#define CID_collider CID_pn_object
#define collider_initialize NULL
#define collider_finalize NULL
#define collider_inspect NULL

TEST_CASE("map_links") {
  const pn_class_t collider_clazz = PN_CLASS(collider);
  void *keys[3];
  for (int i = 0; i < 3; i++) keys[i] = pn_class_new(&collider_clazz, 0);

  // test deleting a head, middle link, tail

  for (int delete_idx = 0; delete_idx < 3; delete_idx++) {
    pn_map_t *map = pn_map(PN_WEAKREF, PN_WEAKREF, 0, 0.75);
    // create a chain of entries that have same head (from identical key
    // hashcode)
    for (int i = 0; i < 3; i++) {
      pn_map_put(map, keys[i], keys[i]);
    }
    pn_map_del(map, keys[delete_idx]);
    for (int i = 0; i < 3; i++) {
      void *value = (i == delete_idx) ? NULL : keys[i];
      CHECK(pn_map_get(map, keys[i]) == value);
    }
    pn_free(map);
  }
  for (int i = 0; i < 3; i++) pn_free(keys[i]);
}

static void test_string(const char *value) {
  size_t size = value ? strlen(value) : 0;

  pn_string_t *str = pn_string(value);
  CHECK_THAT(value, Equals(pn_string_get(str)));
  CHECK(size == pn_string_size(str));

  pn_string_t *strn = pn_stringn(value, size);
  CHECK_THAT(value, Equals(pn_string_get(strn)));
  CHECK(size == pn_string_size(strn));

  pn_string_t *strset = pn_string(NULL);
  pn_string_set(strset, value);
  CHECK_THAT(value, Equals(pn_string_get(strset)));
  CHECK(size == pn_string_size(strset));

  pn_string_t *strsetn = pn_string(NULL);
  pn_string_setn(strsetn, value, size);
  CHECK_THAT(value, Equals(pn_string_get(strsetn)));
  CHECK(size == pn_string_size(strsetn));

  CHECK(pn_hashcode(str) == pn_hashcode(strn));
  CHECK(pn_hashcode(str) == pn_hashcode(strset));
  CHECK(pn_hashcode(str) == pn_hashcode(strsetn));

  CHECK(!pn_compare(str, str));
  CHECK(!pn_compare(str, strn));
  CHECK(!pn_compare(str, strset));
  CHECK(!pn_compare(str, strsetn));

  pn_free(str);
  pn_free(strn);
  pn_free(strset);
  pn_free(strsetn);
}

TEST_CASE("string_null") { test_string(NULL); }
TEST_CASE("string_empty") { test_string(""); }
TEST_CASE("string_simple") { test_string("this is a test"); }
TEST_CASE("string_long") {
  test_string(
      "012345678910111213151617181920212223242526272829303132333435363"
      "738394041424344454647484950515253545556575859606162636465666768");
}

TEST_CASE("string embedded null") {
  const char value[] = "this has an embedded \000 in it";
  size_t size = sizeof(value);

  pn_string_t *strn = pn_stringn(value, size);
  CHECK_THAT(value, Equals(pn_string_get(strn)));
  CHECK(pn_string_size(strn) == size);

  pn_string_t *strsetn = pn_string(NULL);
  pn_string_setn(strsetn, value, size);
  CHECK_THAT(value, Equals(pn_string_get(strsetn)));
  CHECK(pn_string_size(strsetn) == size);

  CHECK(pn_hashcode(strn) == pn_hashcode(strsetn));
  CHECK(!pn_compare(strn, strsetn));

  pn_free(strn);
  pn_free(strsetn);
}

TEST_CASE("string_format") {
  pn_string_t *str = pn_string("");
  CHECK(str);
  int err = pn_string_format(str, "%s",
                             "this is a string that should be long "
                             "enough to force growth but just in case we'll "
                             "tack this other really long string on for the "
                             "heck of it");
  CHECK(err == 0);
  pn_free(str);
}

TEST_CASE("string_addf") {
  pn_string_t *str = pn_string("hello ");
  CHECK(str);
  int err = pn_string_addf(str, "%s",
                           "this is a string that should be long "
                           "enough to force growth but just in case we'll "
                           "tack this other really long string on for the "
                           "heck of it");
  CHECK(err == 0);
  pn_free(str);
}

TEST_CASE("map_iteration") {
  int n = 5;
  pn_list_t *pairs = pn_list(PN_OBJECT, 2 * n);
  for (int i = 0; i < n; i++) {
    void *key = pn_class_new(PN_OBJECT, 0);
    void *value = pn_class_new(PN_OBJECT, 0);
    pn_list_add(pairs, key);
    pn_list_add(pairs, value);
    pn_decref(key);
    pn_decref(value);
  }

  pn_map_t *map = pn_map(PN_OBJECT, PN_OBJECT, 0, 0.75);

  CHECK(pn_map_head(map) == 0);

  for (int i = 0; i < n; i++) {
    pn_map_put(map, pn_list_get(pairs, 2 * i), pn_list_get(pairs, 2 * i + 1));
  }

  for (pn_handle_t entry = pn_map_head(map); entry;
       entry = pn_map_next(map, entry)) {
    void *key = pn_map_key(map, entry);
    void *value = pn_map_value(map, entry);
    ssize_t idx = pn_list_index(pairs, key);
    CHECK(idx >= 0);

    CHECK(pn_list_get(pairs, idx) == key);
    CHECK(pn_list_get(pairs, idx + 1) == value);

    pn_list_del(pairs, idx, 2);
  }

  CHECK(pn_list_size(pairs) == 0);

  pn_decref(map);
  pn_decref(pairs);
}

#define test_inspect(o, expected)                                              \
  do {                                                                         \
    pn_string_t *dst = pn_string(NULL);                                        \
    pn_inspect(o, dst);                                                        \
    CHECK_THAT(expected, Equals(pn_string_get(dst)));                          \
    pn_free(dst);                                                              \
  } while (0)

TEST_CASE("list_inspect") {
  pn_list_t *l = build_list(0, END);
  test_inspect(l, "[]");
  pn_free(l);

  l = build_list(0, pn_string("one"), END);
  test_inspect(l, "[\"one\"]");
  pn_free(l);

  l = build_list(0, pn_string("one"), pn_string("two"), END);
  test_inspect(l, "[\"one\", \"two\"]");
  pn_free(l);

  l = build_list(0, pn_string("one"), pn_string("two"), pn_string("three"),
                 END);
  test_inspect(l, "[\"one\", \"two\", \"three\"]");
  pn_free(l);
}

TEST_CASE("map_inspect") {
  // note that when there is more than one entry in a map, the order
  // of the entries is dependent on the hashes involved, it will be
  // deterministic though
  pn_map_t *m = build_map(0.75, 0, END);
  test_inspect(m, "{}");
  pn_free(m);

  m = build_map(0.75, 0, pn_string("key"), pn_string("value"), END);
  test_inspect(m, "{\"key\": \"value\"}");
  pn_free(m);

  m = build_map(0.75, 0, pn_string("k1"), pn_string("v1"), pn_string("k2"),
                pn_string("v2"), END);
  test_inspect(m, "{\"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);

  m = build_map(0.75, 0, pn_string("k1"), pn_string("v1"), pn_string("k2"),
                pn_string("v2"), pn_string("k3"), pn_string("v3"), END);
  test_inspect(m, "{\"k3\": \"v3\", \"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);
}

TEST_CASE("map_coalesced_chain") {
  pn_hash_t *map = pn_hash(PN_OBJECT, 16, 0.75);
  pn_string_t *values[9] = {pn_string("a"), pn_string("b"), pn_string("c"),
                            pn_string("d"), pn_string("e"), pn_string("f"),
                            pn_string("g"), pn_string("h"), pn_string("i")};
  // add some items:
  pn_hash_put(map, 1, values[0]);
  pn_hash_put(map, 2, values[1]);
  pn_hash_put(map, 3, values[2]);

  // use up all non-addressable elements:
  pn_hash_put(map, 14, values[3]);
  pn_hash_put(map, 15, values[4]);
  pn_hash_put(map, 16, values[5]);

  // use an addressable element for a key that doesn't map to it:
  pn_hash_put(map, 4, values[6]);
  pn_hash_put(map, 17, values[7]);
  CHECK(pn_hash_size(map) == 8);

  // free up one non-addressable entry:
  pn_hash_del(map, 16);
  CHECK(pn_hash_get(map, 16) == NULL);
  CHECK(pn_hash_size(map) == 7);

  // add a key whose addressable slot is already taken (by 17),
  // generating a coalesced chain:
  pn_hash_put(map, 12, values[8]);

  // remove an entry from the coalesced chain:
  pn_hash_del(map, 4);
  CHECK(pn_hash_get(map, 4) == NULL);

  // test lookup of all entries:
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 1)), Equals("a"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 2)), Equals("b"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 3)), Equals("c"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 14)), Equals("d"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 15)), Equals("e"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 17)), Equals("h"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 12)), Equals("i"));
  CHECK(pn_hash_size(map) == 7);

  // cleanup:
  for (pn_handle_t i = pn_hash_head(map); i; i = pn_hash_head(map)) {
    pn_hash_del(map, pn_hash_key(map, i));
  }
  CHECK(pn_hash_size(map) == 0);

  for (size_t i = 0; i < 9; ++i) {
    pn_free(values[i]);
  }
  pn_free(map);
}

TEST_CASE("map_coalesced_chain2") {
  pn_hash_t *map = pn_hash(PN_OBJECT, 16, 0.75);
  pn_string_t *values[10] = {pn_string("a"), pn_string("b"), pn_string("c"),
                             pn_string("d"), pn_string("e"), pn_string("f"),
                             pn_string("g"), pn_string("h"), pn_string("i"),
                             pn_string("j")};
  // add some items:
  pn_hash_put(map, 1, values[0]); // a
  pn_hash_put(map, 2, values[1]); // b
  pn_hash_put(map, 3, values[2]); // c

  // use up all non-addressable elements:
  pn_hash_put(map, 14, values[3]); // d
  pn_hash_put(map, 15, values[4]); // e
  pn_hash_put(map, 16, values[5]); // f
  // take slot from addressable region
  pn_hash_put(map, 29, values[6]); // g, goes into slot 12

  // free up one non-addressable entry:
  pn_hash_del(map, 14);
  CHECK(pn_hash_get(map, 14) == NULL);

  // add a key whose addressable slot is already taken (by 29),
  // generating a coalesced chain:
  pn_hash_put(map, 12, values[7]); // h
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 12)), Equals("h"));
  // delete from tail of coalesced chain:
  pn_hash_del(map, 12);
  CHECK(pn_hash_get(map, 12) == NULL);

  // extend chain into cellar again, then coalesce again extending back
  // into addressable region
  pn_hash_put(map, 42, values[8]); // i
  pn_hash_put(map, 25, values[9]); // j
  // delete entry from coalesced chain, where next element in chain is
  // in cellar:
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 29)), Equals("g"));
  pn_hash_del(map, 29);

  // test lookup of all entries:
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 1)), Equals("a"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 2)), Equals("b"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 3)), Equals("c"));
  // d was deleted
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 15)), Equals("e"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 16)), Equals("f"));
  // g was deleted, h was deleted
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 42)), Equals("i"));
  CHECK_THAT(pn_string_get((pn_string_t *)pn_hash_get(map, 25)), Equals("j"));
  CHECK(pn_hash_size(map) == 7);

  // cleanup:
  for (pn_handle_t i = pn_hash_head(map); i; i = pn_hash_head(map)) {
    pn_hash_del(map, pn_hash_key(map, i));
  }
  CHECK(pn_hash_size(map) == 0);

  for (size_t i = 0; i < 10; ++i) {
    pn_free(values[i]);
  }
  pn_free(map);
}

TEST_CASE("list_compare") {
  pn_list_t *a = pn_list(PN_OBJECT, 0);
  pn_list_t *b = pn_list(PN_OBJECT, 0);

  CHECK(pn_equals(a, b));

  void *one = pn_class_new(PN_OBJECT, 0);
  void *two = pn_class_new(PN_OBJECT, 0);
  void *three = pn_class_new(PN_OBJECT, 0);

  pn_list_add(a, one);
  CHECK(!pn_equals(a, b));
  pn_list_add(b, one);
  CHECK(pn_equals(a, b));

  pn_list_add(b, two);
  CHECK(!pn_equals(a, b));
  pn_list_add(a, two);
  CHECK(pn_equals(a, b));

  pn_list_add(a, three);
  CHECK(!pn_equals(a, b));
  pn_list_add(b, three);
  CHECK(pn_equals(a, b));

  pn_free(a);
  pn_free(b);
  pn_free(one);
  pn_free(two);
  pn_free(three);
}

typedef struct {
  pn_list_t *list;
  size_t index;
} pn_it_state_t;

static void *pn_it_next(void *state) {
  pn_it_state_t *it = (pn_it_state_t *)state;
  if (it->index < pn_list_size(it->list)) {
    return pn_list_get(it->list, it->index++);
  } else {
    return NULL;
  }
}

TEST_CASE("list_iterator") {
  pn_list_t *list = build_list(0, pn_string("one"), pn_string("two"),
                               pn_string("three"), pn_string("four"), END);
  pn_iterator_t *it = pn_iterator();
  pn_it_state_t *state =
      (pn_it_state_t *)pn_iterator_start(it, pn_it_next, sizeof(pn_it_state_t));
  state->list = list;
  state->index = 0;

  void *obj;
  int index = 0;
  while ((obj = pn_iterator_next(it))) {
    CHECK(obj == pn_list_get(list, index));
    ++index;
  }
  CHECK(index == 4);

  pn_free(list);
  pn_free(it);
}

TEST_CASE("list_heap") {
  int size = 64;
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

    pn_list_minpush(list, (void *)r);
  }

  intptr_t prev = (intptr_t)pn_list_minpop(list);
  CHECK(prev == min);
  CHECK(pn_list_size(list) == (size_t)(size - 1));
  int count = 0;
  while (pn_list_size(list)) {
    intptr_t r = (intptr_t)pn_list_minpop(list);
    CHECK(r >= prev);
    prev = r;
    count++;
  }
  CHECK(count == size - 1);
  CHECK(prev == max);

  pn_free(list);
}
