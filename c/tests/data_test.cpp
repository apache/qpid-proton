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

#include "core/data.h"

#include <proton/codec.h>
#include <proton/error.h>

#include <cstdarg>

using namespace pn_test;

// Check that pn_data_set_decode_limits() enforces a node-count cap.
TEST_CASE("data_decode_node_limit") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // Tighten the limit to 4 nodes
  pn_data_set_decode_limits(data, 4, 0);

  // Build and encode a list of 4 ints (should fit exactly)
  auto_free<pn_data_t, pn_data_free> src(pn_data(0));
  pn_data_put_list(src);
  pn_data_enter(src);
  for (int i = 0; i < 4; i++) pn_data_put_int(src, i);
  pn_data_exit(src);

  char buf[256];
  int enc = pn_data_encode(src, buf, sizeof(buf));
  REQUIRE(enc > 0);

  // Should decode successfully (4 nodes: 1 list + 4 ints, but list itself is 1
  // node and the 4 ints are children — total 5 nodes needed; lower to 5)
  pn_data_set_decode_limits(data, 5, 0);
  ssize_t r = pn_data_decode(data, buf, enc);
  CHECK(r == enc);
  CHECK(pn_data_errno(data) == 0);

  // Now tighten so the same data overflows
  pn_data_clear(data);
  pn_data_set_decode_limits(data, 3, 0);  // too few for list + 4 ints
  r = pn_data_decode(data, buf, enc);
  CHECK(r == PN_OUT_OF_MEMORY);
  CHECK(pn_data_errno(data) == PN_OUT_OF_MEMORY);

  // Limits survive pn_data_clear()
  pn_data_clear(data);
  CHECK(pn_data_errno(data) == 0); // error cleared
  // limits still in effect: re-decode should still fail
  r = pn_data_decode(data, buf, enc);
  CHECK(r == PN_OUT_OF_MEMORY);
}

// Check that pn_data_set_decode_limits() shrinks the backing node allocation
// when the new max_nid is lower than the current capacity.
TEST_CASE("data_decode_limit_shrinks_capacity") {
  // Pre-allocate a data object with a large capacity.
  auto_free<pn_data_t, pn_data_free> data(pn_data(64));
  pn_data_t *d = data;   // raw pointer for struct-field access
  // capacity should now be 64 (or the pre-allocated hint).
  CHECK(d->capacity == 64);

  // Lower the limit to 8.  The backing array must shrink to 8.
  pn_data_set_decode_limits(data, 8, 0);
  CHECK(d->max_nid == 8);
  CHECK(d->capacity == 8);   // realloc-to-smaller must have happened

  // Lowering below the number of live nodes must clamp to size, not max_nid.
  // Put 4 nodes in, then try to lower the limit to 2.
  for (int i = 0; i < 4; i++) pn_data_put_int(data, i);
  CHECK(pn_data_size(data) == 4);
  pn_data_set_decode_limits(data, 2, 0);
  CHECK(d->max_nid == 2);
  // capacity must not have been reduced below the 4 live nodes.
  CHECK(d->capacity >= 4);
  // And the existing nodes must still be intact.
  CHECK(pn_data_size(data) == 4);

  // Setting max_nid = 0 (unlimited) must NOT shrink to zero nodes.
  pn_data_set_decode_limits(data, 0, 0);
  CHECK(d->capacity >= 4);   // live nodes still accessible
  CHECK(pn_data_size(data) == 4);
}

// Check that pn_data_set_decode_limits() enforces a string-buffer cap.
TEST_CASE("data_decode_buf_limit") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // Encode a symbol of 20 bytes
  auto_free<pn_data_t, pn_data_free> src(pn_data(0));
  pn_data_put_symbol(src, pn_bytes("12345678901234567890"));

  char buf[256];
  int enc = pn_data_encode(src, buf, sizeof(buf));
  REQUIRE(enc > 0);

  // Allow plenty of nodes but only 10 bytes of string buffer — should fail
  pn_data_set_decode_limits(data, 0, 10);
  ssize_t r = pn_data_decode(data, buf, enc);
  CHECK(r == PN_OUT_OF_MEMORY);
  CHECK(pn_data_errno(data) == PN_OUT_OF_MEMORY);

  // Raise the buf limit enough — should succeed
  pn_data_clear(data);
  pn_data_set_decode_limits(data, 0, 64);
  r = pn_data_decode(data, buf, enc);
  CHECK(r == enc);
  CHECK(pn_data_errno(data) == 0);

  // Disable both limits (0 = no limit) — should always succeed
  pn_data_clear(data);
  pn_data_set_decode_limits(data, 0, 0);
  r = pn_data_decode(data, buf, enc);
  CHECK(r == enc);
  CHECK(pn_data_errno(data) == 0);
}

