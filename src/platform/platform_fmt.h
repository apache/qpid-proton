#ifndef _PROTON_SRC_PLATFORM_FMT_H
#define _PROTON_SRC_PLATFORM_FMT_H 1

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

/*
 * Platform dependent type-specific format specifiers for PRIx and %z
 * for use with printf.  PRIx defs are normally available in
 * inttypes.h (C99), but extra steps are required for C++, and they
 * are not available in Visual Studio at all.
 * Visual studio uses "%I" for size_t instead of "%z".
 */

#ifndef __cplusplus

// normal case
#include <inttypes.h>
#define PN_ZI "zi"
#define PN_ZU "zu"

#ifdef _OPENVMS

#undef PN_ZI
#undef PN_ZU
#define PN_ZI "i"
#define PN_ZU "u"
#define PRIu64 "llu"
#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "lli"

#endif /* _OPENVMS */

#else

#ifdef _MSC_VER
#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "I64u"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "I64i"

#define PN_ZI "Ii"
#define PN_ZU "Iu"
#else
// Normal C++
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#define PN_ZI "zi"
#define PN_ZU "zu"

#endif /* _MSC_VER */

#endif /* __cplusplus */

#endif /* platform_fmt.h */
