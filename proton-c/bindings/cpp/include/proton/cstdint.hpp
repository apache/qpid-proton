#ifndef CSTDINT_HPP
#define CSTDINT_HPP
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

///@cond INTERNAL_DETAIL

/**@file
 *
 * Workaround for compilers that lack cstdint.
 * Not the full cstdint, just the type needed by proton.
 */

#ifndef PN_HAS_CSTDINT
#define PN_HAS_CSTDINT __cplusplus >= 201100
#endif

#if PN_HAS_CSTDINT
#include <cstdint>
#else
#include <proton/type_compat.h>

namespace std {
// Exact-size integer types.
using ::int8_t;
using ::int16_t;
using ::int32_t;
using ::int64_t;
using ::uint8_t;
using ::uint16_t;
using ::uint32_t;
using ::uint64_t;
}
#endif

///@endcond
#endif // CSTDINT_HPP
