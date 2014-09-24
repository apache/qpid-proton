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
 *
 */

/*
 * This file provides the functions for "misc_defs.h" in the form of
 * included code, as opposed to a separate library or object
 * dependency.  In the absence of portable "pragma weak" compiler
 * directives, this provides a simple workaround.
 *
 * Usage for a single compilation unit:
 *
 *  #include "pncompat/misc_funcs.inc"
 *
 * Usage for multiple combined compilation units: chose one to include
 * "pncompat/misc_funcs.inc" as above and in each other unit needing the
 * definitions use
 *
 *  #include "pncompat/misc_defs.h"
 *
 */

#include "misc_defs.h"

#if defined(_WIN32) && ! defined(__CYGWIN__)
#include "pncompat/internal/getopt.c"
#endif

#if defined(_WIN32) && ! defined(__CYGWIN__)
#include <windows.h>
pn_timestamp_t time_now(void)
{
  FILETIME now;
  ULARGE_INTEGER t;
  GetSystemTimeAsFileTime(&now);
  t.u.HighPart = now.dwHighDateTime;
  t.u.LowPart = now.dwLowDateTime;
  // Convert to milliseconds and adjust base epoch
  return t.QuadPart / 10000 - 11644473600000;
}
#else
#include <sys/time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
pn_timestamp_t time_now(void)
{
  struct timeval now;
  if (gettimeofday(&now, NULL)) {fprintf(stderr, "gettimeofday failed\n"); abort();}
  return ((pn_timestamp_t)now.tv_sec) * 1000 + (now.tv_usec / 1000);
}
#endif
