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
 * Control log messages that are not associated with a transport.
 * See pn_transport_trace for transport-related logging.
 */

#include <proton/import_export.h>
#include <proton/type_compat.h>

/** Callback for customized logging. */
typedef void (*pn_logger_t)(const char *message);

/** Enable/disable global logging.
 *
 * By default, logging is enabled by envionment variable PN_TRACE_LOG.
 * Calling this function overrides the environment setting.
 */
PN_EXTERN void pn_log_enable(bool enabled);

/** Set the logger.
 *
 * By default a logger that prints to stderr is installed.
 *  
 * @param logger is called with each log messsage if logging is enabled.
 * Passing 0 disables logging regardless of pn_log_enable() or environment settings.
 */
PN_EXTERN void pn_log_logger(pn_logger_t logger);

#endif
