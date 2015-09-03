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
 * Configuration macros. If set before this file is included (e.g. via -D
 * options to the compiler) they will not be altered, otherwise we will enable
 * everything possible.
 *
 * The macros are:
 * - PN_USE_CPP11 - assume a C++11 or greater compiler. Defaulted from compiler settings.
 * - PN_USE_BOOST - include support for boost smart pointers, default 0.
 */


#ifndef PN_USE_CPP11
#if ((defined(__cplusplus) && __cplusplus >= 201100))
#define PN_USE_CPP11 1
#else
#define PN_USE_CPP11 0
#endif
#endif

#endif // CONFIG_HPP
