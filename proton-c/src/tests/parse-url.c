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
#include <proton/type_compat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// No point in running this code if assert doesn't work!
#undef NDEBUG
#include <assert.h>

#include <proton/error.h>
#include <proton/util.h>

static inline bool equalStrP(const char* s1, const char* s2)
{
  return (s1==s2) || (s1 && s2 && strcmp(s1,s2)==0);
}

static bool test_url_parse(const char* url0, const char* scheme0, const char* user0, const char* pass0, const char* host0, const char* port0, const char*path0)
{
  char* url = (char*)malloc(strlen(url0)+1);
  char* scheme = 0;
  char* user = 0;
  char* pass = 0;
  char* host = 0;
  char* port = 0;
  char* path = 0;

  strcpy(url, url0);

  pni_parse_url(url, &scheme, &user, &pass, &host, &port, &path);
  bool r =  equalStrP(scheme,scheme0) &&
            equalStrP(user, user0) &&
            equalStrP(pass, pass0) &&
            equalStrP(host, host0) &&
            equalStrP(port, port0) &&
            equalStrP(path, path0);

  free(url);

  return r;
}

int main(int argc, char **argv)
{
  assert(test_url_parse("", 0, 0, 0, "", 0, 0));
  assert(test_url_parse("/Foo.bar:90087@somewhere", 0, 0, 0, "", 0, "Foo.bar:90087@somewhere"));
  assert(test_url_parse("host", 0, 0, 0, "host", 0, 0));
  assert(test_url_parse("host:423", 0, 0, 0, "host", "423", 0));
  assert(test_url_parse("user@host", 0, "user", 0, "host", 0, 0));
  assert(test_url_parse("user:1243^&^:pw@host:423", 0, "user", "1243^&^:pw", "host", "423", 0));
  assert(test_url_parse("user:1243^&^:pw@host:423/Foo.bar:90087", 0, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087"));
  assert(test_url_parse("user:1243^&^:pw@host:423/Foo.bar:90087@somewhere", 0, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087@somewhere"));
  assert(test_url_parse("[::1]", 0, 0, 0, "::1", 0, 0));
  assert(test_url_parse("[::1]:amqp", 0, 0, 0, "::1", "amqp", 0));
  assert(test_url_parse("user@[::1]", 0, "user", 0, "::1", 0, 0));
  assert(test_url_parse("user@[::1]:amqp", 0, "user", 0, "::1", "amqp", 0));
  assert(test_url_parse("user:1243^&^:pw@[::1]:amqp", 0, "user", "1243^&^:pw", "::1", "amqp", 0));
  assert(test_url_parse("user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087"));
  assert(test_url_parse("user:1243^&^:pw@[::1:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "[", ":1:amqp", "Foo.bar:90087"));
  assert(test_url_parse("user:1243^&^:pw@::1]:amqp/Foo.bar:90087", 0, "user", "1243^&^:pw", "", ":1]:amqp", "Foo.bar:90087"));
  assert(test_url_parse("amqp://user@[::1]", "amqp", "user", 0, "::1", 0, 0));
  assert(test_url_parse("amqp://user@[::1]:amqp", "amqp", "user", 0, "::1", "amqp", 0));
  assert(test_url_parse("amqp://user@[1234:52:0:1260:f2de:f1ff:fe59:8f87]:amqp", "amqp", "user", 0, "1234:52:0:1260:f2de:f1ff:fe59:8f87", "amqp", 0));
  assert(test_url_parse("amqp://user:1243^&^:pw@[::1]:amqp", "amqp", "user", "1243^&^:pw", "::1", "amqp", 0));
  assert(test_url_parse("amqp://user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", "amqp", "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087"));
  assert(test_url_parse("amqp://host", "amqp", 0, 0, "host", 0, 0));
  assert(test_url_parse("amqp://user@host", "amqp", "user", 0, "host", 0, 0));
  assert(test_url_parse("amqp://user@host/path:%", "amqp", "user", 0, "host", 0, "path:%"));
  assert(test_url_parse("amqp://user@host:5674/path:%", "amqp", "user", 0, "host", "5674", "path:%"));
  assert(test_url_parse("amqp://user@host/path:%", "amqp", "user", 0, "host", 0, "path:%"));
  assert(test_url_parse("amqp://bigbird@host/queue@host", "amqp", "bigbird", 0, "host", 0, "queue@host"));
  assert(test_url_parse("amqp://host/queue@host", "amqp", 0, 0, "host", 0, "queue@host"));
  assert(test_url_parse("amqp://host:9765/queue@host", "amqp", 0, 0, "host", "9765", "queue@host"));
  assert(test_url_parse("user:pass%2fword@host", 0, "user", "pass/word", "host", 0, 0));
  assert(test_url_parse("user:pass%2Fword@host", 0, "user", "pass/word", "host", 0, 0));
  assert(test_url_parse("us%2fer:password@host", 0, "us/er", "password", "host", 0, 0));
  assert(test_url_parse("us%2Fer:password@host", 0, "us/er", "password", "host", 0, 0));
  assert(test_url_parse("user:pass%2fword%@host", 0, "user", "pass/word%", "host", 0, 0));
  assert(test_url_parse("localhost/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", 0, 0, 0, "localhost", 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  assert(test_url_parse("/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", 0, 0, 0, "", 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  assert(test_url_parse("amqp://localhost/temp-queue://ID:ganymede-36663-1408448359876-2:123:0", "amqp", 0, 0, "localhost", 0, "temp-queue://ID:ganymede-36663-1408448359876-2:123:0"));
  // Really perverse url
  assert(test_url_parse("://:@://:", "", "", "", "", "", "/:"));
  return 0;
}
