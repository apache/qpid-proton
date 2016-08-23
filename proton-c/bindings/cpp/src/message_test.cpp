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

namespace {

using namespace std;
using namespace proton;

#define CHECK_STR(ATTR) \
    m.ATTR(#ATTR); \
    ASSERT_EQUAL(std::string(#ATTR), m.ATTR())

#define CHECK_MESSAGE_ID(ATTR) \
    m.ATTR(#ATTR); \
    ASSERT_EQUAL(scalar(#ATTR), m.ATTR())

void test_message_defaults() {
    message m;
    ASSERT(m.body().empty());
    ASSERT(m.id().empty());
    ASSERT(m.user().empty());
    ASSERT(m.to().empty());
    ASSERT(m.subject().empty());
    ASSERT(m.reply_to().empty());
    ASSERT(m.correlation_id().empty());
    ASSERT(m.content_type().empty());
    ASSERT(m.content_encoding().empty());
    ASSERT(m.group_id().empty());
    ASSERT(m.reply_to_group_id().empty());
    ASSERT_EQUAL(0, m.expiry_time().milliseconds());
    ASSERT_EQUAL(0, m.creation_time().milliseconds());

    ASSERT_EQUAL(false, m.inferred());
    ASSERT_EQUAL(false, m.durable());
    ASSERT_EQUAL(0, m.ttl().milliseconds());
    ASSERT_EQUAL(message::default_priority, m.priority());
    ASSERT_EQUAL(false, m.first_acquirer());
    ASSERT_EQUAL(0u, m.delivery_count());
}

void test_message_properties() {
    message m("hello");
    std::string s = get<std::string>(m.body());
    ASSERT_EQUAL("hello", s);

    CHECK_MESSAGE_ID(id);
    CHECK_STR(user);
    CHECK_STR(to);
    CHECK_STR(subject);
    CHECK_STR(reply_to);
    CHECK_MESSAGE_ID(correlation_id);
    CHECK_STR(content_type);
    CHECK_STR(content_encoding);
    CHECK_STR(group_id);
    CHECK_STR(reply_to_group_id);
    m.expiry_time(timestamp(42));
    ASSERT_EQUAL(m.expiry_time().milliseconds(), 42);
    m.creation_time(timestamp(4242));
    ASSERT_EQUAL(m.creation_time().milliseconds(), 4242);
    m.ttl(duration(30));
    ASSERT_EQUAL(m.ttl().milliseconds(), 30);
    m.priority(3);
    ASSERT_EQUAL(m.priority(), 3);

    message m2(m);
    ASSERT_EQUAL("hello", get<std::string>(m2.body()));
    ASSERT_EQUAL(message_id("id"), m2.id());
    ASSERT_EQUAL("user", m2.user());
    ASSERT_EQUAL("to", m2.to());
    ASSERT_EQUAL("subject", m2.subject());
    ASSERT_EQUAL("reply_to", m2.reply_to());
    ASSERT_EQUAL(message_id("correlation_id"), m2.correlation_id());
    ASSERT_EQUAL("content_type", m2.content_type());
    ASSERT_EQUAL("content_encoding", m2.content_encoding());
    ASSERT_EQUAL("group_id", m2.group_id());
    ASSERT_EQUAL("reply_to_group_id", m2.reply_to_group_id());
    ASSERT_EQUAL(42, m2.expiry_time().milliseconds());
    ASSERT_EQUAL(4242, m.creation_time().milliseconds());

    m2 = m;
    ASSERT_EQUAL("hello", get<std::string>(m2.body()));
    ASSERT_EQUAL(message_id("id"), m2.id());
    ASSERT_EQUAL("user", m2.user());
    ASSERT_EQUAL("to", m2.to());
    ASSERT_EQUAL("subject", m2.subject());
    ASSERT_EQUAL("reply_to", m2.reply_to());
    ASSERT_EQUAL(message_id("correlation_id"), m2.correlation_id());
    ASSERT_EQUAL("content_type", m2.content_type());
    ASSERT_EQUAL("content_encoding", m2.content_encoding());
    ASSERT_EQUAL("group_id", m2.group_id());
    ASSERT_EQUAL("reply_to_group_id", m2.reply_to_group_id());
    ASSERT_EQUAL(42, m2.expiry_time().milliseconds());
    ASSERT_EQUAL(4242, m.creation_time().milliseconds());
}

void test_message_body() {
    std::string s("hello");
    message m1(s.c_str());
    ASSERT_EQUAL(s, get<std::string>(m1.body()));
    message m2(s);
    ASSERT_EQUAL(s, coerce<std::string>(m2.body()));
    message m3;
    m3.body(s);
    ASSERT_EQUAL(s, coerce<std::string>(m3.body()));
    ASSERT_EQUAL(5, coerce<int64_t>(message(5).body()));
    ASSERT_EQUAL(3.1, coerce<double>(message(3.1).body()));
}

void test_message_maps() {
    message m;

    ASSERT(m.properties().empty());
    ASSERT(m.message_annotations().empty());
    ASSERT(m.delivery_annotations().empty());

    m.properties().put("foo", 12);
    m.delivery_annotations().put("bar", "xyz");

    m.message_annotations().put(23, "23");
    ASSERT_EQUAL(m.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m.message_annotations().get(23), scalar("23"));

    message m2(m);

    ASSERT_EQUAL(m2.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m2.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m2.message_annotations().get(23), scalar("23"));

    m.properties().put("foo","newfoo");
    m.delivery_annotations().put(24, 1000);
    m.message_annotations().erase(23);

    m2 = m;
    ASSERT_EQUAL(1u, m2.properties().size());
    ASSERT_EQUAL(m2.properties().get("foo"), scalar("newfoo"));
    ASSERT_EQUAL(2u, m2.delivery_annotations().size());
    ASSERT_EQUAL(m2.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m2.delivery_annotations().get(24), scalar(1000));
    ASSERT(m2.message_annotations().empty());
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_message_properties());
    RUN_TEST(failed, test_message_defaults());
    RUN_TEST(failed, test_message_body());
    RUN_TEST(failed, test_message_maps());
    return failed;
}
