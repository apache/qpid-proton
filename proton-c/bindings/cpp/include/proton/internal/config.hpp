#ifndef PROTON_INTERNAL_CONFIG_HPP
#define PROTON_INTERNAL_CONFIG_HPP

/*
 *
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
 *
 */

/// @cond INTERNAL

/// @file
///
/// Configuration macros.  They can be set via -D compiler options or
/// in code.
///
/// On a C++11 compliant compiler, all C++11 features are enabled by
/// default.  Otherwise they can be enabled or disabled separately
/// with -D on the compile line.

// Read library compilation presets -
// This sets the options the library itself was compiled with
// and sets up the compilation options is we are compiling the library itself
#include "config_presets.hpp"

/// Whether the library supports threads depends on the configuration of the library compilation only
#define PN_CPP_SUPPORTS_THREADS PN_CPP_LIB_HAS_CPP11 || (PN_CPP_LIB_HAS_STD_THREAD && PN_CPP_LIB_HAS_STD_MUTEX)
/// @endcond

/// The Apple clang compiler doesn't really support PN_CPP_HAS_THREAD_LOCAL
/// before Xcode 8 even though it claims to be C++11 compatible
#if defined(__clang__) && defined(__apple_build_version__) && ((__clang_major__ * 100) + __clang_minor__) >= 301
#if __has_feature(cxx_thread_local)
#define PN_CPP_HAS_THREAD_LOCAL 1
#else
#define PN_CPP_HAS_THREAD_LOCAL 0
#endif
#endif

#ifndef PN_CPP_HAS_CPP11
#if defined(__cplusplus) && __cplusplus >= 201103
#define PN_CPP_HAS_CPP11 1
#else
#define PN_CPP_HAS_CPP11 0
#endif
#endif

#ifndef PN_CPP_HAS_LONG_LONG_TYPE
#define PN_CPP_HAS_LONG_LONG_TYPE PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_NULLPTR
#define PN_CPP_HAS_NULLPTR PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_RVALUE_REFERENCES
#define PN_CPP_HAS_RVALUE_REFERENCES PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_OVERRIDE
#define PN_CPP_HAS_OVERRIDE PN_CPP_HAS_CPP11
#endif

#if PN_CPP_HAS_OVERRIDE
#define PN_CPP_OVERRIDE override
#else
#define PN_CPP_OVERRIDE
#endif

#ifndef PN_CPP_HAS_EXPLICIT_CONVERSIONS
#define PN_CPP_HAS_EXPLICIT_CONVERSIONS PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_DEFAULTED_FUNCTIONS
#define PN_CPP_HAS_DEFAULTED_FUNCTIONS PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_DEFAULTED_MOVE_INITIALIZERS
#define PN_CPP_HAS_DEFAULTED_MOVE_INITIALIZERS PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_DELETED_FUNCTIONS
#define PN_CPP_HAS_DELETED_FUNCTIONS PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_THREAD_LOCAL
#define PN_CPP_HAS_THREAD_LOCAL PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_VARIADIC_TEMPLATES
#define PN_CPP_HAS_VARIADIC_TEMPLATES PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_LAMBDAS
#define PN_CPP_HAS_LAMBDAS PN_CPP_HAS_CPP11
#endif

// Library features

#ifndef PN_CPP_HAS_HEADER_RANDOM
#define PN_CPP_HAS_HEADER_RANDOM PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_STD_UNIQUE_PTR
#define PN_CPP_HAS_STD_UNIQUE_PTR PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_STD_MUTEX
#define PN_CPP_HAS_STD_MUTEX PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_STD_ATOMIC
#define PN_CPP_HAS_STD_ATOMIC PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_STD_THREAD
#define PN_CPP_HAS_STD_THREAD PN_CPP_HAS_CPP11
#endif

#endif // PROTON_INTERNAL_CONFIG_HPP

/// @endcond
