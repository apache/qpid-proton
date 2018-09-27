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

#include "test_tools.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proton/error.h>
#include <proton/message.h>

#define assert(E) ((E) ? 0 : (abort(), 0))

static void test_overflow_error(test_t *t)
{
  pn_message_t *message = pn_message();
  char buf[6];
  size_t size = 6;

  int err = pn_message_encode(message, buf, &size);
  TEST_INT_EQUAL(t,PN_OVERFLOW, err);
  TEST_INT_EQUAL(t, 0, pn_message_errno(message));
  pn_message_free(message);
}

static void recode(pn_message_t *dst, pn_message_t *src) {
  pn_rwbytes_t buf = { 0 };
  int size = pn_message_encode2(src, &buf);
  assert(size > 0);
  pn_message_decode(dst, buf.start, size);
  free(buf.start);
}

static void test_inferred(test_t *t) {
  pn_message_t *src = pn_message();
  pn_message_t *dst = pn_message();

  TEST_CHECK(t, !pn_message_is_inferred(src));
  pn_data_put_binary(pn_message_body(src), PN_BYTES_LITERAL("hello"));
  recode(dst, src);
  TEST_CHECK(t, !pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  TEST_CHECK(t, pn_message_is_inferred(dst));

  pn_message_clear(src);
  TEST_CHECK(t, !pn_message_is_inferred(src));
  pn_data_put_list(pn_message_body(src));
  pn_data_enter(pn_message_body(src));
  pn_data_put_binary(pn_message_body(src), PN_BYTES_LITERAL("hello"));
  recode(dst, src);
  TEST_CHECK(t, !pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  TEST_CHECK(t, pn_message_is_inferred(dst));

  pn_message_clear(src);
  TEST_CHECK(t, !pn_message_is_inferred(src));
  pn_data_put_string(pn_message_body(src), PN_BYTES_LITERAL("hello"));
  recode(dst, src);
  TEST_CHECK(t, !pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  /* A value section is never considered to be inferred */
  TEST_CHECK(t, !pn_message_is_inferred(dst));

  pn_message_free(src);
  pn_message_free(dst);
}

int main(int argc, char **argv)
{
  int failed = 0;
  RUN_ARGV_TEST(failed, t, test_overflow_error(&t));
  RUN_ARGV_TEST(failed, t, test_inferred(&t));
  return 0;
}
