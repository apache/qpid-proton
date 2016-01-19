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
#include "proton/type_traits.hpp"

#include <proton/scalar.hpp>
#include <proton/value.hpp>
#include <proton/message_id.hpp>
#include <proton/annotation_key.hpp>

#include <sstream>

using namespace std;
using namespace proton;
using namespace test;

// Inserting and extracting simple C++ values.
template <class T> void type_test(T x, type_id tid, T y) {
    scalar s(x);
    ASSERT_EQUAL(tid, s.type());
    ASSERT(!s.empty());
    ASSERT_EQUAL(x, s.get<T>());

    scalar v2;
    ASSERT(v2.type() == NULL_TYPE);
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, v2.get<T>());
    ASSERT_EQUAL(s, v2);
    ASSERT_EQUAL(str(x), str(s));

    v2 = y;
    ASSERT(s != v2);
    ASSERT(s < v2);
    ASSERT(v2 > s);
}

#define ASSERT_MISMATCH(EXPR, WANT, GOT)                        \
    try {                                                       \
        (void)(EXPR);                                           \
        FAIL("expected type_error: " #EXPR);                    \
    } catch (const type_error& e) {                             \
        ASSERT_EQUAL(WANT, e.want);                             \
        ASSERT_EQUAL(GOT, e.got);                               \
    }

void convert_test() {
    scalar a;
    ASSERT_EQUAL(NULL_TYPE, a.type());
    ASSERT(a.empty());
    ASSERT_MISMATCH(a.get<float>(), FLOAT, NULL_TYPE);

    a = amqp_binary("foo");
    ASSERT_MISMATCH(a.get<int16_t>(), SHORT, BINARY);
    ASSERT_MISMATCH(a.as_int(), LONG, BINARY);
    ASSERT_MISMATCH(a.as_double(), DOUBLE, BINARY);
    ASSERT_MISMATCH(a.get<amqp_string>(), STRING, BINARY); // No strict conversion
    ASSERT_EQUAL(a.as_string(), std::string("foo")); // OK string-like conversion

    a = int16_t(42);
    ASSERT_MISMATCH(a.get<std::string>(), STRING, SHORT);
    ASSERT_MISMATCH(a.get<amqp_timestamp>(), TIMESTAMP, SHORT);
    ASSERT_MISMATCH(a.as_string(), STRING, SHORT);
    ASSERT_EQUAL(a.as_int(), 42);
    ASSERT_EQUAL(a.as_uint(), 42);
    ASSERT_EQUAL(a.as_double(), 42);

    a = int16_t(-42);
    ASSERT_EQUAL(a.as_int(), -42);
    ASSERT_EQUAL(a.as_uint(), uint64_t(-42));
    ASSERT_EQUAL(a.as_double(), -42);
}

void encode_decode_test() {
    value v;
    scalar a("foo");
    v = a;                      // Assignment to value does encode, get<> does decode.
    ASSERT_EQUAL(v, a);
    ASSERT_EQUAL(std::string("foo"), v.get<std::string>());
    scalar a2 = v.get<scalar>();
    ASSERT_EQUAL(std::string("foo"), a2.get<std::string>());
}

void message_id_test() {
    ASSERT_EQUAL(23, message_id(23).as_int());
    ASSERT_EQUAL(23, message_id(23).get<uint64_t>());
    ASSERT(message_id("foo") != message_id(amqp_binary("foo")));
    ASSERT_EQUAL(scalar("foo"), message_id("foo"));
    ASSERT_EQUAL("foo", message_id("foo").as_string());
    ASSERT(message_id("a") < message_id("z"));
    ASSERT_EQUAL(amqp_uuid(), message_id(amqp_uuid()).get<amqp_uuid>());
}

void annotation_key_test() {
    ASSERT_EQUAL(23, annotation_key(23).as_int());
    ASSERT_EQUAL(23, annotation_key(23).get<uint64_t>());
    ASSERT_EQUAL("foo", annotation_key("foo").as_string());
    ASSERT_EQUAL(scalar(amqp_symbol("foo")), annotation_key("foo"));
}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, type_test(false, BOOLEAN, true));
    RUN_TEST(failed, type_test(amqp_ubyte(42), UBYTE, amqp_ubyte(50)));
    RUN_TEST(failed, type_test(amqp_byte('x'), BYTE, amqp_byte('y')));
    RUN_TEST(failed, type_test(amqp_ushort(4242), USHORT, amqp_ushort(5252)));
    RUN_TEST(failed, type_test(amqp_short(-4242), SHORT, amqp_short(3)));
    RUN_TEST(failed, type_test(amqp_uint(4242), UINT, amqp_uint(5252)));
    RUN_TEST(failed, type_test(amqp_int(-4242), INT, amqp_int(3)));
    RUN_TEST(failed, type_test(amqp_ulong(4242), ULONG, amqp_ulong(5252)));
    RUN_TEST(failed, type_test(amqp_long(-4242), LONG, amqp_long(3)));
    RUN_TEST(failed, type_test(wchar_t(23), CHAR, wchar_t(24)));
    RUN_TEST(failed, type_test(amqp_float(1.234), FLOAT, amqp_float(2.345)));
    RUN_TEST(failed, type_test(amqp_double(11.2233), DOUBLE, amqp_double(12)));
    RUN_TEST(failed, type_test(amqp_timestamp(0), TIMESTAMP, amqp_timestamp(1)));
    RUN_TEST(failed, type_test(amqp_decimal32(0), DECIMAL32, amqp_decimal32(1)));
    RUN_TEST(failed, type_test(amqp_decimal64(0), DECIMAL64, amqp_decimal64(1)));
    pn_decimal128_t da = {0}, db = {1};
    RUN_TEST(failed, type_test(amqp_decimal128(da), DECIMAL128, amqp_decimal128(db)));
    pn_uuid_t ua = {0}, ub = {1};
    RUN_TEST(failed, type_test(amqp_uuid(ua), UUID, amqp_uuid(ub)));
    RUN_TEST(failed, type_test(amqp_string("aaa"), STRING, amqp_string("aaaa")));
    RUN_TEST(failed, type_test(amqp_symbol("aaa"), SYMBOL, amqp_symbol("aaaa")));
    RUN_TEST(failed, type_test(amqp_binary("aaa"), BINARY, amqp_binary("aaaa")));
    RUN_TEST(failed, type_test(std::string("xxx"), STRING, std::string("yyy")));
    RUN_TEST(failed, encode_decode_test());
    RUN_TEST(failed, message_id_test());
    RUN_TEST(failed, annotation_key_test());
    RUN_TEST(failed, convert_test());
    return failed;
}
