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

#include <proton/ssl.h>

#include "test_tools.h"

static void test_ssl_protocols(test_t *t)
{
  pn_ssl_domain_t *sd = pn_ssl_domain(PN_SSL_MODE_CLIENT);

  // Error no protocol set
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "")==PN_ARG_ERR);
  // Unknown protocol
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "blah")==PN_ARG_ERR);
  // Unknown protocol with known protocl prefix
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1.x")==PN_ARG_ERR);

  // known protocols
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1.1")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1.2")==0);

  // Multiple protocols
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.1 TLSv1.2")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.1")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1.1 TLSv1.2")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.2")==0);

  // Illegal separators
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1/TLSv1.1 TLSv1.2")==PN_ARG_ERR);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1-TLSv1.1 TLSv1.2")==PN_ARG_ERR);

  // Legal separators
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1,TLSv1.1;TLSv1.2  ; ")==0);
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1;TLSv1.1 TLSv1.2,,,,")==0);

  // Known followed by unknown protocols
  TEST_CHECK(t, pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.x;TLSv1_2")==PN_ARG_ERR);

  pn_ssl_domain_free(sd);
}

int main(int argc, char **argv) {
  int failed = 0;
  // Don't run these tests if ssl functionality wasn't compiled
  if (!pn_ssl_present()) {
    fprintf(stderr, "No SSL implementation to test\n");
    return failed;
  }
#if !defined(_WIN32)
  RUN_ARGV_TEST(failed, t, test_ssl_protocols(&t));
#endif
  return failed;
}
