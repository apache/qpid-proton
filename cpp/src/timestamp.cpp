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

#include "proton/timestamp.hpp"

#include "proton/internal/config.hpp"
#include <proton/error.hpp>
#include <proton/proactor.h>
#include <proton/types.h>

#include <iostream>

// Can't use std::chrono since we still support C++03, sigh.

#ifdef WIN32
#include <windows.h>

#else
#include <sys/time.h>
#endif

namespace proton {

#ifdef WIN32

timestamp timestamp::now() {
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  ULARGE_INTEGER t;
  t.u.HighPart = now.dwHighDateTime;
  t.u.LowPart = now.dwLowDateTime;
  // Convert to milliseconds and adjust base epoch
  return timestamp(t.QuadPart / 10000 - 11644473600000);
}

#else

timestamp timestamp::now() {
  struct timeval now;
  if (::gettimeofday(&now, NULL)) throw proton::error("gettimeofday failed");
  return timestamp(int64_t(now.tv_sec) * 1000 + (now.tv_usec / 1000));
}

#endif

std::ostream& operator<<(std::ostream& o, timestamp ts) { return o << ts.milliseconds(); }

}
