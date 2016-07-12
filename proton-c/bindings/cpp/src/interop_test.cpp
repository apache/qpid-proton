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

#include "proton/codec/decoder.hpp"
#include "proton/codec/encoder.hpp"
#include "proton/error.hpp"
#include "proton/value.hpp"
#include "test_bits.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <iosfwd>

namespace {

using namespace std;
using namespace proton;

using proton::codec::encoder;
using proton::codec::decoder;

using proton::internal::data;

using test::str;

std::string tests_dir;

string read(string filename) {
    filename = tests_dir+string("/interop/")+filename+string(".amqp");
    ifstream ifs(filename.c_str());
    if (!ifs.good()) FAIL("Can't open " << filename);
    return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
}

// Test data ostream operator
void test_data_ostream() {
    data dt(data::create());
    decoder d(dt);
    d.decode(read("primitives"));
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(dt));
}

// Test extracting to exact AMQP types works corectly, extrating to invalid types fails.
void test_decoder_primitves_exact() {
    value dv;
    decoder d(dv);
    d.decode(read("primitives"));
    ASSERT(d.more());
    try { get< ::int8_t>(d); FAIL("got bool as byte"); } catch(conversion_error){}
    ASSERT_EQUAL(true, get<bool>(d));
    ASSERT_EQUAL(false, get<bool>(d));
    try { get< ::int8_t>(d); FAIL("got ubyte as byte"); } catch(conversion_error){}
    ASSERT_EQUAL(42, get< ::uint8_t>(d));
    try { get< ::int32_t>(d); FAIL("got uint as ushort"); } catch(conversion_error){}
    ASSERT_EQUAL(42, get< ::uint16_t>(d));
    try { get< ::uint16_t>(d); FAIL("got short as ushort"); } catch(conversion_error){}
    ASSERT_EQUAL(-42, get< ::int16_t>(d));
    ASSERT_EQUAL(12345u, get< ::uint32_t>(d));
    ASSERT_EQUAL(-12345, get< ::int32_t>(d));
    ASSERT_EQUAL(12345u, get< ::uint64_t>(d));
    ASSERT_EQUAL(-12345, get< ::int64_t>(d));
    try { get<double>(d); FAIL("got float as double"); } catch(conversion_error){}
    ASSERT_EQUAL(0.125f, get<float>(d));
    try { get<float>(d); FAIL("got double as float"); } catch(conversion_error){}
    ASSERT_EQUAL(0.125, get<double>(d));
    ASSERT(!d.more());
}

// Test inserting primitive sand encoding as AMQP.
void test_encoder_primitives() {
    value dv;
    encoder e(dv);
    e << true << false;
    e << ::uint8_t(42);
    e << ::uint16_t(42) << ::int16_t(-42);
    e << ::uint32_t(12345) << ::int32_t(-12345);
    e << ::uint64_t(12345) << ::int64_t(-12345);
    e << float(0.125) << double(0.125);
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(str(e)));
    std::string data = e.encode();
    ASSERT_EQUAL(read("primitives"), data);
}

}

int main(int argc, char** argv) {
    int failed = 0;
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " tests-dir" << endl;
        return 1;
    }
    tests_dir = argv[1];

    RUN_TEST(failed, test_data_ostream());
    RUN_TEST(failed, test_decoder_primitves_exact());
    RUN_TEST(failed, test_encoder_primitives());
    return failed;
}
