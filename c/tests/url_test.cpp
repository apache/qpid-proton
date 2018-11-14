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

#define PN_USE_DEPRECATED_API 1

#include "./pn_test.hpp"

#include "proton/error.h"
#include "proton/type_compat.h"
#include "proton/url.h"

using namespace pn_test;
using Catch::Matchers::Equals;

void check_url(const char *url, const char *scheme, const char *user,
               const char *pass, const char *host, const char *port,
               const char *path, bool round_trip = true) {
  INFO("url=\"" << url << '"');
  auto_free<pn_url_t, pn_url_free> purl(pn_url_parse(url));
  CHECK_THAT(scheme, Equals(pn_url_get_scheme(purl)));
  CHECK_THAT(user, Equals(pn_url_get_username(purl)));
  CHECK_THAT(pass, Equals(pn_url_get_password(purl)));
  CHECK_THAT(host, Equals(pn_url_get_host(purl)));
  CHECK_THAT(port, Equals(pn_url_get_port(purl)));
  CHECK_THAT(path, Equals(pn_url_get_path(purl)));
  if (round_trip) CHECK_THAT(url, Equals(pn_url_str(purl)));
}

TEST_CASE("url") {
  const char *null = 0;
  check_url("/Foo.bar:90087@somewhere", null, null, null, null, null,
            "Foo.bar:90087@somewhere");
  check_url("host", null, null, null, "host", null, null);
  check_url("host:423", null, null, null, "host", "423", null);
  check_url("user@host", null, "user", null, "host", null, null);

  // Can't round-trip passwords with ':', not strictly legal
  // but the parser allows it.
  check_url("user:1243^&^:pw@host:423", null, "user", "1243^&^:pw", "host",
            "423", null, false);
  check_url("user:1243^&^:pw@host:423/Foo.bar:90087", null, "user",
            "1243^&^:pw", "host", "423", "Foo.bar:90087", false);
  check_url("user:1243^&^:pw@host:423/Foo.bar:90087@somewhere", null, "user",
            "1243^&^:pw", "host", "423", "Foo.bar:90087@somewhere", false);

  check_url("[::1]:amqp", null, null, null, "::1", "amqp", null);
  check_url("user@[::1]", null, "user", null, "::1", null, null);
  check_url("user@[::1]:amqp", null, "user", null, "::1", "amqp", null);

  // Can't round-trip passwords with ':', not strictly legal
  // but the parser allows it.
  check_url("user:1243^&^:pw@[::1]:amqp", null, "user", "1243^&^:pw", "::1",
            "amqp", null, false);
  check_url("user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", null, "user",
            "1243^&^:pw", "::1", "amqp", "Foo.bar:90087", false);
  check_url("user:1243^&^:pw@[::1:amqp/Foo.bar:90087", null, "user",
            "1243^&^:pw", "[::1", "amqp", "Foo.bar:90087", false);
  check_url("user:1243^&^:pw@::1]:amqp/Foo.bar:90087", null, "user",
            "1243^&^:pw", "::1]", "amqp", "Foo.bar:90087", false);

  check_url("amqp://user@[::1]", "amqp", "user", null, "::1", null, null);
  check_url("amqp://user@[::1]:amqp", "amqp", "user", null, "::1", "amqp",
            null);
  check_url("amqp://user@[1234:52:0:1260:f2de:f1ff:fe59:8f87]:amqp", "amqp",
            "user", null, "1234:52:0:1260:f2de:f1ff:fe59:8f87", "amqp", null);

  // Can't round-trip passwords with ':', not strictly legal
  // but the parser allows it.
  check_url("amqp://user:1243^&^:pw@[::1]:amqp", "amqp", "user", "1243^&^:pw",
            "::1", "amqp", null, false);
  check_url("amqp://user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", "amqp", "user",
            "1243^&^:pw", "::1", "amqp", "Foo.bar:90087", false);

  check_url("amqp://host", "amqp", null, null, "host", null, null);
  check_url("amqp://user@host", "amqp", "user", null, "host", null, null);
  check_url("amqp://user@host/path:%", "amqp", "user", null, "host", null,
            "path:%");
  check_url("amqp://user@host:5674/path:%", "amqp", "user", null, "host",
            "5674", "path:%");
  check_url("amqp://user@host/path:%", "amqp", "user", null, "host", null,
            "path:%");
  check_url("amqp://bigbird@host/queue@host", "amqp", "bigbird", null, "host",
            null, "queue@host");
  check_url("amqp://host/queue@host", "amqp", null, null, "host", null,
            "queue@host");
  check_url("amqp://host:9765/queue@host", "amqp", null, null, "host", "9765",
            "queue@host");
  check_url("user:pass%2fword@host", null, "user", "pass/word", "host", null,
            null, false);
  check_url("user:pass%2Fword@host", null, "user", "pass/word", "host", null,
            null);
  // Can't round-trip passwords with lowercase hex encoding
  check_url("us%2fer:password@host", null, "us/er", "password", "host", null,
            null, false);
  check_url("us%2Fer:password@host", null, "us/er", "password", "host", null,
            null);
  // Can't round-trip passwords with lowercase hex encoding
  check_url("user:pass%2fword%@host", null, "user", "pass/word%", "host", null,
            null, false);
  check_url("localhost/temp-queue://"
            "ID:ganymede-36663-1408448359876-2:123:0",
            null, null, null, "localhost", null,
            "temp-queue://ID:ganymede-36663-1408448359876-2:123:0");
  check_url("/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", null, null,
            null, null, null,
            "temp-queue://ID:ganymede-36663-1408448359876-2:123:0");
  check_url("amqp://localhost/temp-queue://"
            "ID:ganymede-36663-1408448359876-2:123:0",
            "amqp", null, null, "localhost", null,
            "temp-queue://ID:ganymede-36663-1408448359876-2:123:0");
  // PROTON-995
  check_url("amqps://%40user%2F%3A:%40pass%2F%3A@example.net/"
            "some_topic",
            "amqps", "@user/:", "@pass/:", "example.net", null, "some_topic");
  check_url("amqps://user%2F%3A=:pass%2F%3A=@example.net/some_topic", "amqps",
            "user/:=", "pass/:=", "example.net", null, "some_topic");
  // Really perverse url
  check_url("://:@://:", "", "", "", null, "", "/:");
}
