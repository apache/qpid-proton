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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  All output from test marcros goes to stdout not stderr, error messages are normal for a test.
  Some errno handling functions are thread-unsafe
  */


/* Call via TEST_ASSERT macros */
static void assert_fail_(const char* cond, const char* file, int line, const char *fmt, ...) {
  printf("%s:%d: Assertion failed: %s", file, line, cond);
  if (fmt && *fmt) {
    va_list ap;
    va_start(ap, fmt);
    printf(" - ");
    vprintf(fmt, ap);
    printf("\n");
    fflush(stdout);
    va_end(ap);
  }
  abort();
}

/* Unconditional assert (does not depend on NDEBUG) for tests. */
#define TEST_ASSERT(expr) \
  ((expr) ?  (void)0 : assert_fail_(#expr, __FILE__, __LINE__, NULL))

/* Unconditional assert with printf-style message (does not depend on NDEBUG) for tests. */
#define TEST_ASSERTF(expr, ...) \
  ((expr) ?  (void)0 : assert_fail_(#expr, __FILE__, __LINE__, __VA_ARGS__))

/* Like TEST_ASSERT but includes  errno string for err */
/* TODO aconway 2017-02-16: not thread safe, replace with safe strerror_r or similar */
#define TEST_ASSERT_ERRNO(expr, err) \
  TEST_ASSERTF((expr), "%s", strerror(err))


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
    printf("%s:%d:[%s] check failed: (%s)", file, line, t->name, sexpr);
    if (fmt && *fmt) {
      printf(" - ");
      vprintf(fmt, ap);
    }
    printf("\n");
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


/* Some very simple platform-secifics to acquire an unused socket */
#if defined(WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
static inline void sock_close(sock_t sock) { closesocket(sock); }

#else  /* POSIX */

typedef int sock_t;
# include <netinet/in.h>
# include <unistd.h>
static inline void sock_close(sock_t sock) { close(sock); }
#endif


/* Create a socket and bind(LOOPBACK:0) to get a free port.
   Use SO_REUSEADDR so other processes can bind and listen on this port.
   Close the returned fd when the other process is listening.
   Asserts on error.
*/
static sock_t sock_bind0(void) {
  int sock =  socket(AF_INET, SOCK_STREAM, 0);
  TEST_ASSERT_ERRNO(sock >= 0, errno);
  int on = 1;
  TEST_ASSERT_ERRNO(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) == 0, errno);
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;    /* set the type of connection to TCP/IP */
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;            /* bind to port 0 */
  TEST_ASSERT_ERRNO(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0, errno);
  return sock;
}

static int sock_port(sock_t sock) {
  struct sockaddr addr = {0};
  socklen_t len = sizeof(addr);
  TEST_ASSERT_ERRNO(getsockname(sock, &addr, &len) == 0, errno);
  int port = -1;
  switch (addr.sa_family) {
   case AF_INET: port = ((struct sockaddr_in*)&addr)->sin_port; break;
   case AF_INET6: port = ((struct sockaddr_in6*)&addr)->sin6_port; break;
   default: TEST_ASSERTF(false, "unknown protocol type %d\n", addr.sa_family); break;
  }
  return ntohs(port);
}

#endif // TESTS_TEST_TOOLS_H
