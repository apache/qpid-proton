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

#include "./pn_test.hpp"

TEST_CASE("ssl_protocols") {
  pn_test::auto_free<pn_ssl_domain_t, pn_ssl_domain_free> sd(
      pn_ssl_domain(PN_SSL_MODE_CLIENT));

  // Error no protocol set
  CHECK(pn_ssl_domain_set_protocols(sd, "") == PN_ARG_ERR);
  // Unknown protocol
  CHECK(pn_ssl_domain_set_protocols(sd, "blah") == PN_ARG_ERR);
  // Unknown protocol with known protocl prefix
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1.x") == PN_ARG_ERR);

  // known protocols
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1.1") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1.2") == 0);

  // Multiple protocols
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.1 TLSv1.2") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.1") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1.1 TLSv1.2") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.2") == 0);

  // Illegal separators
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1/TLSv1.1 TLSv1.2") == PN_ARG_ERR);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1-TLSv1.1 TLSv1.2") == PN_ARG_ERR);

  // Legal separators
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1,TLSv1.1;TLSv1.2  ; ") == 0);
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1;TLSv1.1 TLSv1.2,,,,") == 0);

  // Known followed by unknown protocols
  CHECK(pn_ssl_domain_set_protocols(sd, "TLSv1 TLSv1.x;TLSv1_2") == PN_ARG_ERR);
}
