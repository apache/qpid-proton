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

// Pointer conversion test.
// NOTE needs to be run with valgrind to be effective.


#include "test_bits.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"

using namespace std;
using namespace proton;

template <class connection_ptr, class session_ptr>
void test_owning() {

    connection_ptr conn(connection::cast(pn_connection()));
    session& s = conn->default_session();
    session_ptr p = s.ptr();
    session_ptr p2 = s.ptr();
}

template <class connection_ptr, class session_ptr>
void test_counted() {
    connection_ptr conn(connection::cast(pn_connection()), false);
    session& s = conn->default_session();
    session_ptr p = s.ptr();
    session_ptr p2 = s.ptr();
}

void test_auto() {
    std::auto_ptr<connection> conn(connection::cast(pn_connection()));
    session& s = conn->default_session();
    std::auto_ptr<session> p(s.ptr().release());
}

int main(int argc, char** argv) {
    int failed = 0;
    failed += run_test(&test_counted<counted_ptr<connection>,
                       counted_ptr<session> >, "counted");

#if PN_HAS_STD_PTR
    failed += run_test(&test_owning<
                       std::shared_ptr<connection>,
                       std::shared_ptr<session> >,
                       "std::shared");
    failed += run_test(&test_owning<
                       std::unique_ptr<connection>,
                       std::unique_ptr<session> >,
                       "std::unique");
#endif
#if PN_HAS_BOOST
    failed += run_test(&test_owning<
                       boost::shared_ptr<connection>,
                       boost::shared_ptr<session> >,
                       "boost::shared");
    failed += run_test(&test_counted<
                       boost::intrusive_ptr<connection>,
                       boost::intrusive_ptr<session> >,
                       "boost::intrusive");
#endif
    return failed;
}
