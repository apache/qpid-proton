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

#include <catch.hpp>
#include <proton/internal/object.hpp>
// for pn_data
#include <proton/codec.h>

namespace {

TEST_CASE("pn_ptr_base(pn_data)", "[object]") {
    SECTION("incref and decref in constructor") {
        pn_data_t *o = pn_data(0);
        CHECK(pn_refcount(o) == 1);
        {
            proton::internal::pn_ptr<pn_data_t> ptr = proton::internal::pn_ptr<pn_data_t>(o);
            CHECK(pn_refcount(o) == 2);
        }
        CHECK(pn_refcount(o) == 1);
        pn_data_free(o);
    }
    SECTION("inspect") {
        pn_data_t *o = pn_data(0);
        {
            proton::internal::pn_ptr<pn_data_t> ptr = proton::internal::pn_ptr<pn_data_t>(o);
            CHECK(ptr.inspect().empty());
        }
        pn_data_free(o);
    }
}

}// namespace
