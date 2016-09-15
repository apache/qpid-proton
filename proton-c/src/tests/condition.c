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

#include <proton/condition.h>
#include <proton/connection.h>
#include <stdio.h>
#include <string.h>

static int fail = 0;

#define TEST_ASSERT(B)                                  \
    if(!(B)) {                                          \
        ++fail;                                         \
        printf("%s:%d %s\n", __FILE__, __LINE__ , #B); \
    }

int main(int argc, char **argv) {
    pn_connection_t *c = pn_connection();
    pn_condition_t *cond = pn_connection_condition(c);

    // Verify empty
    TEST_ASSERT(!pn_condition_is_set(cond));
    TEST_ASSERT(!pn_condition_get_name(cond));
    TEST_ASSERT(!pn_condition_get_description(cond));

    // Format a condition
    pn_condition_format(cond, "foo", "hello %d", 42);
    TEST_ASSERT(pn_condition_is_set(cond));
    TEST_ASSERT(strcmp("foo", pn_condition_get_name(cond)) == 0);
    TEST_ASSERT(strcmp("hello 42", pn_condition_get_description(cond)) == 0);

    // Clear and verify empty
    pn_condition_clear(cond);
    TEST_ASSERT(!pn_condition_is_set(cond));
    TEST_ASSERT(!pn_condition_get_name(cond));
    TEST_ASSERT(!pn_condition_get_description(cond));

    pn_connection_free(c);
    return fail;
}
