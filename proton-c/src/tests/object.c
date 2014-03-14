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

static pn_list_t *build_list(size_t capacity, int options, ...)
{
  pn_list_t *result = pn_list(capacity, options);
  va_list ap;

  va_start(ap, options);
  while (true) {
    void *arg = va_arg(ap, void *);
    if (arg == END) {
      break;
    }

    pn_list_add(result, arg);
    if (PN_REFCOUNT & options) {
      pn_decref(arg);
    }
  }
  va_end(ap);

  return result;
}

static pn_map_t *build_map(size_t capacity, float load_factor, int options, ...)
{
  pn_map_t *result = pn_map(capacity, load_factor, options);
  va_list ap;

  void *prev = NULL;

  va_start(ap, options);
  int count = 0;
  while (true) {
    void *arg = va_arg(ap, void *);
    bool last = arg == END;
    if (arg == END) {
      arg = NULL;
    }

    if (count % 2) {
      pn_map_put(result, prev, arg);
      if (PN_REFCOUNT & options) {
        pn_decref(prev);
        pn_decref(arg);
      }
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

static pn_class_t null_class = {0};

static pn_class_t noop_class = {noop, noop, zero, delta};

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

static void test_finalize(void)
{
  static pn_class_t clazz = {NULL, finalizer};

  int **obj = (int **) pn_new(sizeof(int **), &clazz);
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

static void test_hashcode(void)
{
  static pn_class_t clazz = {NULL, NULL, hashcode};
  void *obj = pn_new(0, &clazz);
  assert(obj);
  assert(pn_hashcode(obj) == (uintptr_t) obj);
  assert(pn_hashcode(NULL) == 0);
  pn_free(obj);
}

static void test_compare(void)
{
  static pn_class_t clazz = {NULL, NULL, NULL, delta};

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

static void check_list_index(pn_list_t *list, void *value, ssize_t idx)
{
  assert(pn_list_index(list, value) == idx);
}

static void test_list_index(void)
{
  pn_list_t *l = pn_list(0, 0);
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
  pn_list_t *l = build_list(0, PN_REFCOUNT,
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
  pn_map_t *m = build_map(0, 0.75, PN_REFCOUNT,
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
  pn_map_t *m = build_map(0, 0.75, PN_REFCOUNT,
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

static void test_hash(void)
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
  pn_list_t *pairs = pn_list(2*n, PN_REFCOUNT);
  for (int i = 0; i < n; i++) {
    void *key = pn_new(0, NULL);
    void *value = pn_new(0, NULL);
    pn_list_add(pairs, key);
    pn_list_add(pairs, value);
    pn_decref(key);
    pn_decref(value);
  }

  pn_map_t *map = pn_map(0, 0.75, PN_REFCOUNT);

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
  pn_list_t *l = build_list(0, PN_REFCOUNT, END);
  test_inspect(l, "[]");
  pn_free(l);

  l = build_list(0, PN_REFCOUNT, pn_string("one"), END);
  test_inspect(l, "[\"one\"]");
  pn_free(l);

  l = build_list(0, PN_REFCOUNT,
                 pn_string("one"),
                 pn_string("two"),
                 END);
  test_inspect(l, "[\"one\", \"two\"]");
  pn_free(l);

  l = build_list(0, PN_REFCOUNT,
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
  pn_map_t *m = build_map(0, 0.75, PN_REFCOUNT, END);
  test_inspect(m, "{}");
  pn_free(m);

  m = build_map(0, 0.75, PN_REFCOUNT,
                pn_string("key"), pn_string("value"),
                END);
  test_inspect(m, "{\"key\": \"value\"}");
  pn_free(m);

  m = build_map(0, 0.75, PN_REFCOUNT,
                pn_string("k1"), pn_string("v1"),
                pn_string("k2"), pn_string("v2"),
                END);
  test_inspect(m, "{\"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);

  m = build_map(0, 0.75, PN_REFCOUNT,
                pn_string("k1"), pn_string("v1"),
                pn_string("k2"), pn_string("v2"),
                pn_string("k3"), pn_string("v3"),
                END);
  test_inspect(m, "{\"k3\": \"v3\", \"k1\": \"v1\", \"k2\": \"v2\"}");
  pn_free(m);
}

void test_list_compare(void)
{
  pn_list_t *a = pn_list(0, PN_REFCOUNT);
  pn_list_t *b = pn_list(0, PN_REFCOUNT);

  assert(pn_equals(a, b));

  void *one = pn_new(0, NULL);
  void *two = pn_new(0, NULL);
  void *three = pn_new(0, NULL);

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
  pn_list_t *list = build_list(0, PN_REFCOUNT,
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
    assert(obj == pn_list_get(list, index++));
  }
  assert(index == 4);

  pn_free(list);
  pn_free(it);
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

  test_list_index();

  test_map();

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

  return 0;
}
