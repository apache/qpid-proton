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

using test::many;
using test::scalar_test_group;

// Inserting and extracting arrays from a container T of type U
template <class T> void sequence_test(
    type_id tid, const many<typename T::value_type>& values, const string& s)
{
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
    typename T::iterator it = y.begin();
    *y.begin() = *(++it); // Second element is bigger so y is lexicographically bigger than x
    value vy(y);
    ASSERT(vx != vy);
    ASSERT(vx < vy);
    ASSERT(vy > vx);

    ASSERT_EQUAL(s, to_string(vx));
}

template <class T, class U> void map_test(const U& values, const string& s) {
    T m(values.begin(), values.end());
    value v(m);
    ASSERT_EQUAL(MAP, v.type());
    T m2(get<T>(v));
    ASSERT_EQUAL(m.size(), m2.size());
    ASSERT_EQUAL(m, m2);
    if (!s.empty())
        ASSERT_EQUAL(s, to_string(v));
}

void null_test() {
    proton::null n;
    ASSERT_EQUAL("null", to_string(n));

    std::vector<proton::value> nulls(2, n);
    ASSERT_EQUAL("[null, null]", to_string(nulls));

    std::vector<proton::null> nulls1(2, n);
    ASSERT_EQUAL("@PN_NULL[null, null]", to_string(nulls1));

    std::vector<proton::value> vs;
    vs.push_back(n);
    vs.push_back(nulls);
    vs.push_back(nulls1);
    ASSERT_EQUAL("[null, [null, null], @PN_NULL[null, null]]", to_string(vs));

    std::map<proton::value, proton::value> vm;
    vm[n] = 1;
    vm[nulls1] = 2;
    vm[nulls] = 3;
    // Different types compare by type-id, so NULL < ARRAY < LIST
    ASSERT_EQUAL("{null=1, @PN_NULL[null, null]=2, [null, null]=3}", to_string(vm));

    std::map<proton::scalar, proton::scalar> vm2;
    vm2[n] = 1;                 // Different types compare by type-id, NULL is smallest
    vm2[2] = n;
    ASSERT_EQUAL("{null=1, 2=null}", to_string(vm2));

#if PN_CPP_HAS_CPP11
    proton::value nn = nullptr;
    ASSERT(n == nn);            // Don't use ASSERT_EQUAL, it will try to print
    ASSERT_EQUAL("null", to_string(nn));
    std::vector<std::nullptr_t> nulls2 {nullptr, nullptr};
    ASSERT_EQUAL("@PN_NULL[null, null]", to_string(nulls2));
    std::map<proton::scalar, proton::scalar> m {{nullptr, nullptr}};
    ASSERT_EQUAL("{null=null}", to_string(m));
    std::map<proton::value, proton::value> m2 {{nullptr, nullptr}};
    ASSERT_EQUAL("{null=null}", to_string(m2));
#endif
}

}

int main(int, char**) {
    try {
        int failed = 0;
        scalar_test_group<value>(failed);

        // Sequence tests
        RUN_TEST(failed, sequence_test<list<bool> >(
                     ARRAY, many<bool>() + false + true, "@PN_BOOL[false, true]"));
        RUN_TEST(failed, sequence_test<vector<int> >(
                     ARRAY, many<int>() + -1 + 2, "@PN_INT[-1, 2]"));
        RUN_TEST(failed, sequence_test<deque<string> >(
                     ARRAY, many<string>() + "a" + "b", "@PN_STRING[\"a\", \"b\"]"));
        RUN_TEST(failed, sequence_test<deque<symbol> >(
                     ARRAY, many<symbol>() + "a" + "b", "@PN_SYMBOL[:a, :b]"));
        RUN_TEST(failed, sequence_test<vector<value> >(
                     LIST, many<value>() + value(0) + value("a"), "[0, \"a\"]"));
        RUN_TEST(failed, sequence_test<vector<scalar> >(
                     LIST, many<scalar>() + scalar(0) + scalar("a"), "[0, \"a\"]"));

        // // Map tests
        typedef pair<string, uint64_t> si_pair;
        many<si_pair> si_pairs;
        si_pairs << si_pair("a", 0) << si_pair("b", 1) << si_pair("c", 2);

        RUN_TEST(failed, (map_test<map<string, uint64_t> >(
                              si_pairs, "{\"a\"=0, \"b\"=1, \"c\"=2}")));
        RUN_TEST(failed, (map_test<vector<si_pair> >(
                              si_pairs, "{\"a\"=0, \"b\"=1, \"c\"=2}")));

        many<std::pair<value,value> > value_pairs(si_pairs);
        RUN_TEST(failed, (map_test<map<value, value> >(
                              value_pairs, "{\"a\"=0, \"b\"=1, \"c\"=2}")));

        many<pair<scalar,scalar> > scalar_pairs(si_pairs);
        RUN_TEST(failed, (map_test<map<scalar, scalar> >(
                              scalar_pairs, "{\"a\"=0, \"b\"=1, \"c\"=2}")));

        annotation_key ak(si_pairs[0].first);
        pair<annotation_key, message_id> p(si_pairs[0]);
        many<pair<annotation_key, message_id> > restricted_pairs(si_pairs);
        RUN_TEST(failed, (map_test<map<annotation_key, message_id> >(
                              restricted_pairs, "{:a=0, :b=1, :c=2}")));
        RUN_TEST(failed, null_test());

#if PN_CPP_HAS_CPP11
        RUN_TEST(failed, sequence_test<forward_list<binary> >(
                     ARRAY, many<binary>() + binary("xx") + binary("yy"), "@PN_BINARY[b\"xx\", b\"yy\"]"));
        RUN_TEST(failed, (map_test<unordered_map<string, uint64_t> >(si_pairs, "")));
#endif
        return failed;
    } catch (const std::exception& e) {
        std::cout << "ERROR in main(): " << e.what() << std::endl;
    }
}
