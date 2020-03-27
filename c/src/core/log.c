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

#include "log_private.h"
#include "logger_private.h"
#include "util.h"

#include <proton/error.h>
#include <proton/log.h>
#include <proton/object.h>

static int enabled_env  = -1;   /* Set from environment variable. */
static int enabled_call = -1;   /* set by pn_log_enable */

void pn_log_enable(bool value) {
    enabled_call = value;
}

bool pni_log_enabled(void) {
    if (enabled_call != -1) return enabled_call; /* Takes precedence */
    if (enabled_env == -1) 
        enabled_env = pn_env_bool("PN_TRACE_LOG");
    return enabled_env;
}

void pn_log_logger(void (*new_logger)(const char* message)) {
  if (!new_logger) pn_log_enable(false);
}

void pni_vlogf_impl(const char *fmt, va_list ap) {
  pni_logger_vlogf(pn_default_logger(), PN_SUBSYSTEM_ALL, PN_LEVEL_TRACE, fmt, ap);
}

/**@internal
 *
 * Note: We check pni_log_enabled() in the pn_logf macro *before* calling
 * pni_logf_impl because evaluating the arguments to that call could have
 * side-effects with performance impact (e.g. calling functions to construct
 * complicated messages.) It is important that a disabled log statement results
 * in nothing more than a call to pni_log_enabled().
 */
void pni_logf_impl(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pni_vlogf_impl(fmt, ap);
  va_end(ap);
}

void pn_log_data(const char *msg, const char *bytes, size_t size)
{
  if (!pni_log_enabled()) return;

  char buf[256];
  ssize_t n = pn_quote_data(buf, 256, bytes, size);
  if (n >= 0) {
    pni_logf_impl("%s: %s", msg, buf);
  } else if (n == PN_OVERFLOW) {
    pni_logf_impl("%s: %s (truncated)", msg, buf);
  } else {
    pni_logf_impl("%s: cannot log data: %s", msg, pn_code(n));
  }
}
