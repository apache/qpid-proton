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

#include "proton/decoder.hpp"
#include "proton/encoder.hpp"
#include "proton/data.hpp"
#include "test_bits.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <iosfwd>

using namespace std;
using namespace proton;

std::string tests_dir;

string read(string filename) {
    filename = tests_dir+string("/interop/")+filename+string(".amqp");
    ifstream ifs(filename.c_str());
    if (!ifs.good()) FAIL("Can't open " << filename);
    return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
}

template <class T> T get(decoder& d) { return d.get_as<T, type_id_of<T>::value>(); }

template <class T> std::string str(const T& value) {
    ostringstream oss;
    oss << value;
    return oss.str();
}

// Test data ostream operator
void test_data_ostream() {
    data_value dv;
    dv.decoder().decode(read("primitives"));
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(dv));
}

// Test extracting to exact AMQP types works corectly, extrating to invalid types fails.
void test_decoder_primitves_exact() {
    data_value dv;
    dv.decoder().decode(read("primitives"));
    decoder& d(dv.decoder());
    ASSERT(d.more());
    try { get< ::int8_t>(d); FAIL("got bool as byte"); } catch(decode_error){}
    ASSERT_EQUAL(true, get<bool>(d));
    ASSERT_EQUAL(false, get<bool>(d));
    try { get< ::int8_t>(d); FAIL("got ubyte as byte"); } catch(decode_error){}
    ASSERT_EQUAL(42, get< ::uint8_t>(d));
    try { get< ::int32_t>(d); FAIL("got uint as ushort"); } catch(decode_error){}
    ASSERT_EQUAL(42, get< ::uint16_t>(d));
    try { get< ::uint16_t>(d); FAIL("got short as ushort"); } catch(decode_error){}
    ASSERT_EQUAL(-42, get< ::int16_t>(d));
    ASSERT_EQUAL(12345, get< ::uint32_t>(d));
    ASSERT_EQUAL(-12345, get< ::int32_t>(d));
    ASSERT_EQUAL(12345, get< ::uint64_t>(d));
    ASSERT_EQUAL(-12345, get< ::int64_t>(d));
    try { get<double>(d); FAIL("got float as double"); } catch(decode_error){}
    ASSERT_EQUAL(0.125, get<float>(d));
    try { get<float>(d); FAIL("got double as float"); } catch(decode_error){}
    ASSERT_EQUAL(0.125, get<double>(d));
    ASSERT(!d.more());
}

// Test inserting primitive sand encoding as AMQP.
void test_encoder_primitives() {
    data_value dv;
    encoder& e = dv.encoder();
    e << true << false;
    e << ::uint8_t(42);
    e << ::uint16_t(42) << ::int16_t(-42);
    e << ::uint32_t(12345) << ::int32_t(-12345);
    e << ::uint64_t(12345) << ::int64_t(-12345);
    e << float(0.125) << double(0.125);
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(e.data()));
    std::string data = e.encode();
    ASSERT_EQUAL(read("primitives"), data);
}

// Test type conversions.
void test_value_conversions() {
    data_value v;
    ASSERT_EQUAL(true, bool(v = true));
    ASSERT_EQUAL(2, int(v=amqp_byte(2)));
    ASSERT_EQUAL(3, long(v=amqp_byte(3)));
    ASSERT_EQUAL(3, long(v=amqp_byte(3)));
    ASSERT_EQUAL(1.0, double(v=amqp_float(1.0)));
    ASSERT_EQUAL(1.0, float(v=amqp_double(1.0)));
    try { (void)bool(v = amqp_byte(1)); FAIL("got byte as bool"); } catch (decode_error) {}
    try { (void)float(v = true); FAIL("got bool as float"); } catch (decode_error) {}
}

// TODO aconway 2015-06-11: interop test is not complete.

int main(int argc, char** argv) {
    int failed = 0;
    if (argc != 2) FAIL("Usage: " << argv[0] << " tests-dir");
    tests_dir = argv[1];

    failed += RUN_TEST(test_data_ostream);
    failed += RUN_TEST(test_decoder_primitves_exact);
    failed += RUN_TEST(test_encoder_primitives);
    failed += RUN_TEST(test_value_conversions);
    return failed;
}
