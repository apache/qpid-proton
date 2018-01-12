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

#include "test_bits.hpp"
#include <proton/url.hpp>

namespace {

#define CHECK_URL(S, SCHEME, USER, PWD, HOST, PORT, PATH) do {   \
        proton::url u(S);                                        \
        ASSERT_EQUAL(SCHEME, u.scheme());                        \
        ASSERT_EQUAL(USER, u.user());                            \
        ASSERT_EQUAL(PWD, u.password());                         \
        ASSERT_EQUAL(HOST, u.host());                            \
        ASSERT_EQUAL(PORT, u.port());                            \
        ASSERT_EQUAL(PATH, u.path());                            \
    } while(0)

void parse_to_string_test() {
    CHECK_URL("amqp://foo:xyz/path",
              "amqp", "", "", "foo", "xyz", "path");
    CHECK_URL("amqp://username:password@host:1234/path",
              "amqp", "username", "password", "host", "1234", "path");
    CHECK_URL("host:1234",
              "amqp", "", "", "host", "1234", "");
    CHECK_URL("host",
              "amqp", "", "", "host", "amqp", "");
    CHECK_URL("host/path",
              "amqp", "", "", "host", "amqp", "path");
    CHECK_URL("amqps://host",
              "amqps", "", "", "host", "amqps", "");
    CHECK_URL("/path",
              "amqp", "", "", "localhost", "amqp", "path");
    CHECK_URL("",
              "amqp", "", "", "localhost", "amqp", "");
    CHECK_URL(":1234",
              "amqp", "", "", "localhost", "1234", "");
}

void parse_slash_slash() {
    CHECK_URL("//username:password@host:1234/path",
              "amqp", "username", "password", "host", "1234", "path");
    CHECK_URL("//host:port/path",
              "amqp", "", "", "host", "port", "path");
    CHECK_URL("//host",
              "amqp", "", "", "host", "amqp", "");
    CHECK_URL("//:port",
              "amqp", "", "", "localhost", "port", "");
    CHECK_URL("//:0",
              "amqp", "", "", "localhost", "0", "");
}

#define CHECK_URL_NODEFAULT(S, SCHEME, USER, PWD, HOST, PORT, PATH) do {   \
        proton::url u(S, false);                                        \
        ASSERT_EQUAL(SCHEME, u.scheme());                               \
        ASSERT_EQUAL(USER, u.user());                                   \
        ASSERT_EQUAL(PWD, u.password());                                \
        ASSERT_EQUAL(HOST, u.host());                                   \
        ASSERT_EQUAL(PORT, u.port());                                   \
        ASSERT_EQUAL(PATH, u.path());                                   \
    } while(0)

}

void parse_nodefault() {
    CHECK_URL_NODEFAULT("",
                        "", "", "", "", "", "");
    CHECK_URL_NODEFAULT("//:",
                        "", "", "", "", "", "");
    CHECK_URL_NODEFAULT("//:0",
                        "", "", "", "", "0", "");
    CHECK_URL_NODEFAULT("//h:",
                        "", "", "", "h", "", "");
}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, parse_to_string_test());
    RUN_TEST(failed, parse_slash_slash());
    RUN_TEST(failed, parse_nodefault());
    return failed;
}
