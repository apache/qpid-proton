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

#include <proton/atom.hpp>

using namespace std;
using namespace proton;

// Inserting and extracting simple C++ values.
template <class T> void type_test(T x, type_id tid, T y) {
    atom v(x);
    ASSERT_EQUAL(tid, v.type());
    ASSERT(!v.empty());
    ASSERT_EQUAL(x, v.get<T>());

    atom v2;
    ASSERT(v2.type() == NULL_TYPE);
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, v2.get<T>());
    ASSERT_EQUAL(v, v2);
    ASSERT_EQUAL(str(x), str(v));

    v2 = y;
    ASSERT(v != v2);
    ASSERT(v < v2);
    ASSERT(v2 > v);
}

#define ASSERT_MISMATCH(EXPR) \
    try { (void)(EXPR); FAIL("expected type_mismatch: " #EXPR); } catch (type_mismatch) {}

void convert_test() {
    atom a;
    ASSERT_EQUAL(NULL_TYPE, a.type());
    ASSERT(a.empty());
    ASSERT_MISMATCH(a.get<float>());

    a = amqp_binary("foo");
    ASSERT_MISMATCH(a.get<int16_t>());
    ASSERT_MISMATCH(a.as_int());
    ASSERT_MISMATCH(a.as_double());
    ASSERT_MISMATCH(a.get<amqp_string>());           // No strict conversion
    ASSERT_EQUAL(a.as_string(), std::string("foo")); // OK string-like conversion

    a = int16_t(42);
    ASSERT_MISMATCH(a.get<std::string>());
    ASSERT_MISMATCH(a.get<amqp_timestamp>());
    ASSERT_MISMATCH(a.as_string());
    ASSERT_EQUAL(a.as_int(), 42);
    ASSERT_EQUAL(a.as_uint(), 42);
    ASSERT_EQUAL(a.as_double(), 42);

    a = int16_t(-42);
    ASSERT_EQUAL(a.as_int(), -42);
    ASSERT_EQUAL(a.as_uint(), uint64_t(-42));
    ASSERT_EQUAL(a.as_double(), -42);
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
    return failed;
}
