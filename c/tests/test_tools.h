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

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <proton/type_compat.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Handy way to make pn_bytes: PN_BYTES_LITERAL(FOO) => pn_bytes("FOO",3) */
#define PN_BYTES_LITERAL(X) (pn_bytes(sizeof(#X)-1, #X))

/* A struct to collect the results of a test, created by RUN_TEST macro. */
typedef struct test_t {
  const char* name;
  int errors;
  uintptr_t data;               /* Test can store some non-error data here */
  bool inverted;                /* An inverted test prints no output and reports failure if it passes */
} test_t;

/* Internal, use macros. Print error message and increase the t->errors count.
   All output from test macros goes to stderr so it interleaves with PN_TRACE logs.
*/
void test_vlogf_(test_t *t, const char *prefix, const char* expr,
                 const char* file, int line, const char *fmt, va_list ap)
{
  if (t->inverted) return;
  fprintf(stderr, "%s:%d", file, line);
  if (prefix && *prefix) fprintf(stderr, ": %s", prefix);
  if (expr && *expr) fprintf(stderr, ": %s", expr);
  if (fmt && *fmt) {
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
  }
  if (t) fprintf(stderr, " [%s]", t->name);
  fprintf(stderr, "\n");
  fflush(stderr);
}

void test_logf_(test_t *t, const char *prefix, const char* expr,
                const char* file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(t, prefix, expr, file, line, fmt, ap);
  va_end(ap);
}

void test_errorf_(test_t *t, const char* expr,
                  const char* file, int line, const char *fmt, ...) {
  ++t->errors;
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(t, "error", expr, file, line, fmt, ap);
  va_end(ap);
}

bool test_check_(test_t *t, bool expr, const char *sexpr,
                 const char *file, int line, const char* fmt, ...) {
  if (!expr) {
    ++t->errors;
    va_list ap;
    va_start(ap, fmt);
    test_vlogf_(t, "check failed", sexpr, file, line, fmt, ap);
    va_end(ap);
  }
  return expr;
}

/* Call via TEST_ASSERT macros */
void assert_fail_(const char* expr, const char* file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  test_vlogf_(NULL, "assertion failed", expr, file, line, fmt, ap);
  va_end(ap);
  abort();
}

