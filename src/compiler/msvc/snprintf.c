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

#include "platform/platform.h"

#include <stdarg.h>
#include <stdio.h>

// [v]snprintf on Windows only matches C99 when no errors or overflow.
// Note: [v]snprintf behavior changed in VS2015 to be C99 compliant.
// vsnprintf_s is unchanged.  This platform code can go away some day.


int pni_vsnprintf(char *buf, size_t count, const char *fmt, va_list ap) {
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

int pni_snprintf(char *buf, size_t count, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = pni_vsnprintf(buf, count, fmt, ap);
  va_end(ap);
  return n;
}
