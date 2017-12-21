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

void check_url(const std::string& s,
               const std::string& scheme,
               const std::string& user,
               const std::string& pwd,
               const std::string& host,
               const std::string& port,
               const std::string& path
              )
{
    proton::url u(s);
    ASSERT_EQUAL(scheme, u.scheme());
    ASSERT_EQUAL(user, u.user());
    ASSERT_EQUAL(pwd, u.password());
    ASSERT_EQUAL(host, u.host());
    ASSERT_EQUAL(port, u.port());
    ASSERT_EQUAL(path, u.path());
}

void parse_to_string_test() {
    check_url("amqp://foo:xyz/path",
              "amqp", "", "", "foo", "xyz", "path");
    check_url("amqp://username:password@host:1234/path",
              "amqp", "username", "password", "host", "1234", "path");
    check_url("host:1234",
              "amqp", "", "", "host", "1234", "");
    check_url("host",
              "amqp", "", "", "host", "amqp", "");
    check_url("host/path",
              "amqp", "", "", "host", "amqp", "path");
    check_url("amqps://host",
              "amqps", "", "", "host", "amqps", "");
    check_url("/path",
              "amqp", "", "", "localhost", "amqp", "path");
    check_url("",
              "amqp", "", "", "localhost", "amqp", "");
    check_url(":1234",
              "amqp", "", "", "localhost", "1234", "");
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, parse_to_string_test());
    return failed;
}
