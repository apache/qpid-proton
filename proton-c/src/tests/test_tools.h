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
#include <proton/condition.h>
#include <proton/event.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A struct to collect the results of a test, created by RUN_TEST macro. */
typedef struct test_t {
  const char* name;
  int errors;
  uintptr_t data;               /* Test can store some non-error data here */
} test_t;

/* Internal, use macros. Print error message and increase the t->errors count.
   All output from test marcros goes to stderr so it interleaves with PN_TRACE logs.
*/

static void test_vlogf_(test_t *t, const char *prefix, const char* expr,
                        const char* file, int line, const char *fmt, va_list ap)
{
  fprintf(stderr, "%s:%d", file, line);
  if (prefix && *prefix) fprintf(stderr, ": %s", prefix);
  if (expr && *expr) fprintf(stderr, ": %s", expr);
  if (fmt && *fmt) {
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
  }
  if (t) fprintf(stderr, " [%s]", t->name);
  fprintf(stderr, "\n");
  fflush(stdout);
}

static void test_errorf_(test_t *t, const char* expr,
                         const char* file, int line, const char *fmt, ...) {
  ++t->errors;
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(t, "error", expr, file, line, fmt, ap);
  va_end(ap);
}

static bool test_check_(test_t *t, bool expr, const char *sexpr,
                        const char *file, int line, const char* fmt, ...) {
  if (!expr) {
    ++t->errors;
    va_list ap;
    va_start(ap, fmt);
    test_vlogf_(t, "error: check failed", sexpr, file, line, fmt, ap);
    va_end(ap);
  }
  return expr;
}

static void test_logf_(test_t *t, const char *prefix, const char* expr,
                       const char* file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(t, prefix, expr, file, line, fmt, ap);
  va_end(ap);
}

/* Call via TEST_ASSERT macros */
static void assert_fail_(const char* expr, const char* file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(NULL, "assertion failed", expr, file, line, fmt, ap);
  va_end(ap);
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


/* Print a message but don't mark the test as having an error */
#define TEST_LOGF(TEST, ...) \
  test_logf_((TEST), "info", NULL, __FILE__, __LINE__, __VA_ARGS__)

/* Print an error with printf-style message, increment TEST->errors */
#define TEST_ERRORF(TEST, ...) \
  test_errorf_((TEST), NULL, __FILE__, __LINE__, __VA_ARGS__)

/* If EXPR is false, print and record an error for t  */
#define TEST_CHECKF(TEST, EXPR, ...) \
  test_check_((TEST), (EXPR), #EXPR, __FILE__, __LINE__, __VA_ARGS__)

/* If EXPR is false, print and record an error for t including EXPR  */
#define TEST_CHECK(TEST, EXPR) \
  test_check_((TEST), (EXPR), #EXPR, __FILE__, __LINE__, "")

static inline bool test_etype_equal_(test_t *t, int want, int got, const char *file, int line) {
  return test_check_(t, want == got, NULL, file, line, "want %s got %s",
                     pn_event_type_name((pn_event_type_t)want),
                     pn_event_type_name((pn_event_type_t)got));
}

#define TEST_CHECK_COND(T, WANT, COND) do {                             \
    pn_condition_t *cond = (COND);                                      \
    if (TEST_CHECKF((T), pn_condition_is_set(cond), "expecting error")) { \
      const char* description = pn_condition_get_description(cond);     \
      if (!strstr(description, (WANT))) {                               \
        TEST_ERRORF((T), "expected '%s' in '%s'", (WANT), description); \
      }                                                                 \
    }                                                                   \
  } while(0)

#define TEST_CHECK_NO_COND(T, COND) do {                                \
    pn_condition_t *cond = (COND);                                      \
    if (cond && pn_condition_is_set(cond)) {                            \
      TEST_ERRORF((T), "unexpected condition: %s", pn_condition_get_description(cond)); \
    }                                                                   \
  } while(0)

#define TEST_ETYPE_EQUAL(TEST, WANT, GOT) \
  test_etype_equal_((TEST), (WANT), (GOT), __FILE__, __LINE__)

static inline pn_event_t *test_event_type_(test_t *t, pn_event_type_t want, pn_event_t *got, const char *file, int line) {
  test_check_(t, want == pn_event_type(got), NULL, file, line, "want %s got %s",
              pn_event_type_name(want),
              pn_event_type_name(pn_event_type(got)));
  if (want != pn_event_type(got)) {
    pn_condition_t *cond = pn_event_condition(got);
    if (cond && pn_condition_is_set(cond)) {
      test_errorf_(t, NULL, file, line, "condition: %s:%s",
                   pn_condition_get_name(cond), pn_condition_get_description(cond));
    }
    return NULL;
  }
  return got;
}

#define TEST_EVENT_TYPE(TEST, WANT, GOT) \
  test_event_type_((TEST), (WANT), (GOT), __FILE__, __LINE__)

/* T is name of a test_t variable, EXPR is the test expression (which should update T)
   FAILED is incremented if the test has errors
*/
#define RUN_TEST(FAILED, T, EXPR) do {                          \
    fprintf(stderr, "TEST: %s\n", #EXPR);                                \
    fflush(stdout);                                             \
    test_t T = { #EXPR, 0 };                                    \
    (EXPR);                                                     \
    if (T.errors) {                                             \
      fprintf(stderr, "FAIL: %s (%d errors)\n", #EXPR, T.errors);        \
      ++(FAILED);                                               \
    }                                                           \
  } while(0)

/* Like RUN_TEST but only if one of the argv strings is found in the test EXPR */
#define RUN_ARGV_TEST(FAILED, T, EXPR) do {                             \
    if (argc == 1) {                                                    \
      RUN_TEST(FAILED, T, EXPR);                                        \
    } else {                                                            \
      for (int i = 1; i < argc; ++i) {                                  \
        if (strstr(#EXPR, argv[i])) {                                   \
          RUN_TEST(FAILED, T, EXPR);                                    \
          break;                                                        \
        }                                                               \
      }                                                                 \
    }                                                                   \
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

/* Combines includes a sock_t with the int and char* versions of the port for convenience */
typedef struct test_port_t {
  sock_t sock;
  int port;                     /* port as integer */
  char str[256];                /* port as string */
  char host_port[256];          /* host:port string */
} test_port_t;

/* Modifies tp->host_port to use host, returns the new tp->host_port */
static const char *test_port_use_host(test_port_t *tp, const char *host) {
  snprintf(tp->host_port, sizeof(tp->host_port), "%s:%d", host, tp->port);
  return tp->host_port;
}

/* Create a test_port_t  */
static inline test_port_t test_port(const char* host) {
  test_port_t tp = {0};
  tp.sock = sock_bind0();
  tp.port = sock_port(tp.sock);
  snprintf(tp.str, sizeof(tp.str), "%d", tp.port);
  test_port_use_host(&tp, host);
  return tp;
}

#endif // TESTS_TEST_TOOLS_H
