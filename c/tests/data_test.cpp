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
#include "core/value_dump.h"

#include <proton/codec.h>
#include <proton/error.h>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace pn_test;

// Compare the semantic content of two encoded AMQP byte buffers using pn_value_dump(),
// the same textifying path used for frame tracing. This tolerates the encoder legitimately
// choosing a different (but equivalent) encoding width on re-encode, e.g. LIST0 instead of
// an empty LIST8.
static void check_roundtrip(pn_bytes_t initial, pn_bytes_t final_bytes) {
  char initial_buf[256];
  char final_buf[256];
  pn_value_dump(initial, initial_buf, sizeof(initial_buf));
  pn_value_dump(final_bytes, final_buf, sizeof(final_buf));
  CHECK(std::string(initial_buf) == std::string(final_buf));
}

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

TEST_CASE("data_decode_single_nested_described_type") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // described(amqp-value) whose value is itself a described list
  // 0x00 0x53 0x77 0x00 0x53 0x31 0x45
  const uint8_t encoded[] = {
    0x00, 0x53, 0x77,
    0x00, 0x53, 0x31,
    0x45
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == (ssize_t) sizeof(encoded));
  CHECK(pn_data_errno(data) == 0);

  char roundtrip[32];
  int enc = pn_data_encode(data, roundtrip, sizeof(roundtrip));
  REQUIRE(enc > 0);
  check_roundtrip(pn_bytes(sizeof(encoded), (const char *) encoded), pn_bytes((size_t) enc, roundtrip));
}

TEST_CASE("data_decode_rejects_deep_described_chain") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // three chained described constructors: only one nested described value is allowed.
  const uint8_t encoded[] = {
    0x00, 0x53, 0x77,
    0x00, 0x53, 0x31,
    0x00, 0x53, 0x32,
    0x40
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == PN_ARG_ERR);
}

TEST_CASE("data_decode_described_empty_list") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // @ulong(0x77):list() where the empty list is encoded as LIST8 (not the
  // compact LIST0 form), to exercise the container-open/immediately-empty path.
  // 0x00 0x53 0x77 0xc0 0x01 0x00
  const uint8_t encoded[] = {
    0x00, 0x53, 0x77, 0xc0, 0x01, 0x00
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == (ssize_t) sizeof(encoded));
  CHECK(pn_data_errno(data) == 0);

  char roundtrip[32];
  int enc = pn_data_encode(data, roundtrip, sizeof(roundtrip));
  REQUIRE(enc > 0);
  check_roundtrip(pn_bytes(sizeof(encoded), (const char *) encoded), pn_bytes((size_t) enc, roundtrip));
}

TEST_CASE("data_decode_described_empty_list_in_list") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // [ @ulong(0x77):list(), 5 ]
  const uint8_t encoded[] = {
    0xc0, 0x09, 0x02,
      0x00, 0x53, 0x77, 0xc0, 0x01, 0x00,
      0x52, 0x05
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == (ssize_t) sizeof(encoded));
  CHECK(pn_data_errno(data) == 0);
  CHECK("[@amqp-value(119) [], 5]" == inspect(data));

  char roundtrip[32];
  int enc = pn_data_encode(data, roundtrip, sizeof(roundtrip));
  REQUIRE(enc > 0);
  check_roundtrip(pn_bytes(sizeof(encoded), (const char *) encoded), pn_bytes((size_t) enc, roundtrip));
}

TEST_CASE("data_decode_described_empty_map") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // @ulong(0x77):map() where the empty map is encoded as MAP8.
  // 0x00 0x53 0x77 0xc1 0x01 0x00
  const uint8_t encoded[] = {
    0x00, 0x53, 0x77, 0xc1, 0x01, 0x00
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == (ssize_t) sizeof(encoded));
  CHECK(pn_data_errno(data) == 0);

  char roundtrip[32];
  int enc = pn_data_encode(data, roundtrip, sizeof(roundtrip));
  REQUIRE(enc > 0);
  check_roundtrip(pn_bytes(sizeof(encoded), (const char *) encoded), pn_bytes((size_t) enc, roundtrip));
}

// Hand-build `depth` nested AMQP list32 frames as raw wire bytes: [0xd0][size:4 BE][count:4 BE],
// innermost count 0, each outer one wrapping the next as its single element. This lets us drive
// the decoder to a nesting depth no pn_data_t-based construction can conveniently reach, without
// relying on any recursive helper on the encode side.
static std::vector<uint8_t> build_nested_list32(uint32_t depth) {
  const size_t per = 9; // 1 tag + 4 size + 4 count
  std::vector<uint8_t> buf(depth * per);
  uint8_t *p = buf.data();
  for (uint32_t i = 0; i < depth; i++) {
    uint32_t count = (i == depth - 1) ? 0 : 1;
    uint32_t size = 4 + (uint32_t) ((depth - 1 - i) * per); // covers count(4) + nested content
    *p++ = 0xd0; // PNE_LIST32
    p[0] = (uint8_t) (size >> 24); p[1] = (uint8_t) (size >> 16); p[2] = (uint8_t) (size >> 8); p[3] = (uint8_t) size;
    p += 4;
    p[0] = (uint8_t) (count >> 24); p[1] = (uint8_t) (count >> 16); p[2] = (uint8_t) (count >> 8); p[3] = (uint8_t) count;
    p += 4;
  }
  return buf;
}

TEST_CASE("data_decode_deep_nesting") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // Depth well beyond what any fixed recursion-depth heuristic would allow, but
  // comfortably under the absolute PNI_NID_MAX node-id ceiling (2^16 - 1), so the
  // only limit in play is the (disabled) node-count limit below. The iterative
  // decoder keeps the nesting in its own heap-backed node storage, so a value
  // this deeply nested decodes normally.
  const uint32_t depth = 20000;
  std::vector<uint8_t> buf = build_nested_list32(depth);

  pn_data_set_decode_limits(data, 0, 0); // unlimited: isolate depth handling from node budget
  ssize_t dec = pn_data_decode(data, (const char *) buf.data(), buf.size());
  CHECK(dec == (ssize_t) buf.size());
  CHECK(pn_data_errno(data) == 0);
}

TEST_CASE("data_decode_rejects_deeply_nested_values_on_node_limit") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // Same deep-nesting shape as above, but now with a tiny node budget: decoding
  // must fail cleanly with PN_OUT_OF_MEMORY rather than fail some other way or
  // succeed, proving that node count is what bounds nesting.
  const uint32_t depth = 20000;
  std::vector<uint8_t> buf = build_nested_list32(depth);

  pn_data_set_decode_limits(data, 10, 0);
  ssize_t dec = pn_data_decode(data, (const char *) buf.data(), buf.size());
  CHECK(dec == PN_OUT_OF_MEMORY);
  CHECK(pn_data_errno(data) == PN_OUT_OF_MEMORY);
}

TEST_CASE("data_decode_into_entered_container") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));

  // Decoding appends to wherever the pn_data_t is positioned, including inside
  // a container the caller has entered: exactly one value is decoded and the
  // caller's position is left as it was.
  REQUIRE(pn_data_put_list(data) == 0);
  REQUIRE(pn_data_enter(data));

  const uint8_t encoded[] = {
    0xc0, 0x04, 0x02, 0x52, 0x05, 0x40  // [5, null]
  };

  ssize_t dec = pn_data_decode(data, (const char *) encoded, sizeof(encoded));
  CHECK(dec == (ssize_t) sizeof(encoded));
  CHECK(pn_data_errno(data) == 0);

  REQUIRE(pn_data_exit(data));
  pn_data_rewind(data);
  CHECK("[[5, null]]" == inspect(data));
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
