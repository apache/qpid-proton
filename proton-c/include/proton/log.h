#ifndef LOG_H
#define LOG_H
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
 * Debug logging for messages that are not associated with a transport.
 * See pn_transport_trace for transport-related logging.
 */

#include <proton/import_export.h>
#include <proton/type_compat.h>
#include <stdarg.h>

/** Callback for customized logging */
typedef void (*pn_logger_t)(const char *message);

/** Initialize the logging module, enables logging based on the PN_LOG_TRACE envionment variable.
 *
 * Must be called before any other pn_log_* functions (e.g. from main()).
 * Not required unless you use pn_log_enable.
 */
PN_EXTERN void pn_log_init(void);

/** Enable/disable logging.
 *
 * Note that if pn_log_init() was not called then this is not
 * thread safe. If called concurrently with calls to other pn_log functions
 * there is a chance that the envionment variable settings will win.
 */
PN_EXTERN void pn_log_enable(bool enabled);

/** Return true if logging is enabled. */
PN_EXTERN bool pn_log_enabled(void);

/** Log a printf style message */
#define pn_logf(...)                            \
    do {                                        \
        if (pn_log_enabled())                   \
            pn_logf_impl(__VA_ARGS__);          \
    } while(0)

/** va_list version of pn_logf */
#define pn_vlogf(fmt, ap)                       \
    do {                                        \
        if (pn_log_enabled())                   \
            pn_vlogf_impl(fmt, ap);             \
    } while(0)

/** Set the logger. By default a logger that prints to stderr is installed.
 *@param logger Can be 0 to disable logging regardless of pn_log_enable.
 */
PN_EXTERN void pn_log_logger(pn_logger_t logger);

/**@internal*/
PN_EXTERN void pn_logf_impl(const char* fmt, ...);
/**@internal*/
PN_EXTERN void pn_vlogf_impl(const char *fmt, va_list ap);

#endif
