#ifndef SCALAR_TEST_HPP
#define SCALAR_TEST_HPP

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

// Template tests used by both scalar_test.cpp and value_test.hpp to test conversion
// of scalar values via a proton::scalar or a proton::value.

#include "test_bits.hpp"

#include "proton/types.hpp"
#include "proton/error.hpp"

#include <sstream>


namespace test {

using namespace proton;

// Inserting and extracting simple C++ values using same-type get<T> and coerce<T>
template <class V, class T> void simple_type_test(T x, type_id tid, const std::string& s, T y) {
    V vx(x);                    // Construct from C++ value
    ASSERT_EQUAL(tid, vx.type());
    ASSERT(!vx.empty());
    ASSERT_EQUAL(x, get<T>(vx));
    ASSERT_EQUAL(x, coerce<T>(vx));

    V vxa = x;                  // Assign from C++ value
    ASSERT_EQUAL(tid, vxa.type());
    ASSERT(!vx.empty());
    ASSERT_EQUAL(vx, vxa);
    ASSERT_EQUAL(x, get<T>(vxa));
    ASSERT_EQUAL(x, coerce<T>(vxa));

    V v2;                       // Default construct
    ASSERT(v2.type() == NULL_TYPE);
    ASSERT(v2.empty());
    v2 = x;                     // Assign from C++ value
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(vx, v2);
    ASSERT_EQUAL(x, get<T>(v2));
    ASSERT_EQUAL(x, coerce<T>(v2));

    V v3(vx);                   // Copy construct
    ASSERT_EQUAL(tid, v3.type());
    ASSERT_EQUAL(vx, v3);
    ASSERT_EQUAL(x, get<T>(v3));
    ASSERT_EQUAL(x, coerce<T>(v3));

    V v4 = vx;                  // Copy assign
    ASSERT_EQUAL(tid, v4.type());
    ASSERT_EQUAL(x, get<T>(v4));
    ASSERT_EQUAL(x, coerce<T>(v4));

    ASSERT_EQUAL(s, to_string(vx));   // Stringify
    V vy(y);
    ASSERT(vx != vy);           // Compare
    ASSERT(vx < vy);
    ASSERT(vy > vx);
}

// Test native C/C++ integer types via their mapped integer type ([u]int_x_t)
template <class V, class T> void simple_integral_test() {
    typedef typename internal::integer_type<sizeof(T), internal::is_signed<T>::value>::type int_type;
    simple_type_test<V>(T(3), internal::type_id_of<int_type>::value, "3", T(4));
}

// Test invalid gets, valid same-type get<T> is tested by simple_type_test
// Templated to test both scalar and value.
template<class V>  void bad_get_test() {
    try { get<bool>(V(int8_t(1))); FAIL("byte as bool"); } catch (const conversion_error&) {}
    try { get<uint8_t>(V(true)); FAIL("bool as uint8_t"); } catch (const conversion_error&) {}
    try { get<uint8_t>(V(int8_t(1))); FAIL("int8 as uint8"); } catch (const conversion_error&) {}
    try { get<int16_t>(V(uint16_t(1))); FAIL("uint16 as int16"); } catch (const conversion_error&) {}
    try { get<int16_t>(V(int32_t(1))); FAIL("int32 as int16"); } catch (const conversion_error&) {}
    try { get<symbol>(V(std::string())); FAIL("string as symbol"); } catch (const conversion_error&) {}
    try { get<std::string>(V(binary())); FAIL("binary as string"); } catch (const conversion_error&) {}
    try { get<binary>(V(symbol())); FAIL("symbol as binary"); } catch (const conversion_error&) {}
    try { get<binary>(V(timestamp())); FAIL("timestamp as binary"); } catch (const conversion_error&) {}
    try { get<int>(V(timestamp())); FAIL("timestamp as int"); } catch (const conversion_error&) {}
    try { get<timestamp>(V(0)); FAIL("int as timestamp"); } catch (const conversion_error&) {}
    try { get<timestamp>(V(std::string())); FAIL("string as timestamp"); } catch (const conversion_error&) {}
}

// Test some valid coercions and some bad ones with mixed types.
// Templated to test both scalar and value.
template<class V> void coerce_test() {
    // Valid C++ conversions should work with coerce.
    ASSERT_EQUAL(false, coerce<bool>(V(0)));
    ASSERT_EQUAL(true, coerce<bool>(V(-1)));
    ASSERT_EQUAL(true, coerce<bool>(V(int64_t(0xFFFF0000))));

    ASSERT_EQUAL(1, coerce<uint8_t>(V(uint64_t(1)))); // In range.
    ASSERT_EQUAL(1, coerce<uint8_t>(V(uint32_t(0xFF01)))); // int truncate.
    ASSERT_EQUAL(0xFFFF, coerce<uint16_t>(V(int8_t(-1)))); // Sign extend.
    ASSERT_EQUAL(-1, coerce<int32_t>(V(uint64_t(0xFFFFFFFFul)))); // 2s complement

    ASSERT_EQUALISH(1.2f, coerce<float>(V(double(1.2))), 0.001f);
    ASSERT_EQUALISH(3.4, coerce<double>(V(float(3.4))), 0.001);
    ASSERT_EQUALISH(23.0, coerce<double>(V(uint64_t(23))), 0.001); // int to double.
    ASSERT_EQUAL(-1945, coerce<int>(V(float(-1945.123))));    // round to int.

    // String-like conversions.
    ASSERT_EQUAL(std::string("foo"), coerce<std::string>(V(symbol("foo"))));
    ASSERT_EQUAL(std::string("foo"), coerce<std::string>(V(binary("foo"))));

    // Bad coercions, types are not `is_convertible`
    V s("foo");
    try { coerce<bool>(s); FAIL("string as bool"); } catch (const conversion_error&) {}
    try { coerce<int>(s); FAIL("string as int"); } catch (const conversion_error&) {}
    try { coerce<double>(s); FAIL("string as double"); } catch (const conversion_error&) {}

    try { coerce<std::string>(V(0)); FAIL("int as string"); } catch (const conversion_error&) {}
    try { coerce<symbol>(V(true)); FAIL("bool as symbol"); } catch (const conversion_error&) {}
    try { coerce<binary>(V(0.0)); FAIL("double as binary"); } catch (const conversion_error&) {}
    try { coerce<symbol>(V(binary())); FAIL("binary as symbol"); } catch (const conversion_error&) {}
    try { coerce<binary>(V(symbol())); FAIL("symbol as binary"); } catch (const conversion_error&) {}
    try { coerce<binary>(s); } catch (const conversion_error&) {}
    try { coerce<symbol>(s); } catch (const conversion_error&) {}
}

template <class V> void null_test() {
    V v;
    ASSERT(v.empty());
    ASSERT_EQUAL(NULL_TYPE, v.type());
    get<null>(v);
    null n;
    get(v, n);
    V v2(n);
    ASSERT(v.empty());
    ASSERT_EQUAL(NULL_TYPE, v.type());
    v = "foo";
    ASSERT_EQUAL(STRING, v.type());
    try { get<null>(v); FAIL("Expected conversion_error"); } catch (const conversion_error&) {}
    v = null();
    get<null>(v);
}

// Nasty hack for uninterpreted decimal<> types.
template <class T> T make(const char c) { T x; std::fill(x.begin(), x.end(), c); return x; }

template <class V> void scalar_test_group(int& failed) {
    // Direct AMQP-mapped types.
    RUN_TEST(failed, simple_type_test<V>(false, BOOLEAN, "false", true));
    RUN_TEST(failed, simple_type_test<V>(uint8_t(42), UBYTE, "42", uint8_t(50)));
    RUN_TEST(failed, simple_type_test<V>(int8_t(-42), BYTE, "-42", int8_t(-40)));
    RUN_TEST(failed, simple_type_test<V>(uint16_t(4242), USHORT, "4242", uint16_t(5252)));
    RUN_TEST(failed, simple_type_test<V>(int16_t(-4242), SHORT, "-4242", int16_t(3)));
    RUN_TEST(failed, simple_type_test<V>(uint32_t(4242), UINT, "4242", uint32_t(5252)));
    RUN_TEST(failed, simple_type_test<V>(int32_t(-4242), INT, "-4242", int32_t(3)));
    RUN_TEST(failed, simple_type_test<V>(uint64_t(4242), ULONG, "4242", uint64_t(5252)));
    RUN_TEST(failed, simple_type_test<V>(int64_t(-4242), LONG, "-4242", int64_t(3)));
    RUN_TEST(failed, simple_type_test<V>(wchar_t('X'), CHAR, "88", wchar_t('Y')));
    RUN_TEST(failed, simple_type_test<V>(float(1.234), FLOAT, "1.234", float(2.345)));
    RUN_TEST(failed, simple_type_test<V>(double(11.2233), DOUBLE, "11.2233", double(12)));
    RUN_TEST(failed, simple_type_test<V>(timestamp(1234), TIMESTAMP, "1234", timestamp(12345)));
    RUN_TEST(failed, simple_type_test<V>(make<decimal32>(1), DECIMAL32, "decimal32(0x01010101)", make<decimal32>(2)));
    RUN_TEST(failed, simple_type_test<V>(make<decimal64>(3), DECIMAL64, "decimal64(0x0303030303030303)", make<decimal64>(4)));
    RUN_TEST(failed, simple_type_test<V>(make<decimal128>(5), DECIMAL128, "decimal128(0x05050505050505050505050505050505)", make<decimal128>(6)));
    RUN_TEST(failed, simple_type_test<V>(
                 uuid::copy("\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff"),
                 UUID, "00112233-4455-6677-8899-aabbccddeeff",
                 uuid::copy("\xff\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff")));
    RUN_TEST(failed, simple_type_test<V>(std::string("xxx"), STRING, "xxx", std::string("yyy")));
    RUN_TEST(failed, simple_type_test<V>(symbol("aaa"), SYMBOL, "aaa", symbol("aaaa")));
    RUN_TEST(failed, simple_type_test<V>(binary("\010aaa"), BINARY, "b\"\\x08aaa\"", binary("aaaa")));

    // Test native C++ integral types.
    RUN_TEST(failed, (simple_integral_test<V, char>()));
    RUN_TEST(failed, (simple_integral_test<V, signed char>()));
    RUN_TEST(failed, (simple_integral_test<V, unsigned char>()));
    RUN_TEST(failed, (simple_integral_test<V, short>()));
    RUN_TEST(failed, (simple_integral_test<V, int>()));
    RUN_TEST(failed, (simple_integral_test<V, long>()));
    RUN_TEST(failed, (simple_integral_test<V, unsigned short>()));
    RUN_TEST(failed, (simple_integral_test<V, unsigned int>()));
    RUN_TEST(failed, (simple_integral_test<V, unsigned long>()));
#if PN_CPP_HAS_LONG_LONG_TYPE
    RUN_TEST(failed, (simple_integral_test<V, long long>()));
    RUN_TEST(failed, (simple_integral_test<V, unsigned long long>()));
#endif


    RUN_TEST(failed, (coerce_test<V>()));
    RUN_TEST(failed, (null_test<V>()));
    RUN_TEST(failed, (bad_get_test<V>()));
}

}

#endif // SCALAR_TEST_HPP
