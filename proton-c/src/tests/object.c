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

#include <stdio.h>
#include <string.h>
#include <proton/object.h>
#include <assert.h>

static void noop(void *o) {}
static uintptr_t zero(void *o) { return 0; }
static intptr_t delta(void *a, void *b) { return (uintptr_t) b - (uintptr_t) a; }

static pn_class_t null_class = {0};

static pn_class_t noop_class = {noop, zero, delta};

static void test_new(size_t size, pn_class_t *clazz)
{
  void *obj = pn_new(size, clazz);
  assert(obj);
  assert(pn_refcount(obj) == 1);
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

static void test_finalize()
{
  static pn_class_t clazz = {finalizer};

  int **obj = (int **) pn_new(sizeof(int **), &clazz);
  assert(obj);

  int called = 0;
  *obj = &called;
  pn_free(obj);

  assert(called == 1);
}

static void test_free()
{
  // just to make sure it doesn't seg fault or anything
  pn_free(NULL);
}

static uintptr_t hashcode(void *obj) { return (uintptr_t) obj; }

static void test_hashcode()
{
  static pn_class_t clazz = {NULL, hashcode};
  void *obj = pn_new(0, &clazz);
  assert(obj);
  assert(pn_hashcode(obj) == (uintptr_t) obj);
  assert(pn_hashcode(NULL) == 0);
  pn_free(obj);
}

static void test_compare()
{
  static pn_class_t clazz = {NULL, NULL, delta};

  void *a = pn_new(0, &clazz);
  assert(a);
  void *b = pn_new(0, &clazz);
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
  void *obj = pn_new(0, NULL);

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
  pn_list_t *list = pn_list(0, 0);
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
  void *one = pn_new(0, NULL);
  void *two = pn_new(0, NULL);
  void *three = pn_new(0, NULL);
  void *four = pn_new(0, NULL);

  pn_list_t *list = pn_list(0, PN_REFCOUNT);
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

static void test_map()
{
  void *one = pn_new(0, NULL);
  void *two = pn_new(0, NULL);
  void *three = pn_new(0, NULL);

  pn_map_t *map = pn_map(4, 0.75, PN_REFCOUNT);
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

static void test_hash()
{
  void *one = pn_new(0, NULL);
  void *two = pn_new(0, NULL);
  void *three = pn_new(0, NULL);

  pn_hash_t *hash = pn_hash(4, 0.75, PN_REFCOUNT);
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

int main(int argc, char **argv)
{
  for (size_t i = 0; i < 128; i++) {
    test_new(i, NULL);
    test_new(i, &null_class);
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

  test_map();

  test_hash();

  test_string(NULL);
  test_string("");
  test_string("this is a test");
  test_string("012345678910111213151617181920212223242526272829303132333435363"
              "738394041424344454647484950515253545556575859606162636465666768");
  test_string("this has an embedded \000 in it");
  test_stringn("this has an embedded \000 in it", 28);

  return 0;
}
