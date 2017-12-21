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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proton/error.h>
#include <proton/message.h>

#define assert(E) ((E) ? 0 : (abort(), 0))

static void test_overflow_error(void)
{
  pn_message_t *message = pn_message();
  char buf[8];
  size_t size = 8;

  int err = pn_message_encode(message, buf, &size);
  assert(err == PN_OVERFLOW);
  assert(pn_message_errno(message) == 0);
  pn_message_free(message);
}

int main(int argc, char **argv)
{
  test_overflow_error();
  return 0;
}
