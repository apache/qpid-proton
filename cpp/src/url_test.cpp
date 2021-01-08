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

#include <catch.hpp>
#include <proton/url.hpp>

namespace {
using proton::url;

#define CHECK_URL(U, SCHEME, USER, PWD, HOST, PORT, PATH, S) do { \
        url u(U);                                             \
        CHECK((SCHEME) == u.scheme());                        \
        CHECK((USER) == u.user());                            \
        CHECK((PWD) == u.password());                         \
        CHECK((HOST) == u.host());                            \
        CHECK((PORT) == u.port());                            \
        CHECK((PATH) == u.path());                            \
        CHECK(std::string(S) == std::string(u));              \
    } while(0)

TEST_CASE("parse URL","[url]") {
    SECTION("full and defaulted") {
        CHECK_URL(url("amqp://foo:xyz/path"),
                  "amqp", "", "", "foo", "xyz", "path",
                  "amqp://foo:xyz/path");
        CHECK_URL(url("amqp://username:password@host:1234/path"),
                  "amqp", "username", "password", "host", "1234", "path",
                  "amqp://username:password@host:1234/path");
        CHECK_URL(url("host:1234"),
                  "amqp", "", "", "host", "1234", "",
                  "amqp://host:1234");
        CHECK_URL(url("host"),
                  "amqp", "", "", "host", "amqp", "",
                  "amqp://host:amqp");
        CHECK_URL(url("host/path"),
                  "amqp", "", "", "host", "amqp", "path",
                  "amqp://host:amqp/path");
        CHECK_URL(url("amqps://host"),
                  "amqps", "", "", "host", "amqps", "",
                  "amqps://host:amqps");
        CHECK_URL(url("/path"),
                  "amqp", "", "", "localhost", "amqp", "path",
                  "amqp://localhost:amqp/path");
        CHECK_URL(url(""),
                  "amqp", "", "", "localhost", "amqp", "",
                  "amqp://localhost:amqp");
        CHECK_URL(url(":1234"),
                  "amqp", "", "", "localhost", "1234", "",
                  "amqp://localhost:1234");
    }
    SECTION("starting with //") {
        CHECK_URL(url("//username:password@host:1234/path"),
                  "amqp", "username", "password", "host", "1234", "path",
                  "amqp://username:password@host:1234/path");
        CHECK_URL(url("//host:port/path"),
                  "amqp", "", "", "host", "port", "path",
                  "amqp://host:port/path");
        CHECK_URL(url("//host"),
                  "amqp", "", "", "host", "amqp", "",
                  "amqp://host:amqp");
        CHECK_URL(url("//:port"),
                  "amqp", "", "", "localhost", "port", "",
                  "amqp://localhost:port");
        CHECK_URL(url("//:0"),
                  "amqp", "", "", "localhost", "0", "",
                  "amqp://localhost:0");
    }
    SECTION("no defaults") {
        CHECK_URL(url("", false),
                  "", "", "", "", "", "",
                  "");
        CHECK_URL(url("//:", false),
                  "", "", "", "", "", "",
                  ":");
        CHECK_URL(url("//:0", false),
                  "", "", "", "", "0", "",
                  ":0");
        CHECK_URL(url("//h:", false),
                  "", "", "", "h", "", "",
                  "h:");
    }
    SECTION("urlencoding") {
        CHECK_URL(url("amqps://%40user%2F%3A:%40pass%2F%3A@example.net/some_topic"),
                  "amqps", "@user/:", "@pass/:", "example.net", "amqps", "some_topic",
                  "amqps://%40user%2F%3A:%40pass%2F%3A@example.net:amqps/some_topic");
        CHECK_URL(url("amqps://user%2F%3A=:pass%2F%3A=@example.net/some_topic"),
                  "amqps", "user/:=", "pass/:=", "example.net", "amqps", "some_topic",
                  "amqps://user%2F%3A=:pass%2F%3A=@example.net:amqps/some_topic");
    }
    SECTION("ipv6") {
        CHECK_URL(url("[fe80::c662:ab36:1ef1:1596]:5672/path"),
                  "amqp", "", "", "fe80::c662:ab36:1ef1:1596", "5672", "path",
                  "amqp://[fe80::c662:ab36:1ef1:1596]:5672/path");
        CHECK_URL(url("amqp://user:pass@[::1]:1234/path"),
                  "amqp", "user", "pass", "::1", "1234", "path",
                  "amqp://user:pass@[::1]:1234/path");
    }
}

} // namespace
