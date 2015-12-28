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

#include "proton/message.hpp"
#include "test_bits.hpp"
#include <string>
#include <fstream>
#include <streambuf>
#include <iosfwd>

using namespace std;
using namespace proton;


#define CHECK_STR(ATTR) \
    m.ATTR(#ATTR); \
    ASSERT_EQUAL(std::string(#ATTR), m.ATTR())

#define CHECK_STR_VALUE(ATTR) \
    m.ATTR(#ATTR); \
    ASSERT_EQUAL(std::string(#ATTR), m.ATTR().get<std::string>())

void test_message() {
    message m("hello");
    std::string s = m.body().get<std::string>();
    ASSERT_EQUAL("hello", s);

    CHECK_STR_VALUE(id);
    CHECK_STR(user_id);
    CHECK_STR(address);
    CHECK_STR(subject);
    CHECK_STR(reply_to);
    CHECK_STR_VALUE(correlation_id);
    CHECK_STR(content_type);
    CHECK_STR(content_encoding);
    CHECK_STR(group_id);
    CHECK_STR(reply_to_group_id);

    m.expiry_time(42);
    ASSERT_EQUAL(m.expiry_time().milliseconds, 42);
    m.creation_time(4242);
    ASSERT_EQUAL(m.creation_time().milliseconds, 4242);

    m.property("foo", 12);
    ASSERT_EQUAL(m.property("foo"), value(12));
    m.property("xxx", false);
    ASSERT_EQUAL(m.property("xxx"), value(false));
    ASSERT(m.erase_property("xxx"));
    ASSERT(m.property("xxx").empty());
    ASSERT(!m.erase_property("yyy"));

    std::map<std::string, proton::value> props;
    m.properties().get(props);
    ASSERT_EQUAL(props.size(), 1);
    ASSERT_EQUAL(props["foo"], value(12));
    props["bar"] = amqp_symbol("xyz");
    props["foo"] = true;
    m.properties(props);
    ASSERT_EQUAL(m.property("foo"), value(true));
    ASSERT_EQUAL(m.property("bar"), value(amqp_symbol("xyz")));
    m.property("bar", amqp_binary("bar"));
    std::map<std::string, proton::value> props2;
    m.properties().get(props2);
    ASSERT_EQUAL(2, props2.size());
    ASSERT_EQUAL(props2["foo"], value(true));
    ASSERT_EQUAL(props2["bar"], value(amqp_binary("bar")));
}

int main(int argc, char** argv) {
    int failed = 0;
    RUN_TEST(failed, test_message());
    return failed;
}