// Make sure we can grow the capacity of a pn_data_t all the way to the hard
// PNI_NID_MAX ceiling and we stop there (decode-limits disabled for this test).
TEST_CASE("data_grow") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));
  // Disable the decode-limits so we can exercise the absolute uint16 ceiling.
  pn_data_set_decode_limits(data, 0, 0);
  int code = 0;
  while (pn_data_size(data) < PNI_NID_MAX && !code) {
    code = pn_data_put_int(data, 1);
  }
  CHECK_THAT(*pn_data_error(data), error_empty());
  CHECK(pn_data_size(data) == PNI_NID_MAX);
  code = pn_data_put_int(data, 1);
  INFO(pn_code(code));
  CHECK(code == PN_OUT_OF_MEMORY);
  CHECK(pn_data_size(data) == PNI_NID_MAX);
}

TEST_CASE("data_multiple") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(1));
  auto_free<pn_data_t, pn_data_free> src(pn_data(1));

  /* NULL data pointer */
  pn_data_fill(data, "M", NULL);
  CHECK("null" == inspect(data));

  /* Empty data object */
  pn_data_clear(data);
  pn_data_fill(data, "M", src.get());
  CHECK("null" == inspect(data));

  /* Empty array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_fill(data, "M", src.get());
  CHECK("null" == inspect(data));

  /* Single-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, pn_bytes("foo"));
  pn_data_fill(data, "M", src.get());
  CHECK(":foo" == inspect(data));

  /* Multi-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, pn_bytes("foo"));
  pn_data_put_symbol(src, pn_bytes("bar"));
  pn_data_fill(data, "M", src.get());
  CHECK("@PN_SYMBOL[:foo, :bar]" == inspect(data));

  /* Non-array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_symbol(src, pn_bytes("baz"));
  pn_data_fill(data, "M", src.get());
  CHECK(":baz" == inspect(data));
}

TEST_CASE("data_described_list") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(1));

  /* Described list with open frame descriptor */
  pn_data_clear(data);
  pn_data_fill(data, "DL[]", (uint64_t)16);
  CHECK("@open(16) []" == inspect(data));

  /* open frame with some fields */
  pn_data_clear(data);
  pn_data_fill(data, "DL[SSnI]", (uint64_t)16, "container-1", nullptr, 965);
  CHECK("@open(16) [container-id=\"container-1\", channel-max=965]" == inspect(data));

  /* Described list with items after the list */
  pn_data_clear(data);
  pn_data_fill(data, "DL[SSnI]S", (uint64_t)16, "container-1", nullptr, 965, "extra");
  CHECK("@open(16) [container-id=\"container-1\", channel-max=965], \"extra\"" == inspect(data));

  /* Conditional Described list cases */
  pn_data_clear(data);
  pn_data_fill(data, "?DL[SSnI]S", false, (uint64_t)16, "container-1", nullptr, 965, "extra");
  CHECK("null, \"extra\"" == inspect(data));

  pn_data_clear(data);
  pn_data_fill(data, "?DL[?SSnI]?S", true, (uint64_t)16, false, "container-1", nullptr, 965, true, "extra");
  CHECK("@open(16) [channel-max=965], \"extra\"" == inspect(data));
}

TEST_CASE("data_map") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(1));

  /* Map */
  pn_data_clear(data);
  pn_data_fill(data, "{S[iii]SI}", "foo", 1, 987, 3, "bar", 965);
  CHECK("{\"foo\"=[1, 987, 3], \"bar\"=965}" == inspect(data));
}


#define BUFSIZE 1024
static void check_encode_decode(auto_free<pn_data_t, pn_data_free>& src) {
	char buf[BUFSIZE];
	auto_free<pn_data_t, pn_data_free> data(pn_data(1));
	pn_data_clear(data);

	// Encode src array to buf
	int enc_size = pn_data_encode(src, buf, BUFSIZE - 1);
	if (enc_size < 0) {
		FAIL("pn_data_encode() error " << enc_size << ": " << pn_code(enc_size));
	}

	// Decode buf to data
	int dec_size = pn_data_decode(data, buf, BUFSIZE - 1);
	pn_error_t *dec_err = pn_data_error(data);
	CHECK(pn_error_code(dec_err) == 0);
	if (dec_size < 0) {
		FAIL("pn_data_decode() error " << dec_size << ": " << pn_code(dec_size));
	}

	// Checks
	CHECK(enc_size == dec_size);
	CHECK(inspect(src) == inspect(data));
}

static void check_array(const char *fmt, ...) {
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);

	// Create src array
	va_list ap;
	va_start(ap, fmt);
	pn_data_vfill(src, fmt, ap);
	va_end(ap);

	check_encode_decode(src);
}

TEST_CASE("array_list") {
	check_array("@T[]", PN_LIST);
	// TODO: PROTON-2248: using S and s reversed
	// empty list as first array element
	check_array("@T[[][oo][][iii][Sosid]]", PN_LIST, true, false, 1, 2, 3, "hello", false, "world", 43210, 2.565e-56);
	// empty list not as first array element
	check_array("@T[[Sid][oooo][]]", PN_LIST, "aaa", 123, double(3.2415), true, true, false, true);
	// only empty lists
	check_array("@T[[][][][][]]", PN_LIST);
}
