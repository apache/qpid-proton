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

#include "./pn_test.hpp"

#include <proton/error.h>

using Catch::Matchers::Equals;
using namespace pn_test;

TEST_CASE("error_set_and_clear") {
  auto_free<pn_error_t, pn_error_free> err(pn_error());
  REQUIRE(err);

  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));

  CHECK(pn_error_set(err, PN_ERR, "oops") == PN_ERR);
  CHECK(pn_error_code(err) == PN_ERR);
  CHECK_THAT("oops", Equals(pn_error_text(err)));

  pn_error_clear(err);
  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));

  // A zero code clears the error, the text is discarded
  CHECK(pn_error_format(err, PN_EOS, "%s %d", "bye", 42) == PN_EOS);
  CHECK_THAT("bye 42", Equals(pn_error_text(err)));
  CHECK(pn_error_set(err, 0, "ignored") == 0);
  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));
}

// pn_error_set() must copy the text before releasing the old text, in case the
// caller passed in the error's own text. The failure is a use-after-free, so
// run this under valgrind or a sanitizer to see it.
TEST_CASE("error_set_own_text") {
  auto_free<pn_error_t, pn_error_free> err(pn_error());
  REQUIRE(err);

  REQUIRE(pn_error_set(err, PN_ERR, "self reference") == PN_ERR);
  CHECK(pn_error_set(err, PN_TIMEOUT, pn_error_text(err)) == PN_TIMEOUT);
  CHECK(pn_error_code(err) == PN_TIMEOUT);
  CHECK_THAT("self reference", Equals(pn_error_text(err)));

  // Same again via pn_error_format(), which formats into a local buffer first
  CHECK(pn_error_format(err, PN_EOS, "<%s>", pn_error_text(err)) == PN_EOS);
  CHECK_THAT("<self reference>", Equals(pn_error_text(err)));
}

TEST_CASE("error_copy") {
  auto_free<pn_error_t, pn_error_free> err(pn_error());
  auto_free<pn_error_t, pn_error_free> src(pn_error());
  REQUIRE(err);
  REQUIRE(src);

  pn_error_set(src, PN_ARG_ERR, "bad arg");
  CHECK(pn_error_copy(err, src) == PN_ARG_ERR);
  CHECK(pn_error_code(err) == PN_ARG_ERR);
  CHECK_THAT("bad arg", Equals(pn_error_text(err)));
  // The copy is independent of the source
  CHECK(pn_error_text(err) != pn_error_text(src));

  // Copying from an unset error clears the destination
  pn_error_clear(src);
  CHECK(pn_error_copy(err, src) == 0);
  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));

  // A NULL source clears the destination
  pn_error_set(err, PN_ERR, "wiped");
  CHECK(pn_error_copy(err, NULL) == 0);
  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));
}

// Copying an error onto itself must leave it unchanged, not free its own text.
TEST_CASE("error_copy_self") {
  auto_free<pn_error_t, pn_error_free> err(pn_error());
  REQUIRE(err);

  REQUIRE(pn_error_set(err, PN_STATE_ERR, "copy me") == PN_STATE_ERR);
  CHECK(pn_error_copy(err, err) == PN_STATE_ERR);
  CHECK(pn_error_code(err) == PN_STATE_ERR);
  CHECK_THAT("copy me", Equals(pn_error_text(err)));

  // Self copy of an unset error is a no-op
  pn_error_clear(err);
  CHECK(pn_error_copy(err, err) == 0);
  CHECK(pn_error_code(err) == 0);
  CHECK(!pn_error_text(err));
}
