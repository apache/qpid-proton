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
#include "util.h"

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

#ifdef USE_UUID_GENERATE
#include <uuid/uuid.h>
#include <stdlib.h>
char* pn_i_genuuid(void) {
    char *generated = (char *) malloc(37*sizeof(char));
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, generated);
    return generated;
}
#elif USE_UUID_CREATE
#include <uuid.h>
char* pn_i_genuuid(void) {
    char *generated;
    uuid_t uuid;
    uint32_t rc;
    uuid_create(&uuid, &rc);
    // Under FreeBSD the returned string is newly allocated from the heap
    uuid_to_string(&uuid, &generated, &rc);
    return generated;
}
#elif USE_WIN_UUID
#include <rpc.h>
char* pn_i_genuuid(void) {
    unsigned char *generated;
    UUID uuid;
    UuidCreate(&uuid);
    UuidToString(&uuid, &generated);
    char* r = pn_strdup((const char*)generated);
    RpcStringFree(&generated);
    return r;
}
#else
#error "Don't know how to generate uuid strings on this platform"
#endif

#ifdef USE_STRERROR_R
#include <string.h>
static void pn_i_strerror(int errnum, char *buf, size_t buflen) {
  if (strerror_r(errnum, buf, buflen) != 0) pni_fatal("strerror_r() failed\n");
}
#elif USE_STRERROR_S
#include <string.h>
static void pn_i_strerror(int errnum, char *buf, size_t buflen) {
  if (strerror_s(buf, buflen, errnum) != 0) pni_fatal("strerror_s() failed\n");
}
#elif USE_OLD_STRERROR
// This is thread safe on some platforms, and the only option on others
#include <string.h>
static void pn_i_strerror(int errnum, char *buf, size_t buflen) {
  strncpy(buf, strerror(errnum), buflen);
}
#else
#error "Don't know a safe strerror equivalent for this platform"
#endif

int pn_i_error_from_errno(pn_error_t *error, const char *msg)
{
  char err[1024];
  pn_i_strerror(errno, err, 1024);
  int code = PN_ERR;
  if (errno == EINTR)
      code = PN_INTR;
  return pn_error_format(error, code, "%s: %s", msg, err);
}

#ifdef USE_ATOLL
#include <stdlib.h>
int64_t pn_i_atoll(const char* num) {
  return atoll(num);
}
#elif USE_ATOI64
#include <stdlib.h>
int64_t pn_i_atoll(const char* num) {
  return _atoi64(num);
}
#else
#error "Don't know how to convert int64_t values on this platform"
#endif

#ifdef _MSC_VER
// [v]snprintf on Windows only matches C99 when no errors or overflow.
int pn_i_vsnprintf(char *buf, size_t count, const char *fmt, va_list ap) {
  if (fmt == NULL)
    return -1;
  if ((buf == NULL) && (count > 0))
    return -1;
  if (count > 0) {
    int n = vsnprintf_s(buf, count, _TRUNCATE, fmt, ap);
    if (n >= 0)  // no overflow
      return n;  // same as C99
    buf[count-1] = '\0';
  }
  // separate call to get needed buffer size on overflow
  int n = _vscprintf(fmt, ap);
  if (n >= (int) count)
    return n;
  return -1;
}

int pn_i_snprintf(char *buf, size_t count, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = pn_i_vsnprintf(buf, count, fmt, ap);
  va_end(ap);
  return n;
}
#endif
