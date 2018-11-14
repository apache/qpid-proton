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
#include "catch_extra.hpp"

#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/session.h>
#include <proton/transport.h>

/**
 * The decref order tests validate that whenever the last pointer to a
 * child object, e.g. a session or a link, is about to go away, the
 * parent object takes ownership of that reference if the child object
 * has not been freed, this avoids reference cycles but allows
 * navigation from parents to children.
 **/

#define SETUP_CSL                                                              \
  pn_connection_t *conn = pn_connection();                                     \
  pn_session_t *ssn = pn_session(conn);                                        \
  pn_incref(ssn);                                                              \
  pn_link_t *lnk = pn_sender(ssn, "sender");                                   \
  pn_incref(lnk);                                                              \
                                                                               \
  REQUIRE(pn_refcount(conn) == 2);                                             \
  REQUIRE(pn_refcount(ssn) == 2);                                              \
  REQUIRE(pn_refcount(lnk) == 1);

TEST_CASE("test_decref_order_csl") {
  SETUP_CSL;

  pn_decref(conn);
  REQUIRE(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(ssn);
  REQUIRE(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(lnk);
  // all gone now (requires valgrind to validate)
}

TEST_CASE("test_decref_order_cls") {
  SETUP_CSL;

  pn_decref(conn);
  REQUIRE(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(lnk);
  REQUIRE(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(ssn);
  // all gone now (requires valgrind to validate)
}

TEST_CASE("test_decref_order_lcs") {
  SETUP_CSL;

  pn_decref(lnk);
  REQUIRE(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(conn);
  REQUIRE(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(ssn);
  // all gone now (requires valgrind to validate)
}

TEST_CASE("test_decref_order_scl") {
  SETUP_CSL;

  pn_decref(ssn);
  REQUIRE(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(conn);
  REQUIRE(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(lnk);
  // all gone now (requires valgrind to validate)
}

TEST_CASE("test_decref_order_slc") {
  SETUP_CSL;

  pn_decref(ssn);
  REQUIRE(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(lnk);
  REQUIRE(pn_refcount(ssn) == 1); // connection takes over ownership
  REQUIRE(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(conn);
  // all gone now (requires valgrind to validate)
}

TEST_CASE("test_decref_order_lsc") {
  SETUP_CSL;

  pn_decref(lnk);
  REQUIRE(pn_refcount(lnk) == 1); // session takes over ownership
  REQUIRE(pn_refcount(ssn) == 1);
  pn_decref(ssn);
  REQUIRE(pn_refcount(lnk) == 1);
  REQUIRE(pn_refcount(ssn) == 1); // connection takes over ownership
  pn_decref(conn);
  // all gone now (requires valgrind to validate)
}

/**
 * The incref order tests verify that once ownership of the last
 * pointer to a child is taken over by a parent, it is reassigned when
 * the child is increfed.
 **/

#define SETUP_INCREF_ORDER                                                     \
  SETUP_CSL;                                                                   \
  pn_decref(lnk);                                                              \
  pn_decref(ssn);                                                              \
  REQUIRE(pn_refcount(lnk) == 1);                                              \
  REQUIRE(pn_refcount(ssn) == 1);                                              \
  REQUIRE(pn_refcount(conn) == 1);

TEST_CASE("test_incref_order_sl") {
  SETUP_INCREF_ORDER;

  pn_incref(ssn);
  REQUIRE(pn_refcount(conn) == 2);
  REQUIRE(pn_refcount(ssn) == 1);
  REQUIRE(pn_refcount(lnk) == 1);
  pn_incref(lnk);
  REQUIRE(pn_refcount(conn) == 2);
  REQUIRE(pn_refcount(ssn) == 2);
  REQUIRE(pn_refcount(lnk) == 1);

  pn_decref(conn);
  pn_decref(ssn);
  pn_decref(lnk);
}

TEST_CASE("test_incref_order_ls") {
  SETUP_INCREF_ORDER;

  pn_incref(lnk);
  REQUIRE(pn_refcount(conn) == 2);
  REQUIRE(pn_refcount(ssn) == 1);
  REQUIRE(pn_refcount(lnk) == 1);
  pn_incref(ssn);
  REQUIRE(pn_refcount(conn) == 2);
  REQUIRE(pn_refcount(ssn) == 2);
  REQUIRE(pn_refcount(lnk) == 1);

  pn_decref(conn);
  pn_decref(ssn);
  pn_decref(lnk);
}

static void swap(int array[], int i, int j) {
  int a = array[i];
  int b = array[j];
  array[j] = a;
  array[i] = b;
}

static void setup(void **objects) {
  pn_connection_t *conn = pn_connection();
  pn_session_t *ssn = pn_session(conn);
  pn_incref(ssn);
  pn_link_t *lnk = pn_sender(ssn, "sender");
  pn_incref(lnk);
  pn_delivery_t *dlv = pn_delivery(lnk, pn_dtag("dtag", 4));
  pn_incref(dlv);

  REQUIRE(pn_refcount(conn) == 2);
  REQUIRE(pn_refcount(ssn) == 2);
  REQUIRE(pn_refcount(lnk) == 2);
  REQUIRE(pn_refcount(dlv) == 1);

  objects[0] = conn;
  objects[1] = ssn;
  objects[2] = lnk;
  objects[3] = dlv;
}

static bool decreffed(int *indexes, void **objects, int step, void *object) {
  for (int i = 0; i <= step; i++) {
    if (object == objects[indexes[i]]) {
      return true;
    }
  }
  return false;
}

static bool live_descendent(int *indexes, void **objects, int step,
                            int objidx) {
  for (int i = objidx + 1; i < 4; i++) {
    if (!decreffed(indexes, objects, step, objects[i])) {
      return true;
    }
  }

  return false;
}

static void assert_refcount(void *object, int expected) {
  int rc = pn_refcount(object);
  // printf("pn_refcount(%s) = %d\n", pn_object_reify(object)->name, rc);
  REQUIRE(rc == expected);
}

static void test_decref_order(int *indexes, void **objects) {
  setup(objects);

  // printf("-----------\n");
  for (int i = 0; i < 3; i++) {
    int idx = indexes[i];
    void *obj = objects[idx];
    // printf("decreffing %s\n", pn_object_reify(obj)->name);
    pn_decref(obj);
    for (int j = 0; j <= i; j++) {
      // everything we've decreffed already should have a refcount of
      // 1 because it has been preserved by its parent
      assert_refcount(objects[indexes[j]], 1);
    }
    for (int j = i + 1; j < 4; j++) {
      // everything we haven't decreffed yet should have a refcount of
      // 2 unless it has a descendant that has not been decrefed (or
      // it has no child) in which case it should have a refcount of 1
      int idx = indexes[j];
      void *obj = objects[idx];
      REQUIRE(!decreffed(indexes, objects, i, obj));
      if (live_descendent(indexes, objects, i, idx)) {
        assert_refcount(obj, 2);
      } else {
        assert_refcount(obj, 1);
      }
    }
  }

  void *last = objects[indexes[3]];
  // printf("decreffing %s\n", pn_object_reify(last)->name);
  pn_decref(last);
  // all should be gone now, need to run with valgrind to check
}

static void permute(int n, int *indexes, void **objects) {
  int j;
  if (n == 1) {
    test_decref_order(indexes, objects);
  } else {
    for (int i = 1; i <= n; i++) {
      permute(n - 1, indexes, objects);
      if ((n % 2) == 1) {
        j = 1;
      } else {
        j = i;
      }
      swap(indexes, j - 1, n - 1);
    }
  }
}

TEST_CASE("test_decref_permutations") {
  void *objects[4];
  int indexes[4] = {0, 1, 2, 3};
  permute(4, indexes, objects);
}

TEST_CASE("test_transport") {
  pn_transport_t *transport = pn_transport();
  REQUIRE(pn_refcount(transport) == 1);
  pn_incref(transport);
  REQUIRE(pn_refcount(transport) == 2);
  pn_decref(transport);
  REQUIRE(pn_refcount(transport) == 1);
  pn_free(transport);
}

TEST_CASE("test_connection_transport") {
  pn_connection_t *connection = pn_connection();
  REQUIRE(pn_refcount(connection) == 1);
  pn_transport_t *transport = pn_transport();
  REQUIRE(pn_refcount(transport) == 1);
  pn_transport_bind(transport, connection);
  REQUIRE(pn_refcount(connection) == 2);
  pn_decref(transport);
  REQUIRE(pn_refcount(transport) == 1); // preserved by the bind
  REQUIRE(pn_refcount(connection) == 1);
  pn_free(connection);
}

TEST_CASE("test_transport_connection") {
  pn_transport_t *transport = pn_transport();
  REQUIRE(pn_refcount(transport) == 1);
  pn_connection_t *connection = pn_connection();
  REQUIRE(pn_refcount(connection) == 1);
  pn_transport_bind(transport, connection);
  REQUIRE(pn_refcount(connection) == 2);
  pn_decref(connection);
  REQUIRE(pn_refcount(connection) == 1);
  REQUIRE(pn_refcount(transport) == 1);
  pn_free(transport);
}

static void drain(pn_collector_t *collector) {
  while (pn_collector_next(collector))
    ;
}

TEST_CASE("test_collector_connection_transport") {
  pn_collector_t *collector = pn_collector();
  REQUIRE(pn_refcount(collector) == 1);
  pn_connection_t *connection = pn_connection();
  REQUIRE(pn_refcount(connection) == 1);
  pn_connection_collect(connection, collector);
  REQUIRE(pn_refcount(collector) == 2);
  REQUIRE(pn_refcount(connection) == 2);
  drain(collector);
  REQUIRE(pn_refcount(connection) == 1);
  pn_transport_t *transport = pn_transport();
  REQUIRE(pn_refcount(transport) == 1);
  pn_transport_bind(transport, connection);
  REQUIRE(pn_refcount(transport) == 1);
  REQUIRE(pn_refcount(connection) == 3);
  drain(collector);
  REQUIRE(pn_refcount(connection) == 2);
  pn_decref(transport);
  REQUIRE(pn_refcount(transport) == 1); // preserved by the bind
  REQUIRE(pn_refcount(connection) == 1);
  pn_free(connection);
  REQUIRE(pn_refcount(transport) == 1);  // events
  REQUIRE(pn_refcount(connection) == 1); // events
  pn_collector_free(collector);
}

TEST_CASE("test_collector_transport_connection") {
  pn_collector_t *collector = pn_collector();
  REQUIRE(pn_refcount(collector) == 1);
  pn_transport_t *transport = pn_transport();
  REQUIRE(pn_refcount(transport) == 1);
  pn_connection_t *connection = pn_connection();
  REQUIRE(pn_refcount(connection) == 1);
  pn_connection_collect(connection, collector);
  REQUIRE(pn_refcount(collector) == 2);
  REQUIRE(pn_refcount(connection) == 2);
  drain(collector);
  REQUIRE(pn_refcount(connection) == 1);
  pn_transport_bind(transport, connection);
  REQUIRE(pn_refcount(connection) == 3);
  REQUIRE(pn_refcount(transport) == 1);
  drain(collector);
  REQUIRE(pn_refcount(connection) == 2);
  REQUIRE(pn_refcount(transport) == 1);
  pn_decref(connection);
  REQUIRE(pn_refcount(connection) == 1);
  REQUIRE(pn_refcount(transport) == 1);
  pn_free(transport);
  REQUIRE(pn_refcount(connection) == 1);
  REQUIRE(pn_refcount(transport) == 1);
  pn_collector_free(collector);
}
