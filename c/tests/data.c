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

#undef NDEBUG                   /* Make sure that assert() is enabled even in a release build. */

#include "test_tools.h"
#include "core/data.h"

#include <proton/codec.h>
#include <assert.h>
#include <stdio.h>

// Make sure we can grow the capacity of a pn_data_t all the way to the max and we stop there.
static void test_grow(void)
{
  pn_data_t* data = pn_data(0);
  while (pn_data_size(data) < PNI_NID_MAX) {
    int code = pn_data_put_int(data, 1);
    if (code) fprintf(stderr, "%d: %s", code, pn_error_text(pn_data_error(data)));
    assert(code == 0);
  }
  assert(pn_data_size(data) == PNI_NID_MAX);
  int code = pn_data_put_int(data, 1);
  if (code != PN_OUT_OF_MEMORY)
    fprintf(stderr, "expected PN_OUT_OF_MEMORY, got  %s\n", pn_code(code));
  assert(code == PN_OUT_OF_MEMORY);
  assert(pn_data_size(data) == PNI_NID_MAX);
  pn_data_free(data);
}

static void test_multiple(test_t *t) {
  pn_data_t *data = pn_data(1);
  pn_data_t *src = pn_data(1);

  /* NULL data pointer */
  pn_data_fill(data, "M", NULL);
  TEST_INSPECT(t, "null", data);

  /* Empty data object */
  pn_data_clear(data);
  pn_data_fill(data, "M", src);
  TEST_INSPECT(t, "null", data);

  /* Empty array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_fill(data, "M", src);
  TEST_INSPECT(t, "null", data);

  /* Single-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, PN_BYTES_LITERAL(foo));
  pn_data_fill(data, "M", src);
  TEST_INSPECT(t, ":foo", data);

  /* Multi-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, PN_BYTES_LITERAL(foo));
  pn_data_put_symbol(src, PN_BYTES_LITERAL(bar));
  pn_data_fill(data, "M", src);
  TEST_INSPECT(t, "@PN_SYMBOL[:foo, :bar]", data);

  /* Non-array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_symbol(src, PN_BYTES_LITERAL(baz));
  pn_data_fill(data, "M", src);
  TEST_INSPECT(t, ":baz", data);

  pn_data_free(data);
  pn_data_free(src);
}

int main(int argc, char **argv) {
  int failed = 0;
  test_grow();
  RUN_ARGV_TEST(failed, t, test_multiple(&t));
  return failed;
}
