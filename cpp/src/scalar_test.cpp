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

using test::scalar_test_group;

// NOTE: proton::coerce<> and bad proton::get() are tested in value_test to avoid redundant test code.

void encode_decode_test() {
    value v;
    scalar a("foo");
    v = a;                      // Assignment to value does encode, get<> does decode.
    ASSERT_EQUAL(v, a);
    ASSERT_EQUAL(std::string("foo"), get<std::string>(v));
    scalar a2 = get<scalar>(v);
    ASSERT_EQUAL(std::string("foo"), get<std::string>(a2));
}

void message_id_test() {
    ASSERT_EQUAL(23, coerce<int64_t>(message_id(23)));
    ASSERT_EQUAL(23u, get<uint64_t>(message_id(23)));
    ASSERT(message_id("foo") != message_id(binary("foo")));
    ASSERT_EQUAL(scalar("foo"), message_id("foo"));
    ASSERT_EQUAL("foo", coerce<std::string>(message_id("foo")));
    ASSERT(message_id("a") < message_id("z"));
    uuid r = uuid::random();
    ASSERT_EQUAL(r, get<uuid>(message_id(r)));
}

void annotation_key_test() {
    ASSERT_EQUAL(23, coerce<int64_t>(annotation_key(23)));
    ASSERT_EQUAL(23u, get<uint64_t>(annotation_key(23)));
    ASSERT_EQUAL("foo", coerce<std::string>(annotation_key("foo")));
    ASSERT_EQUAL(scalar(symbol("foo")), annotation_key("foo"));
}

template <class T> T make(const char c) { T x; std::fill(x.begin(), x.end(), c); return x; }

}

int main(int, char**) {
    int failed = 0;
    scalar_test_group<scalar>(failed);

    RUN_TEST(failed, encode_decode_test());
    RUN_TEST(failed, message_id_test());
    RUN_TEST(failed, annotation_key_test());
    return failed;
}
