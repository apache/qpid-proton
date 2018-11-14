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

// Tests for the test framework

#include "./pn_test.hpp"

#include <proton/condition.h>
#include <proton/error.h>
#include <proton/event.h>

using namespace pn_test;

TEST_CASE("test_stringify") {
  std::ostringstream o;
  SECTION("event_type") {
    o << PN_CONNECTION_INIT;
    CHECK(o.str() == "PN_CONNECTION_INIT");
    CHECK(Catch::toString(PN_CONNECTION_INIT) == "PN_CONNECTION_INIT");
  }
  SECTION("condition") {
    pn_condition_t *c = pn_condition();
    SECTION("empty") {
      o << *c;
      CHECK(o.str() == "pn_condition{}");
      CHECK_THAT(*c, cond_empty());
    }
    SECTION("name-desc") {
      pn_condition_set_name(c, "foo");
      pn_condition_set_description(c, "bar");
      o << *c;
      CHECK(o.str() == "pn_condition{\"foo\", \"bar\"}");
      CHECK_THAT(*c, cond_matches("foo", "bar"));
      CHECK_THAT(*c, !cond_empty());
    }
    SECTION("desc-only") {
      pn_condition_set_name(c, "foo");
      o << *c;
      CHECK(o.str() == "pn_condition{\"foo\", null}");
      CHECK_THAT(*c, cond_matches("foo"));
    }
    pn_condition_free(c);
  }
  SECTION("error") {
    pn_error_t *err = pn_error();
    pn_error_format(err, PN_EOS, "foo");
    o << *err;
    pn_error_free(err);
    CHECK(o.str() == "pn_error{PN_EOS, \"foo\"}");
  }
}
