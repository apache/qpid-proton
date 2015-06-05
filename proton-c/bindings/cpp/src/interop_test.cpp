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

#include "proton/cpp/Decoder.h"
#include "proton/cpp/Encoder.h"
#include "proton/cpp/Value.h"
#include "./Msg.h"
#include <stdexcept>
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <iosfwd>
#include <unistd.h>

using namespace std;
using namespace proton::reactor;

std::string testsDir;

struct Fail : public logic_error { Fail(const string& what) : logic_error(what) {} };
#define FAIL(WHAT) throw Fail(MSG(__FILE__ << ":" << __LINE__ << ": " << WHAT))
#define ASSERT(TEST) do { if (!(TEST)) FAIL("assert failed: " << #TEST); } while(false)
#define ASSERT_EQUAL(WANT, GOT) if ((WANT) != (GOT)) \
        FAIL(#WANT << " !=  " << #GOT << ": " << WANT << " != " << GOT)


string read(string filename) {
    filename = testsDir+"/interop/"+filename+".amqp";
    ifstream ifs(filename.c_str());
    if (!ifs.good()) FAIL("Can't open " << filename);
    return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
}

template <class T> T get(Decoder& d) {
    T value;
    d >> exact(value);
    return value;
}

template <class T> std::string str(const T& value) {
    ostringstream oss;
    oss << value;
    return oss.str();
}

// Test Data ostream operator
void testDataOstream() {
    Decoder d(read("primitives"));
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(d));
}

// Test extracting to exact AMQP types works corectly, extrating to invalid types fails.
void testDecoderPrimitvesExact() {
    Decoder d(read("primitives"));
    ASSERT(d.more());
    try { get<int8_t>(d); FAIL("got bool as byte"); } catch(Decoder::Error){}
    ASSERT_EQUAL(true, get<bool>(d));
    ASSERT_EQUAL(false, get<bool>(d));
    try { get<int8_t>(d); FAIL("got ubyte as byte"); } catch(Decoder::Error){}
    ASSERT_EQUAL(42, get<uint8_t>(d));
    try { get<int32_t>(d); FAIL("got uint as ushort"); } catch(Decoder::Error){}
    ASSERT_EQUAL(42, get<uint16_t>(d));
    try { get<uint16_t>(d); FAIL("got short as ushort"); } catch(Decoder::Error){}
    ASSERT_EQUAL(-42, get<int16_t>(d));
    ASSERT_EQUAL(12345, get<uint32_t>(d));
    ASSERT_EQUAL(-12345, get<int32_t>(d));
    ASSERT_EQUAL(12345, get<uint64_t>(d));
    ASSERT_EQUAL(-12345, get<int64_t>(d));
    try { get<double>(d); FAIL("got float as double"); } catch(Decoder::Error){}
    ASSERT_EQUAL(0.125, get<float>(d));
    try { get<float>(d); FAIL("got double as float"); } catch(Decoder::Error){}
    ASSERT_EQUAL(0.125, get<double>(d));
    ASSERT(!d.more());
}

// Test inserting primitive sand encoding as AMQP.
void testEncoderPrimitives() {
    Encoder e;
    e << true << false;
    e << uint8_t(42);
    e << uint16_t(42) << int16_t(-42);
    e << uint32_t(12345) << int32_t(-12345);
    e << uint64_t(12345) << int64_t(-12345);
    e << float(0.125) << double(0.125);
    ASSERT_EQUAL("true, false, 42, 42, -42, 12345, -12345, 12345, -12345, 0.125, 0.125", str(e));
    std::string data = e.encode();
    ASSERT_EQUAL(read("primitives"), data);
}

// Test type conversions.
void testValueConversions() {
    Value v;
    ASSERT_EQUAL(true, bool(v = true));
    ASSERT_EQUAL(2, int(v=Byte(2)));
    ASSERT_EQUAL(3, long(v=Byte(3)));
    ASSERT_EQUAL(3, long(v=Byte(3)));
    ASSERT_EQUAL(1.0, double(v=Float(1.0)));
    ASSERT_EQUAL(1.0, float(v=Double(1.0)));
    try { bool(v = Byte(1)); FAIL("got byte as bool"); } catch (Decoder::Error) {}
    try { float(v = true); FAIL("got bool as float"); } catch (Decoder::Error) {}
}

int run_test(void (*testfn)(), const char* name) {
    try {
        testfn();
        return 0;
    } catch(const Fail& e) {
        cout << "FAIL " << name << endl << e.what();
    } catch(const std::exception& e) {
        cout << "ERROR " << name << endl << e.what();
    }
    return 1;
}

// FIXME aconway 2015-06-11: not testing all types.

#define RUN_TEST(T) run_test(&T, #T)

int main(int argc, char** argv) {
    int failed = 0;
    char buf[1024];
    if (argc != 2) FAIL("Usage: " << argv[0] << " tests-dir" << " IN " << getcwd(buf, sizeof(buf)));
    testsDir = argv[1];

    failed += RUN_TEST(testDataOstream);
    failed += RUN_TEST(testDecoderPrimitvesExact);
    failed += RUN_TEST(testEncoderPrimitives);
    failed += RUN_TEST(testValueConversions);
    return failed;
}
