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

#include <proton/log.h>
#include <proton/object.h>
#include <stdio.h>
#include "log_private.h"
#include "util.h"


static void stderr_logger(const char *message) {
    fprintf(stderr, "%s\n", message);
}

static pn_logger_t logger = stderr_logger;
static int enabled_env  = -1;   /* Set from environment variable. */
static int enabled_call = -1;   /* set by pn_log_enable */

void pn_log_enable(bool value) {
    enabled_call = value;
}

bool pn_log_enabled(void) {
    if (enabled_call != -1) return enabled_call; /* Takes precedence */
    if (enabled_env == -1) 
        enabled_env = pn_env_bool("PN_TRACE_LOG");
    return enabled_env;
}

void pn_log_logger(pn_logger_t new_logger) {
    logger = new_logger;
    if (!logger) pn_log_enable(false);
}

void pn_vlogf_impl(const char *fmt, va_list ap) {
    pn_string_t *msg = pn_string("");
    pn_string_vformat(msg, fmt, ap);
    fprintf(stderr, "%s\n", pn_string_get(msg));
}

/**@internal
 *
 * Note: We check pn_log_enabled() in the pn_logf macro *before* calling
 * pn_logf_impl because evaluating the arguments to that call could have
 * side-effects with performance impact (e.g. calling functions to construct
 * complicated messages.) It is important that a disabled log statement results
 * in nothing more than a call to pn_log_enabled().
 */
void pn_logf_impl(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pn_vlogf_impl(fmt, ap);
  va_end(ap);
}

