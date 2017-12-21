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


#include "proton/map.hpp"
#include "test_bits.hpp"

#include <string>
#include <vector>

namespace {

using namespace std;
using namespace proton;

void test_empty() {
    proton::map<string, scalar> m;
    ASSERT_EQUAL(0U, m.size());
    ASSERT(m.empty());
    ASSERT_EQUAL(0U, m.erase("x"));
    ASSERT(!m.exists("x"));

    std::map<string, scalar> sm;
    proton::get(m, sm);
    ASSERT(sm.empty());
}

void test_use() {
    proton::map<string, scalar> m;

    m.put("x", "y");
    ASSERT_EQUAL(scalar("y"), m.get("x"));
    ASSERT(!m.empty());
    ASSERT(m.exists("x"));
    ASSERT_EQUAL(1U, m.size());

    m.put("a", "b");
    ASSERT_EQUAL(scalar("b"), m.get("a"));
    ASSERT_EQUAL(2U, m.size());

    ASSERT_EQUAL(1U, m.erase("x"));
    ASSERT_EQUAL(1U, m.size());
    ASSERT(!m.exists("x"));
    ASSERT_EQUAL(scalar("b"), m.get("a"));

    m.clear();
    ASSERT(m.empty());
}

void test_cppmap() {
    std::map<string, scalar> sm;
    sm["a"] = 2;
    sm["b"] = 3;
    proton::map<string, scalar> m;
    m = sm;
    ASSERT_EQUAL(scalar(2), m.get("a"));
    ASSERT_EQUAL(scalar(3), m.get("b"));
    ASSERT_EQUAL(2U, m.size());

    std::map<string, scalar> sm2;
    proton::get(m, sm2);
    ASSERT_EQUAL(2U, sm2.size());
    ASSERT_EQUAL(scalar(2), sm2["a"]);
    ASSERT_EQUAL(scalar(3), sm2["b"]);

    // Round trip:
    value v = m.value();
    proton::map<string, scalar> m2;
    m2.value(v);

    // Use a vector as map storage
    vector<pair<string, scalar> > vm;

    vm.push_back(std::make_pair(string("x"), 8));
    vm.push_back(std::make_pair(string("y"), 9));
    m.value(vm);                // Can't use type-safe op=, not enabled
    ASSERT_EQUAL(scalar(8), m.get("x"));
    ASSERT_EQUAL(scalar(9), m.get("y"));
    ASSERT_EQUAL(2U, m.size());

    vm.clear();
    proton::get(m, vm);
    ASSERT_EQUAL(2U, vm.size());
    ASSERT_EQUAL(string("x"), vm[0].first);
    ASSERT_EQUAL(scalar(8), vm[0].second);
}

void test_value() {
    proton::map<string, scalar> m;
    value v;
    v = "foo";
    ASSERT_THROWS(conversion_error, m.value(v));
    std::map<int, float> bad;
    // Wrong type of map.
    // Note we can't detect an empty map of bad type because AMQP maps allow
    // mixed types, so there must be data to object to.
    bad[1]=1.0;
    ASSERT_THROWS(conversion_error, m.value(bad));
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_empty());
    RUN_TEST(failed, test_use());
    RUN_TEST(failed, test_cppmap());
    RUN_TEST(failed, test_value());
    return failed;
}
