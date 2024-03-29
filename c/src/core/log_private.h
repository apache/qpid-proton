#ifndef LOG_PRIVATE_H
#define LOG_PRIVATE_H
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

/**@file
 *
 * Log messages that are not associated with a transport.
 */

#include <proton/annotations.h>
#include <proton/log.h>
#include <stdarg.h>

/** Log a printf style message */
#define pn_logf(...)                            \
    do {                                        \
        if (pni_log_enabled())                   \
            pni_logf_impl(__VA_ARGS__);          \
    } while(0)

/** va_list version of pn_logf */
#define pn_vlogf(fmt, ap)                       \
    do {                                        \
        if (pni_log_enabled())                   \
            pni_vlogf_impl(fmt, ap);             \
    } while(0)

/** Return true if logging is enabled. */
PN_EXTERN bool pni_log_enabled(void);

/**@internal*/
PN_EXTERN void pni_logf_impl(PN_PRINTF_FORMAT const char* fmt, ...)
        PN_PRINTF_FORMAT_ATTR(1, 2);
/**@internal*/
PN_EXTERN void pni_vlogf_impl(const char *fmt, va_list ap);

/**@internal*/
void pn_log_data(const char *msg, const char *bytes, size_t size);


#endif
