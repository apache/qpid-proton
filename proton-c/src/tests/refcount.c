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

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <stdio.h>
#include <stdlib.h>

#define assert(E) ((E) ? 0 : (abort(), 0))

/**
 * The decref order tests validate that whenever the last pointer to a
 * child object, e.g. a session or a link, is about to go away, the
 * parent object takes ownership of that reference if the child object
 * has not been freed, this avoids reference cycles but allows
 * navigation from parents to children.
 **/

#define SETUP_CSL                               \
  pn_connection_t *conn = pn_connection();      \
  pn_session_t *ssn = pn_session(conn);         \
  pn_link_t *lnk = pn_sender(ssn, "sender");    \
                                                \
  assert(pn_refcount(conn) == 2);               \
  assert(pn_refcount(ssn) == 2);                \
  assert(pn_refcount(lnk) == 1);

static void test_decref_order_csl(void) {
  SETUP_CSL;

  pn_decref(conn);
  assert(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(ssn);
  assert(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(lnk);
  // all gone now (requires valgrind to validate)
}

static void test_decref_order_cls(void) {
  SETUP_CSL;

  pn_decref(conn);
  assert(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(lnk);
  assert(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(ssn);
  // all gone now (requires valgrind to validate)
}

static void test_decref_order_lcs(void) {
  SETUP_CSL;

  pn_decref(lnk);
  assert(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(conn);
  assert(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(ssn);
  // all gone now (requires valgrind to validate)
}

static void test_decref_order_scl(void) {
  SETUP_CSL;

  pn_decref(ssn);
  assert(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(conn);
  assert(pn_refcount(conn) == 1); // session keeps alive
  pn_decref(lnk);
  // all gone now (requires valgrind to validate)
}

static void test_decref_order_slc(void) {
  SETUP_CSL;

  pn_decref(ssn);
  assert(pn_refcount(ssn) == 1); // link keeps alive
  pn_decref(lnk);
  assert(pn_refcount(ssn) == 1); // connection takes over ownership
  assert(pn_refcount(lnk) == 1); // session takes over ownership
  pn_decref(conn);
  // all gone now (requires valgrind to validate)
}

static void test_decref_order_lsc(void) {
  SETUP_CSL;

  pn_decref(lnk);
  assert(pn_refcount(lnk) == 1); // session takes over ownership
  assert(pn_refcount(ssn) == 1);
  pn_decref(ssn);
  assert(pn_refcount(lnk) == 1);
  assert(pn_refcount(ssn) == 1); // connection takes over ownership
  pn_decref(conn);
  // all gone now (requires valgrind to validate)
}

/**
 * The incref order tests verify that once ownership of the last
 * pointer to a child is taken over by a parent, it is reassigned when
 * the child is increfed.
 **/

#define SETUP_INCREF_ORDER                      \
  SETUP_CSL;                                    \
  pn_decref(lnk);                               \
  pn_decref(ssn);                               \
  assert(pn_refcount(lnk) == 1);                \
  assert(pn_refcount(ssn) == 1);                \
  assert(pn_refcount(conn) == 1);

static void test_incref_order_sl(void) {
  SETUP_INCREF_ORDER;

  pn_incref(ssn);
  assert(pn_refcount(conn) == 2);
  assert(pn_refcount(ssn) == 1);
  assert(pn_refcount(lnk) == 1);
  pn_incref(lnk);
  assert(pn_refcount(conn) == 2);
  assert(pn_refcount(ssn) == 2);
  assert(pn_refcount(lnk) == 1);

  pn_decref(conn);
  pn_decref(ssn);
  pn_decref(lnk);
}

static void test_incref_order_ls(void) {
  SETUP_INCREF_ORDER;

  pn_incref(lnk);
  assert(pn_refcount(conn) == 2);
  assert(pn_refcount(ssn) == 1);
  assert(pn_refcount(lnk) == 1);
  pn_incref(ssn);
  assert(pn_refcount(conn) == 2);
  assert(pn_refcount(ssn) == 2);
  assert(pn_refcount(lnk) == 1);

  pn_decref(conn);
  pn_decref(ssn);
  pn_decref(lnk);
}

int main(int argc, char **argv)
{
  test_decref_order_csl();
  test_decref_order_cls();
  test_decref_order_lcs();
  test_decref_order_scl();
  test_decref_order_slc();
  test_decref_order_lsc();

  test_incref_order_sl();
  test_incref_order_ls();
  return 0;
}
