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
    m.message_annotations().put(23, int8_t(42));

    ASSERT_EQUAL(m.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m.delivery_annotations().get("bar"), scalar("xyz"));

    ASSERT_EQUAL(m.message_annotations().get(23), scalar(int8_t(42)));
    ASSERT_EQUAL(proton::get<int8_t>(m.message_annotations().get(23)), 42);
    ASSERT_EQUAL(m.message_annotations().get(23).get<int8_t>(), 42);

    message m2(m);

    ASSERT_EQUAL(m.properties().get("foo"), scalar(12)); // Decoding shouldn't change it

    ASSERT_EQUAL(m2.properties().get("foo"), scalar(12));
    ASSERT_EQUAL(m2.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m2.message_annotations().get(23), scalar(int8_t(42)));

    m.properties().put("foo","newfoo");
    m.delivery_annotations().put(24, 1000);
    m.message_annotations().erase(23);

    message m3 = m;
    size_t size = m3.properties().size();
    ASSERT_EQUAL(1u, size);
    ASSERT_EQUAL(m3.properties().get("foo"), scalar("newfoo"));
    ASSERT_EQUAL(2u, m3.delivery_annotations().size());
    ASSERT_EQUAL(m3.delivery_annotations().get("bar"), scalar("xyz"));
    ASSERT_EQUAL(m3.delivery_annotations().get(24), scalar(1000));
    ASSERT(m3.message_annotations().empty());

    // PROTON-1498
    message msg;
    msg.message_annotations().put("x-opt-jms-msg-type", int8_t(1));

    proton::message::annotation_map& am_ref = msg.message_annotations();
    uint8_t t = am_ref.get(proton::symbol("x-opt-jms-msg-type")).get<int8_t>();
    ASSERT_EQUAL(1, t);

    proton::message::annotation_map am_val = msg.message_annotations();
    t = am_val.get(proton::symbol("x-opt-jms-msg-type")).get<int8_t>();
    ASSERT_EQUAL(1, t);
}

void test_message_reuse() {
    message m1("one");
    m1.properties().put("x", "y");

    message m2("two");
    m2.properties().put("a", "b");

    m1.decode(m2.encode());     // Use m1 for a newly decoded message
    ASSERT_EQUAL(value("two"), m1.body());
    ASSERT_EQUAL(value("b"), m1.properties().get("a"));
}

void test_message_print() {
  message m("hello");
  m.to("to");
  m.user("user");
  m.subject("subject");
  m.content_type("weird");
  m.correlation_id(23);
  m.properties().put("foo", "bar");
  m.properties().put("num", 9);
  m.delivery_annotations().put("deliverme", "please");
  ASSERT_EQUAL("Message{address=\"to\", user_id=\"user\", subject=\"subject\", correlation_id=23, content_type=\"weird\", body=\"hello\"}", to_string(m));
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_message_properties());
    RUN_TEST(failed, test_message_defaults());
    RUN_TEST(failed, test_message_body());
    RUN_TEST(failed, test_message_maps());
    RUN_TEST(failed, test_message_reuse());
    RUN_TEST(failed, test_message_print());
    return failed;
}
