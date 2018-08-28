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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proton/type_compat.h"
#include "proton/error.h"
#include "proton/url.h"

static bool verify(const char* what, const char* want, const char* got) {
  bool eq = (want == got || (want && got && strcmp(want, got) == 0));
  if (!eq) printf("  %s: '%s' != '%s'\n", what, want, got);
  return eq;
}

static bool test(const char* url, const char* scheme, const char* user, const char* pass, const char* host, const char* port, const char*path)
{
  pn_url_t *purl = pn_url_parse(url);
  bool ok =
    verify("scheme", scheme, pn_url_get_scheme(purl)) &&
    verify("user", user, pn_url_get_username(purl)) &&
    verify("pass", pass, pn_url_get_password(purl)) &&
    verify("host", host, pn_url_get_host(purl)) &&
    verify("port", port, pn_url_get_port(purl)) &&
    verify("path", path, pn_url_get_path(purl));
  pn_url_free(purl);
  return ok;
}

// Run test and additionally verify the round trip of parse and stringify
// matches original string.
static bool testrt(const char* url, const char* scheme, const char* user, const char* pass, const char* host, const char* port, const char*path)
{
  bool ok = test(url, scheme, user, pass, host, port, path);
  pn_url_t *purl = pn_url_parse(url);
  ok = ok && verify("url", url, pn_url_str(purl));
  pn_url_free(purl);
  return ok;
}

#define TEST(EXPR) \
  do { if (!(EXPR)) { printf("%s:%d: %s\n\n", __FILE__, __LINE__, #EXPR); failed++; } } while(0)

int main(int argc, char **argv)
{
  int failed = 0;
  TEST(testrt("/Foo.bar:90087@somewhere", 0, 0, 0, 0, 0, "Foo.bar:90087@somewhere"));
  TEST(testrt("host", 0, 0, 0, "host", 0, 0));
  TEST(testrt("host:423", 0, 0, 0, "host", "423", 0));
  TEST(testrt("user@host", 0, "user", 0, "host", 0, 0));

  // Can't round-trip passwords with ':', not strictly legal but the parser allows it.
  TEST(test("user:1243^&^:pw@host:423", 0, "user", "1243^&^:pw", "host", "423", 0));
  TEST(test("user:1243^&^:pw@host:423/Foo.bar:90087", 0, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087"));
  TEST(test("user:1243^&^:pw@host:423/Foo.bar:90087@somewhere", 0, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087@somewhere"));

  TEST(testrt("[::1]", 0, 0, 0, "::1", 0, 0));
  TEST(testrt("[::1]:amqp", 0, 0, 0, "::1", "amqp", 0));
  TEST(testrt("user@[::1]", 0, "user", 0, "::1", 0, 0));
  TEST(testrt("user@[::1]:amqp", 0, "user", 0, "::1", "amqp", 0));

  // Can't round-trip passwords with ':', not strictly legal but the parser allows it.
  TEST(test("user:1243^&^:pw@[::1]:amqp", 0, "user", "1243^&^:pw", "::1", "amqp", 0));
  TEST(test("user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087"));
  TEST(test("user:1243^&^:pw@[::1:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "[::1", "amqp", "Foo.bar:90087"));
  TEST(test("user:1243^&^:pw@::1]:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "::1]", "amqp", "Foo.bar:90087"));

  TEST(testrt("amqp://user@[::1]", "amqp", "user", 0, "::1", 0, 0));
  TEST(testrt("amqp://user@[::1]:amqp", "amqp", "user", 0, "::1", "amqp", 0));
  TEST(testrt("amqp://user@[1234:52:0:1260:f2de:f1ff:fe59:8f87]:amqp", "amqp", "user", 0, "1234:52:0:1260:f2de:f1ff:fe59:8f87", "amqp", 0));

  // Can't round-trip passwords with ':', not strictly legal but the parser allows it.
  TEST(test("amqp://user:1243^&^:pw@[::1]:amqp", "amqp", "user", "1243^&^:pw", "::1", "amqp", 0));
  TEST(test("amqp://user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", "amqp", "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087"));

  TEST(testrt("amqp://host", "amqp", 0, 0, "host", 0, 0));
  TEST(testrt("amqp://user@host", "amqp", "user", 0, "host", 0, 0));
  TEST(testrt("amqp://user@host/path:%", "amqp", "user", 0, "host", 0, "path:%"));
  TEST(testrt("amqp://user@host:5674/path:%", "amqp", "user", 0, "host", "5674", "path:%"));
  TEST(testrt("amqp://user@host/path:%", "amqp", "user", 0, "host", 0, "path:%"));
  TEST(testrt("amqp://bigbird@host/queue@host", "amqp", "bigbird", 0, "host", 0, "queue@host"));
  TEST(testrt("amqp://host/queue@host", "amqp", 0, 0, "host", 0, "queue@host"));
  TEST(testrt("amqp://host:9765/queue@host", "amqp", 0, 0, "host", "9765", "queue@host"));
  TEST(test("user:pass%2fword@host", 0, "user", "pass/word", "host", 0, 0));
  TEST(testrt("user:pass%2Fword@host", 0, "user", "pass/word", "host", 0, 0));
  // Can't round-trip passwords with lowercase hex encoding
  TEST(test("us%2fer:password@host", 0, "us/er", "password", "host", 0, 0));
  TEST(testrt("us%2Fer:password@host", 0, "us/er", "password", "host", 0, 0));
  // Can't round-trip passwords with lowercase hex encoding
  TEST(test("user:pass%2fword%@host", 0, "user", "pass/word%", "host", 0, 0));
  TEST(testrt("localhost/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", 0, 0, 0, "localhost", 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  TEST(testrt("/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", 0, 0, 0, 0, 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  TEST(testrt("amqp://localhost/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", "amqp", 0, 0, "localhost", 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  // PROTON-995
  TEST(testrt("amqps://%40user%2F%3A:%40pass%2F%3A@example.net/some_topic",
              "amqps", "@user/:", "@pass/:", "example.net", 0, "some_topic"));
  TEST(testrt("amqps://user%2F%3A=:pass%2F%3A=@example.net/some_topic",
              "amqps", "user/:=", "pass/:=", "example.net", 0, "some_topic"));
  // Really perverse url
  TEST(testrt("://:@://:", "", "", "", 0, "", "/:"));
  return failed;
}

#undef PN_USE_DEPRECATED_API
