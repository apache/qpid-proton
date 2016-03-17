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

#include <proton/types.hpp>
#include <proton/error.hpp>


using namespace std;
using namespace proton;
using namespace test;

// Inserting and extracting simple C++ values.
template <class T> void simple_type_test(T x, type_id tid, const std::string& s, T y) {
    value vx(x);
    ASSERT_EQUAL(tid, vx.type());
    ASSERT_EQUAL(x, get<T>(vx));

    value vxa = x;
    ASSERT_EQUAL(tid, vxa.type());
    ASSERT_EQUAL(x, get<T>(vxa));

    value v2;
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, get<T>(v2));

    value v3(vx);
    ASSERT_EQUAL(tid, v3.type());
    ASSERT_EQUAL(x, get<T>(v3));

    value v4 = vx;
    ASSERT_EQUAL(tid, v4.type());
    ASSERT_EQUAL(x, get<T>(v4));


    ASSERT_EQUAL(vx, v2);
    ASSERT_EQUAL(s, str(vx));
    value vy(y);
    ASSERT(vx != vy);
    ASSERT(vx < vy);
    ASSERT(vy > vx);
}

template <class T> void simple_integral_test() {
    typedef typename internal::integer_type<sizeof(T), internal::is_signed<T>::value>::type int_type;
    simple_type_test(T(3), internal::type_id_of<int_type>::value, "3", T(4));
}

// Inserting and extracting arrays from a container T of type U
template <class T> void sequence_test(type_id tid, const many<typename T::value_type>& values) {
    T x(values.begin(), values.end());

    value vx(x);                // construt
    ASSERT_EQUAL(tid, vx.type());
    ASSERT_EQUAL(x, get<T>(vx));

    value v2;                   // assign
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, get<T>(v2));
    ASSERT_EQUAL(vx, v2);

    T y(x);
    *y.begin() = *(++y.begin()); // Second element is bigger so y is lexicographically bigger than x
    value vy(y);
    ASSERT(vx != vy);
    ASSERT(vx < vy);
    ASSERT(vy > vx);
}

template <class T, class U> void map_test(const U& values) {
    T m(values.begin(), values.end());
    value v(m);
    ASSERT_EQUAL(MAP, v.type());
    T m2(get<T>(v));
    ASSERT_EQUAL(m.size(), m2.size());
    ASSERT_EQUAL(m, m2);
}

template <class T> T make(const char c) {
    T x; std::fill(x.begin(), x.end(), c); return x;
}

void null_test() {
    value v;
    ASSERT(v.empty());
    ASSERT_EQUAL(NULL_TYPE, v.type());
    get<null>(v);
    null n;
    get(v, n);
    value v2(n);
    ASSERT(v.empty());
    ASSERT_EQUAL(NULL_TYPE, v.type());
    v = "foo";
    ASSERT_EQUAL(STRING, v.type());
    try { get<null>(v); FAIL("Expected conversion_error"); } catch (conversion_error) {}
    v = null();
    get<null>(v);
}

void get_coerce_test() {
    // Valid conversions
    ASSERT_EQUAL(true, coerce<bool>(value(true)));

    ASSERT_EQUAL(1, coerce<uint8_t>(value(uint8_t(1))));
    ASSERT_EQUAL(-1, coerce<int8_t>(value(int8_t(-1))));

    ASSERT_EQUAL(2, coerce<uint16_t>(value(uint8_t(2))));
    ASSERT_EQUAL(-2, coerce<int16_t>(value(int8_t(-2))));

    ASSERT_EQUAL(3, coerce<uint32_t>(value(uint16_t(3))));
    ASSERT_EQUAL(-3, coerce<int32_t>(value(int16_t(-3))));

    ASSERT_EQUAL(4, coerce<uint64_t>(value(uint32_t(4))));
    ASSERT_EQUAL(-4, coerce<int64_t>(value(int32_t(-4))));

    ASSERT_EQUALISH(1.2, coerce<float>(value(double(1.2))), 0.001);
    ASSERT_EQUALISH(3.4, coerce<double>(value(float(3.4))), 0.001);

    ASSERT_EQUAL(std::string("foo"), coerce<std::string>(value(symbol("foo"))));

    // Bad conversions
    try { get<bool>(value(int8_t(1))); FAIL("byte as bool"); } catch (conversion_error) {}
    try { get<uint8_t>(value(true)); FAIL("bool as uint8_t"); } catch (conversion_error) {}
    try { get<uint8_t>(value(int8_t(1))); FAIL("int8 as uint8"); } catch (conversion_error) {}
    try { get<int16_t>(value(uint16_t(1))); FAIL("uint16 as int16"); } catch (conversion_error) {}
    try { get<int16_t>(value(int32_t(1))); FAIL("int32 as int16"); } catch (conversion_error) {}
    try { get<symbol>(value(std::string())); FAIL("string as symbol"); } catch (conversion_error) {}
}


