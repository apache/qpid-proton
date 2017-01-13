#ifndef TESTS_TEST_TOOLS_H
#define TESTS_TEST_TOOLS_H

/*
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
 */

#include <proton/type_compat.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Call via ASSERT macro. */
static void assert_fail_(const char* cond, const char* file, int line) {
  printf("%s:%d: Assertion failed: %s\n", file, line, cond);
  abort();
}

/* Unconditional assert (does not depend on NDEBUG) for tests. */
#define ASSERT(expr)                                            \
  ((expr) ?  (void)0 : assert_fail_(#expr, __FILE__, __LINE__))

/* Call via macro ASSERT_PERROR */
static void assert_perror_fail_(const char* cond, const char* file, int line) {
  perror(cond);
  printf("%s:%d: Assertion failed (error above): %s\n", file, line, cond);
  abort();
}

/* Like ASSERT but also calls perror() to print the current errno error. */
#define ASSERT_PERROR(expr)                                             \
  ((expr) ?  (void)0 : assert_perror_fail_(#expr, __FILE__, __LINE__))


/* A struct to collect the results of a test.
 * Declare and initialize with TEST_START(t) where t will be declared as a test_t
 */
typedef struct test_t {
  const char* name;
  int errors;
} test_t;

/* if !expr print the printf-style error and increment t->errors. Use via macros. Returns expr. */
static inline bool test_check_(test_t *t, bool expr, const char *sexpr, const char *file, int line, const char* fmt, ...) {
  if (!expr) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d:[%s] check failed: (%s)", file, line, t->name, sexpr);
    if (fmt && *fmt) {
      fprintf(stderr, " - ");
      vfprintf(stderr, fmt, ap);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    ++t->errors;
  }
  return expr;
}

#define TEST_CHECK(TEST, EXPR, ...) test_check_((TEST), (EXPR), #EXPR, __FILE__, __LINE__, __VA_ARGS__)

/* T is name of a test_t variable, EXPR is the test expression (which should update T)
   FAILED is incremented if the test has errors
*/
#define RUN_TEST(FAILED, T, EXPR) do {                          \
    printf("TEST: %s\n", #EXPR);                                \
    fflush(stdout);                                             \
    test_t T = { #EXPR, 0 };                                    \
    (EXPR);                                                     \
    if (T.errors) {                                             \
      printf("FAIL: %s (%d errors)\n", #EXPR, T.errors);        \
      ++(FAILED);                                               \
    }                                                           \
  } while(0)

#if defined(WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
static inline void sock_close(sock_t sock) { closesocket(sock); }

#else

#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

static int port_in_use(int port) {
  /* Attempt to bind a dummy socket to test if the port is in use. */
  int dummy_socket = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_PERROR(dummy_socket >= 0);
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  int ret = bind(dummy_socket, (struct sockaddr *) &addr, sizeof(addr));
  close(dummy_socket);
  return ret < 0;
}

/* Try to pick an unused port by picking random ports till we find one
   that is not in use. This is not foolproof as some other process may
   grab it before the caller binds or connects.
*/
static int pick_port(void) {
  srand(time(NULL));
  static int MAX_TRIES = 10;
  int port = -1;
  int i = 0;
  do {
    /* Pick a random port. Avoid the standard OS ephemeral port range used by
       bind(0) - ports can be allocated and re-allocated very rapidly there.
    */
    port =  (rand()%10000) + 10000;
  } while (i++ < MAX_TRIES && port_in_use(port));
  ASSERT(i < MAX_TRIES && "cannot pick a port");
  return port;
}

#endif

#endif // TESTS_TEST_TOOLS_H
