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

// Make sure we can grow the capacity of a pn_data_t all the way to the max and
// we stop there.
TEST_CASE("data_grow") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));
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

  /* Described list with open frame descriptor */
  pn_data_clear(data);
  pn_data_fill(data, "DL[]", (uint64_t)16);
  CHECK("@open(16) []" == inspect(data));

  /* open frame with some fields */
  pn_data_clear(data);
  pn_data_fill(data, "DL[SSnI]", (uint64_t)16, "container-1", 0, 965);
  CHECK("@open(16) [container-id=\"container-1\", channel-max=965]" == inspect(data));

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
