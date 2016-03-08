#ifndef CODEC_TEST_HPP
#define CODEC_TEST_HPP
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
#include <proton/data.hpp>

using namespace test;
using namespace proton;

template <class T> void  simple_type_test(const T& x) {
    ASSERT(codec::is_encodable<T>::value);
    value v;
    codec::encoder e(v);
    e << x;
    T y;
    codec::decoder d(v);
    d >> y;
    ASSERT_EQUAL(x, y);
}

template <class T> T make_fill(const char c) {
    T x; std::fill(x.begin(), x.end(), c);
    return x;
}

template <class T> void  uncodable_type_test() {
    ASSERT(!codec::is_encodable<T>::value);
}

int main(int, char**) {
    int failed = 0;

    // Basic AMQP types
    RUN_TEST(failed, simple_type_test(false));
    RUN_TEST(failed, simple_type_test(uint8_t(42)));
    RUN_TEST(failed, simple_type_test(int8_t(-42)));
    RUN_TEST(failed, simple_type_test(uint16_t(4242)));
    RUN_TEST(failed, simple_type_test(int16_t(-4242)));
    RUN_TEST(failed, simple_type_test(uint32_t(4242)));
    RUN_TEST(failed, simple_type_test(int32_t(-4242)));
    RUN_TEST(failed, simple_type_test(uint64_t(4242)));
    RUN_TEST(failed, simple_type_test(int64_t(-4242)));
    RUN_TEST(failed, simple_type_test(wchar_t('X')));
    RUN_TEST(failed, simple_type_test(float(1.234)));
    RUN_TEST(failed, simple_type_test(double(11.2233)));
    RUN_TEST(failed, simple_type_test(timestamp(1234)));
    RUN_TEST(failed, simple_type_test(make_fill<decimal32>(0)));
    RUN_TEST(failed, simple_type_test(make_fill<decimal64>(0)));
    RUN_TEST(failed, simple_type_test(make_fill<decimal128>(0)));
    RUN_TEST(failed, simple_type_test(uuid::copy("\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff")));
    RUN_TEST(failed, simple_type_test(std::string("xxx")));
    RUN_TEST(failed, simple_type_test(symbol("aaa")));
    RUN_TEST(failed, simple_type_test(binary("aaa")));

    // Native int type that may map differently per platform to uint types.
    RUN_TEST(failed, simple_type_test(char(42)));
    RUN_TEST(failed, simple_type_test(short(42)));
    RUN_TEST(failed, simple_type_test(int(42)));
    RUN_TEST(failed, simple_type_test(long(42)));

    RUN_TEST(failed, simple_type_test(static_cast<signed char>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<signed short>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<signed int>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<signed long>(42)));

    RUN_TEST(failed, simple_type_test(static_cast<unsigned char>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<unsigned short>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<unsigned int>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<unsigned long>(42)));

#if PN_HAS_LONG_LONG
    RUN_TEST(failed, simple_type_test(static_cast<long>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<signed long>(42)));
    RUN_TEST(failed, simple_type_test(static_cast<unsigned long>(42)));
#endif

    // value and scalar types, more tests in value_test and scalar_test.
    RUN_TEST(failed, simple_type_test(value("foo")));
    value v(23);                // Make sure we can take a non-const ref also
    RUN_TEST(failed, simple_type_test(v));
    RUN_TEST(failed, simple_type_test(scalar(23)));
    RUN_TEST(failed, simple_type_test(annotation_key(42)));
    RUN_TEST(failed, simple_type_test(message_id(42)));

    // Make sure we reject uncodable types
    RUN_TEST(failed, (uncodable_type_test<std::pair<int, float> >()));
    RUN_TEST(failed, (uncodable_type_test<std::pair<scalar, value> >()));
    RUN_TEST(failed, (uncodable_type_test<std::basic_string<wchar_t> >()));
    RUN_TEST(failed, (uncodable_type_test<codec::data>()));
    RUN_TEST(failed, (uncodable_type_test<pn_data_t*>()));

    return failed;
}


#endif // CODEC_TEST_HPP
