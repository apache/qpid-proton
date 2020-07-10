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

#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PN_WINAPI
#include <windows.h>
int pn_i_getpid() {
  return (int) GetCurrentProcessId();
}
#else
#include <unistd.h>
int pn_i_getpid() {
  return (int) getpid();
}
#endif

void pni_vfatal(const char *fmt, va_list ap)
{
  vfprintf(stderr, fmt, ap);
  abort();
}

void pni_fatal(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pni_vfatal(fmt, ap);
  va_end(ap);
}

/* Allow for systems that do not implement clock_gettime()*/
#ifdef USE_CLOCK_GETTIME
#include <time.h>
pn_timestamp_t pn_i_now(void)
{
  struct timespec now;
  if (clock_gettime(CLOCK_REALTIME, &now)) pni_fatal("clock_gettime() failed\n");
  return ((pn_timestamp_t)now.tv_sec) * 1000 + (now.tv_nsec / 1000000);
}
#elif defined(USE_WIN_FILETIME)
#include <windows.h>
pn_timestamp_t pn_i_now(void)
{
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  ULARGE_INTEGER t;
  t.u.HighPart = now.dwHighDateTime;
  t.u.LowPart = now.dwLowDateTime;
  // Convert to milliseconds and adjust base epoch
  return t.QuadPart / 10000 - 11644473600000;
}
#else
#include <sys/time.h>
pn_timestamp_t pn_i_now(void)
{
  struct timeval now;
  if (gettimeofday(&now, NULL)) pni_fatal("gettimeofday failed\n");
  return ((pn_timestamp_t)now.tv_sec) * 1000 + (now.tv_usec / 1000);
}
#endif

#include <string.h>
#include <stdio.h>
static void pn_i_strerror(int errnum, char *buf, size_t buflen)
{
  // PROTON-1029 provide a simple default in case strerror fails
  snprintf(buf, buflen, "errno: %d", errnum);
#ifdef USE_STRERROR_R
  strerror_r(errnum, buf, buflen);
#elif USE_STRERROR_S
  strerror_s(buf, buflen, errnum);
#elif USE_OLD_STRERROR
  strncpy(buf, strerror(errnum), buflen);
#endif
}

int pn_i_error_from_errno(pn_error_t *error, const char *msg)
{
  char err[1024];
  pn_i_strerror(errno, err, 1024);
  int code = PN_ERR;
  if (errno == EINTR)
      code = PN_INTR;
  return pn_error_format(error, code, "%s: %s", msg, err);
}
