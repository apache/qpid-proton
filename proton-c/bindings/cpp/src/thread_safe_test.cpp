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

/// Test reference counting for object wrappers, threads_safe<> wrappers and returned<> wrappers.
///

#include "test_bits.hpp"
#include "proton_bits.hpp"

#include "proton/thread_safe.hpp"
#include "proton/io/connection_engine.hpp"

#include <proton/connection.h>

namespace {

using namespace std;
using namespace proton;

void test_new() {
    pn_connection_t* c = 0;
    thread_safe<connection>* p = 0;
    {
        io::connection_engine e;
        c = unwrap(e.connection());
        int r = pn_refcount(c);
        ASSERT(r >= 1); // engine may have internal refs (transport, collector).
        p = make_thread_safe(e.connection()).release();
        ASSERT_EQUAL(r+1, pn_refcount(c));
        delete p;
        ASSERT_EQUAL(r, pn_refcount(c));
        p = make_thread_safe(e.connection()).release();
    }
    ASSERT_EQUAL(1, pn_refcount(c)); // Engine gone, thread_safe keeping c alive.
    delete p;

#if PN_CPP_HAS_CPP11
    {
        std::shared_ptr<thread_safe<connection> > sp;
        {
            io::connection_engine e;
            c = unwrap(e.connection());
            sp = make_shared_thread_safe(e.connection());
        }
        ASSERT_EQUAL(1, pn_refcount(c)); // Engine gone, sp keeping c alive.
    }
    {
        std::unique_ptr<thread_safe<connection> > up;
        {
            io::connection_engine e;
            c = unwrap(e.connection());
            up = make_unique_thread_safe(e.connection());
        }
        ASSERT_EQUAL(1, pn_refcount(c)); // Engine gone, sp keeping c alive.
    }
#endif
}

void test_convert() {
    // Verify refcounts as expected with conversion between proton::object
    // and thread_safe.
    connection c;
    pn_connection_t* pc = 0;
    {
        io::connection_engine eng;
        c = eng.connection();
        pc = unwrap(c);         // Unwrap in separate scope to avoid confusion from temp values.
    }
    {
        ASSERT_EQUAL(1, pn_refcount(pc));
        returned<connection> pptr = make_thread_safe(c);
        ASSERT_EQUAL(2, pn_refcount(pc));
        returned<connection> pp2 = pptr;
        ASSERT(!pptr.release()); // Transferred to pp2
        ASSERT_EQUAL(2, pn_refcount(pc));
        connection c2 = pp2;        // Transfer and convert to target
        ASSERT_EQUAL(3, pn_refcount(pc)); // c, c2, thread_safe.
        ASSERT(c == c2);
    }
    ASSERT_EQUAL(1, pn_refcount(pc)); // only c is left
}

}

int main(int, char**) {
    int failed = 0;
    RUN_TEST(failed, test_new());
    RUN_TEST(failed, test_convert());
    return failed;
}
