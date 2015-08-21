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

#include <proton/codec.h>
#include "../codec/data.h"
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

int main(int argc, char **argv) {
  test_grow();
}
