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

#ifndef PN_CPP_HAS_CPP11
#if defined(__cplusplus) && __cplusplus >= 201100
#define PN_CPP_HAS_CPP11 1
#else
#define PN_CPP_HAS_CPP11 0
#endif
#endif

#ifndef PN_CPP_HAS_SHARED_PTR
#define PN_CPP_HAS_SHARED_PTR PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_UNIQUE_PTR
#define PN_CPP_HAS_UNIQUE_PTR PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_LONG_LONG
#define PN_CPP_HAS_LONG_LONG PN_CPP_HAS_CPP11
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

#ifndef PN_CPP_HAS_DELETED_FUNCTIONS
#define PN_CPP_HAS_DELETED_FUNCTIONS PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_STD_FUNCTION
#define PN_CPP_HAS_STD_FUNCTION PN_CPP_HAS_CPP11
#endif

#ifndef PN_CPP_HAS_CHRONO
#define PN_CPP_HAS_CHRONO PN_CPP_HAS_CPP11
#endif

#endif // PROTON_INTERNAL_CONFIG_HPP

/// @endcond