int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, simple_type_test(false, BOOLEAN, "false", true));
    RUN_TEST(failed, simple_type_test(uint8_t(42), UBYTE, "42", uint8_t(50)));
    RUN_TEST(failed, simple_type_test(int8_t(-42), BYTE, "-42", int8_t(-40)));
    RUN_TEST(failed, simple_type_test(uint16_t(4242), USHORT, "4242", uint16_t(5252)));
    RUN_TEST(failed, simple_type_test(int16_t(-4242), SHORT, "-4242", int16_t(3)));
    RUN_TEST(failed, simple_type_test(uint32_t(4242), UINT, "4242", uint32_t(5252)));
    RUN_TEST(failed, simple_type_test(int32_t(-4242), INT, "-4242", int32_t(3)));
    RUN_TEST(failed, simple_type_test(uint64_t(4242), ULONG, "4242", uint64_t(5252)));
    RUN_TEST(failed, simple_type_test(int64_t(-4242), LONG, "-4242", int64_t(3)));
    RUN_TEST(failed, simple_type_test(wchar_t('X'), CHAR, "X", wchar_t('Y')));
    RUN_TEST(failed, simple_type_test(float(1.234), FLOAT, "1.234", float(2.345)));
    RUN_TEST(failed, simple_type_test(double(11.2233), DOUBLE, "11.2233", double(12)));
    RUN_TEST(failed, simple_type_test(timestamp(1234), TIMESTAMP, "1234", timestamp(12345)));
    RUN_TEST(failed, simple_type_test(make<decimal32>(1), DECIMAL32, "decimal32(0x01010101)", make<decimal32>(2)));
    RUN_TEST(failed, simple_type_test(make<decimal64>(3), DECIMAL64, "decimal64(0x0303030303030303)", make<decimal64>(4)));
    RUN_TEST(failed, simple_type_test(make<decimal128>(5), DECIMAL128, "decimal128(0x05050505050505050505050505050505)", make<decimal128>(6)));
    RUN_TEST(failed, simple_type_test(
                 uuid::copy("\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff"),
                 UUID, "00112233-4455-6677-8899-aabbccddeeff",
                 uuid::copy("\xff\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff")));
    RUN_TEST(failed, simple_type_test(std::string("xxx"), STRING, "xxx", std::string("yyy")));
    RUN_TEST(failed, simple_type_test(symbol("aaa"), SYMBOL, "aaa", symbol("aaaa")));
    RUN_TEST(failed, simple_type_test(binary("\010aaa"), BINARY, "b\"\\x08aaa\"", binary("aaaa")));

    // Test native C++ integral types.
    RUN_TEST(failed, simple_integral_test<char>());
    RUN_TEST(failed, simple_integral_test<signed char>());
    RUN_TEST(failed, simple_integral_test<unsigned char>());
    RUN_TEST(failed, simple_integral_test<short>());
    RUN_TEST(failed, simple_integral_test<int>());
    RUN_TEST(failed, simple_integral_test<long>());
    RUN_TEST(failed, simple_integral_test<unsigned short>());
    RUN_TEST(failed, simple_integral_test<unsigned int>());
    RUN_TEST(failed, simple_integral_test<unsigned long>());
#if PN_HAS_LONG_LONG
    RUN_TEST(failed, simple_integral_test<long long>());
    RUN_TEST(failed, simple_integral_test<unsigned long long>());
#endif

    // Sequence tests
    RUN_TEST(failed, sequence_test<std::list<bool> >(ARRAY, many<bool>() + false + true));
    RUN_TEST(failed, sequence_test<std::vector<int> >(ARRAY, many<int>() + -1 + 2));
    RUN_TEST(failed, sequence_test<std::deque<std::string> >(ARRAY, many<std::string>() + "a" + "b"));
    RUN_TEST(failed, sequence_test<std::deque<symbol> >(ARRAY, many<symbol>() + "a" + "b"));
    RUN_TEST(failed, sequence_test<std::vector<value> >(LIST, many<value>() + value(0) + value("a")));
    RUN_TEST(failed, sequence_test<std::vector<scalar> >(LIST, many<scalar>() + scalar(0) + scalar("a")));

    // // Map tests
    typedef std::pair<std::string, uint64_t> pair;
    many<pair> pairs;
    pairs << pair("a", 0) << pair("b", 1) << pair("c", 2);

    RUN_TEST(failed, (map_test<std::map<std::string, uint64_t> >(pairs)));
    RUN_TEST(failed, (map_test<std::vector<pair> >(pairs)));

    many<std::pair<value,value> > value_pairs(pairs);
    RUN_TEST(failed, (map_test<std::map<value, value> >(value_pairs)));

    many<std::pair<scalar,scalar> > scalar_pairs(pairs);
    RUN_TEST(failed, (map_test<std::map<scalar, scalar> >(scalar_pairs)));

    annotation_key ak(pairs[0].first);
    std::pair<annotation_key, message_id> p(pairs[0]);
    many<std::pair<annotation_key, message_id> > restricted_pairs(pairs);
    RUN_TEST(failed, (map_test<std::map<annotation_key, message_id> >(restricted_pairs)));

#if PN_CPP_HAS_CPP11
    RUN_TEST(failed, sequence_test<std::forward_list<binary> >(ARRAY, many<binary>() + binary("xx") + binary("yy")));
    RUN_TEST(failed, (map_test<std::unordered_map<std::string, uint64_t> >(pairs)));
#endif

    RUN_TEST(failed, get_coerce_test());
    return failed;
}
