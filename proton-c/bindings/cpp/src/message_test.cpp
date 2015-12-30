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
#include "proton/scalar.hpp"
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

#define CHECK_MESSAGE_ID(ATTR) \
    m.ATTR(#ATTR); \
    ASSERT_EQUAL(scalar(#ATTR), m.ATTR())

void test_message_properties() {
    message m("hello");
    std::string s = m.body().get<std::string>();
    ASSERT_EQUAL("hello", s);

    CHECK_MESSAGE_ID(id);
    CHECK_STR(user_id);
    CHECK_STR(address);
    CHECK_STR(subject);
    CHECK_STR(reply_to);
    CHECK_MESSAGE_ID(correlation_id);
    CHECK_STR(content_type);
    CHECK_STR(content_encoding);
    CHECK_STR(group_id);
    CHECK_STR(reply_to_group_id);
    m.expiry_time(42);
    ASSERT_EQUAL(m.expiry_time().milliseconds, 42);
    m.creation_time(4242);
    ASSERT_EQUAL(m.creation_time().milliseconds, 4242);

    message m2(m);
    ASSERT_EQUAL("hello", m2.body().get<std::string>());
    ASSERT_EQUAL(message_id("id"), m2.id());
    ASSERT_EQUAL("user_id", m2.user_id());
    ASSERT_EQUAL("address", m2.address());
    ASSERT_EQUAL("subject", m2.subject());
    ASSERT_EQUAL("reply_to", m2.reply_to());
    ASSERT_EQUAL(message_id("correlation_id"), m2.correlation_id());
    ASSERT_EQUAL("content_type", m2.content_type());
    ASSERT_EQUAL("content_encoding", m2.content_encoding());
    ASSERT_EQUAL("group_id", m2.group_id());
    ASSERT_EQUAL("reply_to_group_id", m2.reply_to_group_id());
    ASSERT_EQUAL(42, m2.expiry_time().milliseconds);
    ASSERT_EQUAL(4242, m.creation_time().milliseconds);

    m2 = m;
    ASSERT_EQUAL("hello", m2.body().get<std::string>());
    ASSERT_EQUAL(message_id("id"), m2.id());
    ASSERT_EQUAL("user_id", m2.user_id());
    ASSERT_EQUAL("address", m2.address());
    ASSERT_EQUAL("subject", m2.subject());
    ASSERT_EQUAL("reply_to", m2.reply_to());
    ASSERT_EQUAL(message_id("correlation_id"), m2.correlation_id());
    ASSERT_EQUAL("content_type", m2.content_type());
    ASSERT_EQUAL("content_encoding", m2.content_encoding());
    ASSERT_EQUAL("group_id", m2.group_id());
    ASSERT_EQUAL("reply_to_group_id", m2.reply_to_group_id());
    ASSERT_EQUAL(42, m2.expiry_time().milliseconds);
    ASSERT_EQUAL(4242, m.creation_time().milliseconds);
}

void test_message_body() {
    std::string s("hello");
    message m1(s.c_str());
    ASSERT_EQUAL(s, m1.body().get<std::string>());
    message m2(s);
    ASSERT_EQUAL(s, m2.body().as_string());
    message m3;
    m3.body(s);
    ASSERT_EQUAL(s, m3.body().as_string());
    ASSERT_EQUAL(5, message(5).body().as_int());
    ASSERT_EQUAL(3.1, message(3.1).body().as_double());
}

void test_message_maps() {
    message m;

    ASSERT(m.properties().empty());
    ASSERT(m.annotations().empty());
    ASSERT(m.instructions().empty());

    m.properties()["foo"] = 12;
    m.instructions()["bar"] = "xyz";
    m.annotations()[23] = "23";

    ASSERT_EQUAL(m.properties()["foo"], scalar(12));
    ASSERT_EQUAL(m.instructions()["bar"], scalar("xyz"));
    ASSERT_EQUAL(m.annotations()[23], scalar("23"));

    message m2(m);
    message::annotation_map& amap = m2.instructions();

    ASSERT_EQUAL(m2.properties()["foo"], scalar(12));
    ASSERT_EQUAL(m2.instructions()["bar"], scalar("xyz"));
    ASSERT_EQUAL(m2.annotations()[23], scalar("23"));

    m.properties()["foo"] = "newfoo";
    m.instructions()[24] = 1000;
    m.annotations().erase(23);

    m2 = m;
    ASSERT_EQUAL(1, m2.properties().size());
    ASSERT_EQUAL(m2.properties()["foo"], scalar("newfoo"));
    ASSERT_EQUAL(2, m2.instructions().size());
    ASSERT_EQUAL(m2.instructions()["bar"], scalar("xyz"));
    ASSERT_EQUAL(m2.instructions()[24], scalar(1000));
    ASSERT(m2.annotations().empty());
}

int main(int argc, char** argv) {
    int failed = 0;
    RUN_TEST(failed, test_message_properties());
    RUN_TEST(failed, test_message_body());
    RUN_TEST(failed, test_message_maps());
    return failed;
}
