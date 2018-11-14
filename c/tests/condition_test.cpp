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

#include <proton/condition.h>
#include <proton/connection.h>

using Catch::Matchers::Equals;
using namespace pn_test;

TEST_CASE("condition") {
  auto_free<pn_connection_t, pn_connection_free> c(pn_connection());
  REQUIRE(c);
  pn_condition_t *cond = pn_connection_condition(c);
  REQUIRE(cond);

  // Verify empty
  CHECK(!pn_condition_is_set(cond));
  CHECK(!pn_condition_get_name(cond));
  CHECK(!pn_condition_get_description(cond));

  // Format a condition
  pn_condition_format(cond, "foo", "hello %d", 42);
  CHECK(pn_condition_is_set(cond));
  CHECK_THAT("foo", Equals(pn_condition_get_name(cond)));
  CHECK_THAT("hello 42", Equals(pn_condition_get_description(cond)));

  // Clear and verify empty
  pn_condition_clear(cond);
  CHECK(!pn_condition_is_set(cond));
  CHECK(!pn_condition_get_name(cond));
  CHECK(!pn_condition_get_description(cond));
}
