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

#include "scalar_test.hpp"

namespace {

using namespace std;
using namespace proton;
using namespace test;

// Inserting and extracting arrays from a container T of type U
template <class T> void sequence_test(type_id tid, const many<typename T::value_type>& values) {
    T x(values.begin(), values.end());

    value vx(x);                // construct
    ASSERT_EQUAL(tid, vx.type());
    ASSERT_EQUAL(x, get<T>(vx));
    ASSERT_EQUAL(x, coerce<T>(vx));
    {
        T y;
        get(vx, y);             // Two argument get.
        ASSERT_EQUAL(x, y);
    }
    {
        T y;
        coerce(vx, y);          // Two argument coerce.
        ASSERT_EQUAL(x, y);
    }
    value v2;                   // assign
    v2 = x;
    ASSERT_EQUAL(tid, v2.type());
    ASSERT_EQUAL(x, get<T>(v2));
    ASSERT_EQUAL(x, coerce<T>(v2));
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

}

int main(int, char**) {
    int failed = 0;
    scalar_test_group<value>(failed);

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
    return failed;
}
