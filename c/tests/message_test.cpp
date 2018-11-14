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

#include <proton/error.h>
#include <proton/message.h>
#include <stdarg.h>

using namespace pn_test;

TEST_CASE("message_overflow_error") {
  pn_message_t *message = pn_message();
  char buf[6];
  size_t size = 6;

  int err = pn_message_encode(message, buf, &size);
  CHECK(PN_OVERFLOW == err);
  CHECK(0 == pn_message_errno(message));
  pn_message_free(message);
}

static void recode(pn_message_t *dst, pn_message_t *src) {
  pn_rwbytes_t buf = {0};
  int size = pn_message_encode2(src, &buf);
  REQUIRE(size > 0);
  pn_message_decode(dst, buf.start, size);
  free(buf.start);
}

TEST_CASE("message_inferred") {
  pn_message_t *src = pn_message();
  pn_message_t *dst = pn_message();

  CHECK(!pn_message_is_inferred(src));
  pn_data_put_binary(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  CHECK(pn_message_is_inferred(dst));

  pn_message_clear(src);
  CHECK(!pn_message_is_inferred(src));
  pn_data_put_list(pn_message_body(src));
  pn_data_enter(pn_message_body(src));
  pn_data_put_binary(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  CHECK(pn_message_is_inferred(dst));

  pn_message_clear(src);
  CHECK(!pn_message_is_inferred(src));
  pn_data_put_string(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  /* A value section is never considered to be inferred */
  CHECK(!pn_message_is_inferred(dst));

  pn_message_free(src);
  pn_message_free(dst);
}
