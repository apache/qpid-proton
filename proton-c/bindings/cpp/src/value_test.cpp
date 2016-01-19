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

#include <proton/value.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

using namespace std;
using namespace proton;
using namespace test;

// Inserting and extracting simple C++ values.
template <class T> void value_test(T x, type_id tid, const std::string& s, T y) {
    value v(x);
    ASSERT_EQUAL(tid, v.type());
    ASSERT_EQUAL(x, v.get<T>());

    value v2;
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, v2.get<T>());

    value v3(v);
    v3 = x;
    ASSERT_EQUAL(tid, v3.type());
    ASSERT_EQUAL(x, v3.get<T>());

    ASSERT_EQUAL(v, v2);
    ASSERT_EQUAL(s, str(v));
    ASSERT(x != y);
    ASSERT(x < y);
    ASSERT(y > x);
}

// Map values
void map_test() {
    std::map<string, int> m;
    m["a"] = 1;
    m["b"] = 2;
    m["c"] = 3;
    value v = m;
    ASSERT_EQUAL("{\"a\"=1, \"b\"=2, \"c\"=3}",  str(v));
    std::map<value, value> mv;
    v.get(mv);
    ASSERT_EQUAL(mv["a"], value(amqp_int(1)));
    mv["b"] = amqp_binary("xyz");
    mv.erase("c");
    v = value(mv);
    ASSERT_EQUAL("{\"a\"=1, \"b\"=b\"xyz\"}",  str(v));

    std::vector<std::pair<string, value> > vec;
    v.get_pairs(vec);
    ASSERT_EQUAL(2, vec.size());
    ASSERT_EQUAL(std::make_pair(std::string("a"), value(1)), vec[0]);
    ASSERT_EQUAL(std::make_pair(std::string("b"), value(amqp_binary("xyz"))), vec[1]);
}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, value_test(false, BOOLEAN, "false", true));
    RUN_TEST(failed, value_test(amqp_ubyte(42), UBYTE, "42", amqp_ubyte(50)));
    RUN_TEST(failed, value_test(amqp_byte(-42), BYTE, "-42", amqp_byte(-40)));
    RUN_TEST(failed, value_test(amqp_ushort(4242), USHORT, "4242", amqp_ushort(5252)));
    RUN_TEST(failed, value_test(amqp_short(-4242), SHORT, "-4242", amqp_short(3)));
    RUN_TEST(failed, value_test(amqp_uint(4242), UINT, "4242", amqp_uint(5252)));
    RUN_TEST(failed, value_test(amqp_int(-4242), INT, "-4242", amqp_int(3)));
    RUN_TEST(failed, value_test(amqp_ulong(4242), ULONG, "4242", amqp_ulong(5252)));
    RUN_TEST(failed, value_test(amqp_long(-4242), LONG, "-4242", amqp_long(3)));
    RUN_TEST(failed, value_test(amqp_float(1.234), FLOAT, "1.234", amqp_float(2.345)));
    RUN_TEST(failed, value_test(amqp_double(11.2233), DOUBLE, "11.2233", amqp_double(12)));
    RUN_TEST(failed, value_test(amqp_string("aaa"), STRING, "aaa", amqp_string("aaaa")));
    RUN_TEST(failed, value_test(std::string("xxx"), STRING, "xxx", std::string("yyy")));
    RUN_TEST(failed, value_test(amqp_symbol("aaa"), SYMBOL, "aaa", amqp_symbol("aaaa")));
    RUN_TEST(failed, value_test(amqp_binary("aaa"), BINARY, "b\"aaa\"", amqp_binary("aaaa")));
    RUN_TEST(failed, map_test());
    return failed;
}
