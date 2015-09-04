#ifndef CONFIG_HPP
#define CONFIG_HPP
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

/**@file
 *
 * Configuration macros, can be set via -D compiler options or in code. 
 *
 * - PN_HAS_BOOST - include support for boost::shared_ptr and boost::intrusive_ptr
 * - PN_HAS_STD_PTR - include support for c++11 std::shared_ptr and std::unique_ptr
 * - PN_HAS_LONG_LONG - include support for the 'long long' type.
 */

#if ((defined(__cplusplus) && __cplusplus >= 201100))

#define PN_HAS_CPP11 1

#ifndef PN_HAS_STD_PTR
#define PN_HAS_STD_PTR 1
#endif

#ifndef PN_HAS_LONG_LONG
#define PN_HAS_LONG_LONG 1
#endif

#ifndef PN_HAS_STATIC_ASSERT
#define PN_HAS_STATIC_ASSERT 1
#endif

#endif

#if defined(BOOST_VERSION)

#ifndef PN_HAS_BOOST
#define PN_HAS_BOOST 1
#endif

#endif

#endif // CONFIG_HPP