/* Unconditional assert (does not depend on NDEBUG) for tests. */
#define TEST_ASSERT(expr)                                               \
  ((expr) ?  (void)0 : assert_fail_(#expr, __FILE__, __LINE__, NULL))

/* Unconditional assert with printf-style message (does not depend on NDEBUG) for tests. */
#define TEST_ASSERTF(expr, ...)                                         \
  ((expr) ?  (void)0 : assert_fail_(#expr, __FILE__, __LINE__, __VA_ARGS__))

/* Like TEST_ASSERT but includes  errno string for err */
/* TODO aconway 2017-02-16: not thread safe, replace with safe strerror_r or similar */
#define TEST_ASSERT_ERRNO(expr, err)            \
  TEST_ASSERTF((expr), "%s", strerror(err))


/* Print a message but don't mark the test as having an error */
#define TEST_LOGF(TEST, ...)                                            \
  test_logf_((TEST), "info", NULL, __FILE__, __LINE__, __VA_ARGS__)

/* Print an error with printf-style message, increment TEST->errors */
#define TEST_ERRORF(TEST, ...)                                  \
  test_errorf_((TEST), NULL, __FILE__, __LINE__, __VA_ARGS__)

/* If EXPR is false, print and record an error for t including #EXPR  */
#define TEST_CHECKF(TEST, EXPR, ...)                                    \
  test_check_((TEST), (EXPR), #EXPR, __FILE__, __LINE__, __VA_ARGS__)

/* If EXPR is false, print and record an error for t including EXPR  */
#define TEST_CHECK(TEST, EXPR)                                  \
  test_check_((TEST), (EXPR), #EXPR, __FILE__, __LINE__, "")

/* If EXPR is false, print and record an error for t NOT including #EXPR  */
#define TEST_CHECKNF(TEST, EXPR, ...)                                   \
  test_check_((TEST), (EXPR), NULL, __FILE__, __LINE__, __VA_ARGS__)

bool test_etype_equal_(test_t *t, pn_event_type_t want, pn_event_type_t got, const char *file, int line) {
  return test_check_(t, want == got, NULL, file, line, "want %s got %s",
                     pn_event_type_name(want),
                     pn_event_type_name(got));
}
#define TEST_ETYPE_EQUAL(TEST, WANT, GOT) test_etype_equal_((TEST), (WANT), (GOT), __FILE__, __LINE__)

bool test_int_equal_(test_t *t, int want, int got, const char *file, int line) {
  return test_check_(t, want == got, NULL, file, line, "want %d, got %d", want, got);
}
#define TEST_INT_EQUAL(TEST, WANT, GOT) test_int_equal_((TEST), (WANT), (GOT), __FILE__, __LINE__)

bool test_size_equal_(test_t *t, size_t want, size_t got, const char *file, int line) {
  return test_check_(t, want == got, NULL, file, line, "want %zd, got %zd", want, got);
}
#define TEST_SIZE_EQUAL(TEST, WANT, GOT) test_size_equal_((TEST), (WANT), (GOT), __FILE__, __LINE__)

bool test_str_equal_(test_t *t, const char* want, const char* got, const char *file, int line) {
  return test_check_(t, !strcmp(want, got), NULL, file, line, "want '%s', got '%s'", want, got);
}
#define TEST_STR_EQUAL(TEST, WANT, GOT) test_str_equal_((TEST), (WANT), (GOT), __FILE__, __LINE__)

#define TEST_INSPECT(TEST, WANT, GOT) do {              \
    pn_string_t *s = pn_string(NULL);                   \
    TEST_ASSERT(0 == pn_inspect(GOT, s));               \
    TEST_STR_EQUAL((TEST), (WANT), pn_string_get(s));   \
    pn_free(s);                                         \
  } while (0)

#define TEST_STR_IN(TEST, WANT, GOT)                                    \
  test_check_((TEST), strstr((GOT), (WANT)), NULL, __FILE__, __LINE__, "'%s' not in '%s'", (WANT), (GOT))

#define TEST_COND_EMPTY(TEST, C)                                        \
  TEST_CHECKNF((TEST), (!(C) || !pn_condition_is_set(C)), "Unexpected condition - %s:%s", \
              pn_condition_get_name(C), pn_condition_get_description(C))

#define TEST_COND_DESC(TEST, WANT, C)                                   \
  (TEST_CHECKNF(t, pn_condition_is_set((C)), "No condition, expected :%s", (WANT)) ? \
   TEST_STR_IN(t, (WANT), pn_condition_get_description(C)) : 0);

#define TEST_COND_NAME(TEST, WANT, C)                                   \
  (TEST_CHECKNF(t, pn_condition_is_set((C)), "No condition, expected %s:", (WANT)) ? \
   TEST_STR_EQUAL(t, (WANT), pn_condition_get_name(C)) : 0);

#define TEST_CONDITION(TEST, NAME, DESC, C) do {        \
    TEST_COND_NAME(TEST, NAME, C);                      \
    TEST_COND_DESC(TEST, DESC, C);                      \
  } while(0)

/* T is name of a test_t variable, EXPR is the test expression (which should update T)
   FAILED is incremented if the test has errors
*/
#define RUN_TEST(FAILED, T, EXPR) do {                                  \
    fprintf(stderr, "TEST: %s\n", #EXPR);                               \
    fflush(stdout);                                                     \
    test_t T = { #EXPR, 0 };                                            \
    (EXPR);                                                             \
    if (T.errors && !t.inverted) {                                      \
      fprintf(stderr, "FAIL: %s (%d errors)\n", #EXPR, T.errors);       \
      ++(FAILED);                                                       \
    } else if (!T.errors && t.inverted) {                               \
      fprintf(stderr, "UNEXPECTED PASS: %s", #EXPR);                    \
      ++(FAILED);                                                       \
    }                                                                   \
  } while(0)

/* Like RUN_TEST but only if one of the argv strings is found in the test EXPR */
#define RUN_ARGV_TEST(FAILED, T, EXPR) do {     \
    if (argc == 1) {                            \
      RUN_TEST(FAILED, T, EXPR);                \
    } else {                                    \
      for (int i = 1; i < argc; ++i) {          \
        if (strstr(#EXPR, argv[i])) {           \
          RUN_TEST(FAILED, T, EXPR);            \
          break;                                \
        }                                       \
      }                                         \
    }                                           \
  } while(0)

/* Ensure buf has at least size bytes, use realloc if need be */
void rwbytes_ensure(pn_rwbytes_t *buf, size_t size) {
  if (buf->start == NULL || buf->size < size) {
    buf->start = (char*)realloc(buf->start, size);
    buf->size = size;
  }
}

static const size_t BUF_MIN = 1024;

/* Encode message m into buffer buf, return the size.
 * The buffer is expanded using realloc() if needed.
 */
size_t message_encode(pn_message_t* m, pn_rwbytes_t *buf) {
  int err = 0;
  rwbytes_ensure(buf, BUF_MIN);
  size_t size = buf->size;
  while ((err = pn_message_encode(m, buf->start, &size)) != 0) {
    if (err == PN_OVERFLOW) {
      rwbytes_ensure(buf, buf->size * 2);
      size = buf->size;
    } else {
      TEST_ASSERTF(err == 0, "encoding: %s %s", pn_code(err), pn_error_text(pn_message_error(m)));
    }
  }
  return size;
}

/* Decode message from delivery d into message m.
 * Use buf to hold intermediate message data, expand with realloc() if needed.
 */
void message_decode(pn_message_t *m, pn_delivery_t *d, pn_rwbytes_t *buf) {
  pn_link_t *l = pn_delivery_link(d);
  ssize_t size = pn_delivery_pending(d);
  rwbytes_ensure(buf, size);
  ssize_t result = pn_link_recv(l, buf->start, size);
  TEST_ASSERTF(size == result, "%ld != %ld", (long)size, (long)result);
  pn_message_clear(m);
  TEST_ASSERTF(!pn_message_decode(m, buf->start, size), "decode: %s", pn_error_text(pn_message_error(m)));
}

#endif // TESTS_TEST_TOOLS_H
